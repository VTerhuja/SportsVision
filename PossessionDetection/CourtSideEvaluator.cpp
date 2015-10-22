#include <vector>
#include <algorithm>
#include "CourtSideEvaluator.h"
using namespace std;

// Continuity Interval
int	contInterval = 7;
// Allowable Offset
int allowAbleOffset = contInterval * 15;

/* Difference Normalization:
   -1 : No normalization
	0 : Normalize between 0 and 1
	1 : Normalize between -1 and 1
	2 : Normalize negative values between -1 and 0; positive values between 0 and 1
*/
int iNormalize = 3;

/* 3 signals */
int		i3Signals = 1;
int		gameIdx = 1;
string	fName;
string  fGTName;
string	fSegName;
string  fGTSegName;
string	fSegmentFileName;
string	outputFile;

/* Directory Path */
string  sThreeSignalsDirPath	= "E:\\Research\\ResultData\\3Signals\\";
string	sAutoRefSignalDirPath	= "E:\\Research\\ResultData\\Auto_Reference_Result\\";

/* Averaging Window Size (Symmetric size on both forward and backward) */
int		avgWindowSize = 5;

/*************************** Constants *************************/
const char *GAME_ARG		= "-g";
const char *NORM_ARG		= "-n";
const char *THREE_SIG_ARG	= "-t";
const char *HELP			= "/?";

bool bEvaluate = false;
bool bUseMRefAsGT = true;	// Use Manual Reference result as Ground Truth

/* Debug */
bool bDebug			= true;
bool bAvgSDD		= false;
bool bWriteAvgSDD	= false;
bool bWrite2File	= false;
ofstream debugFile;

/* Threshold: Percentage of the maximum magnitude
/* Use two thresholds */
/* 1. Forward Threshold */
/* 2. Backward Threshold */
bool bUse2Threshold = true;


/* Extract Segment information */
VideoSegment ExtractSegmentData(string rowString)
{
	VideoSegment vs;
	int startIndex = 0;
	int nextIndex = rowString.find_first_of(SPACE_SYMBOL, 0);
	string value = rowString.substr(0, nextIndex);
	vs.startFrame = atol(value.c_str());
	startIndex = nextIndex+1;
	nextIndex = rowString.find_first_of(SPACE_SYMBOL, startIndex);
	value = rowString.substr(startIndex, nextIndex-startIndex);
	vs.endFrame = atol(value.c_str());
	startIndex = nextIndex+1;
	value = rowString.substr(startIndex, rowString.length()-startIndex);
	if(value == "L")
		vs.type = SegmentType::LEFT_SEG;
	else
		vs.type = SegmentType::RIGHT_SEG;

	return vs;	
}

/* Extract Segments */
vector<VideoSegment> ExtractSegments(string fileName)
{
	vector<VideoSegment> vs;
	VideoSegment v;
	//count = GetLinesCount(fileName);

	ifstream fs(fileName);
	if(fs.is_open())
	{
		string rowString;
		while(!fs.eof())
		{
			getline(fs, rowString);
			if(!rowString.empty())
				vs.push_back(ExtractSegmentData(rowString));
		}
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
	}
	fs.close();
	return vs;
}

/****************************************************
/*
Summary: Print Command Line Help
*/
/***************************************************/
void help()
{
    cerr << "CourtSideEvaluator [-g game_id] "
         << "[-n normalization_type] [-t Three Signals]" << endl
         << " -g game id " << endl
         << " -n norm type " << endl
         << " -t 3 Signals " << endl;
}

/****************************************************
/*
Summary: Read arguments
*/
/***************************************************/
int check_arg(int i, int max)
{
    if (i == max) {
        help();
        exit(1);
    }
    return i + 1;
}

/****************************************************
/*
Summary: Read Command Line arguments
*/
/***************************************************/
void ReadCommandLine(int argc, char** argv)
{
	int i = 1;
    while (i < argc)
	{
        if (!strcmp(argv[i], GAME_ARG))
		{
			i = check_arg(i, argc);
            gameIdx = atoi(argv[i]);
		}
        else if (!strcmp(argv[i], NORM_ARG))
		{
			i = check_arg(i, argc);
			iNormalize = atoi(argv[i]);
		}
		else if (!strcmp(argv[i], THREE_SIG_ARG)) 
		{
			i = check_arg(i, argc);
			i3Signals = atoi(argv[i]);

        }else if (!strcmp(argv[i], HELP))
		{
            help();
            exit(1);
        }
		else
		{
			help();
            exit(1);
		}
        i++;
    }
	char buff[100];
	if(bUseMRefAsGT)
	{
		sprintf(buff, "Game#%d_Segments.txt", gameIdx);
		fSegName = buff;

		sprintf(buff, "Game#%d_Sides_Segment_Norm.txt", gameIdx);
		fSegmentFileName = buff;

		sprintf(buff, "Game#%d_GT_Segments.txt", gameIdx);
		fGTSegName = buff;

		sprintf(buff, "Game#%d_LRM_i15_s3.00.txt", gameIdx);
		fGTName = buff;

		sprintf(buff, "Game#%d_Auto_i15_s3.00.txt", gameIdx);
		fName = buff;

		sprintf(buff, "Game#%d_3Signals_Normalized.txt", gameIdx);
		outputFile = buff;
	}
	else
	{
		sprintf(buff, "Game#%d_LRM_i15_s3.00.txt", gameIdx);
		fName = buff;
		sprintf(buff, "Game#%d_3Signals_Norm_Output.txt", gameIdx);
		outputFile = buff;
	}
}

/****************************************************
/*
Summary: Check Continuity of Side Detection
Parameters:
	1. pSDDVals	: Side Detector Difference - Matching difference value array
	2. index	: Index of array
	3. step		: Step
	4. previous : Previous state
*/
/***************************************************/
bool CheckContinuity(SDDiff* pSDDVals, int index, int &step, bool previous)
{
	float threshold = 2.0;
	bool bCurPositive = !previous;
	int conCount = 0;

	float avg = 0;

	for(int i = 1; i <= contInterval; i++)
	{
		avg += pSDDVals[index + i].value;
	}

	avg /= contInterval;

	if(avg >= threshold)
		bCurPositive = true;
	else
		bCurPositive = false;

	//for(int i = 1; i <= interval; i++)
	//{
	//	if(pSDDVals[index + i].value >= 0)
	//		bCurPositive = true;
	//	else
	//		bCurPositive = false;

	//	if(bCurPositive != previous)
	//		conCount++;
	//}

	//if(conCount >= interval - 1)
	//	return true;

	step = contInterval;
	return (bCurPositive != previous);
}

/****************************************************
/*
Summary: Check if the side is Right side
*/
/***************************************************/
bool IsRight(float value, float transFraction, float transOffset)
{
	if(value <= transFraction - transOffset)
		return true;
	return false;
}

bool IsRight(float value)
{
	if(value < 0)
		return true;
	return false;
}

/****************************************************
/*
Summary: Check if the side is Left side
*/
/***************************************************/
bool IsLeft(float value, float transFraction, float transOffset)
{
	if(value >= transFraction + transOffset)
		return true;
	return false;
}

bool IsLeft(float value)
{
	if(value >= 0)
		return true;
	return false;
}

/*************************************************************************
/*
Summary: Check Continuity using Normalized Side Detection Difference Value
Parameters:
*/
/************************************************************************/
bool CheckNormContinuity(SDDiff* pSDDVals, int index, int &step, int lastSide, float transFraction, float transOffset)
{
	int contStride = 5;
	bool bIsRight = lastSide == 1 ? true : false;
	for(int i = 0; i < contStride; i++)
	{
		step = i;
		if(bIsRight)
		{
			if(IsRight(pSDDVals[index + i].value, transFraction, transOffset))
				return false;
		}
		else
		{
			if(IsLeft(pSDDVals[index + i].value, transFraction, transOffset))
				return false;
		}
	}
	return true;
}

/****************************************************
/*
Summary: Check if the side is continuous
*/
/***************************************************/
bool CheckContinuous(SDDiff *pSDDVals, int index, float transFraction, float transOffset)
{
	int contStride = 3;
	for(int i = 0; i < contStride; i++)
	{
		if(!IsRight(pSDDVals[index + i].value, transFraction, transOffset) && !IsLeft(pSDDVals[index + i].value, transFraction, transOffset))
			return false;
	}
	return true;
}

/****************************************************
/*
Summary: Find the Zero Crossing of the signal
Parameters:
	1. pSDDVals	: Array of Side Detector Difference
	2. sddCount	: Number of elements in the array
	3. numberOfTransitions : Number of detected transitions
*/
/***************************************************/
vector<PossessionMarker> FindZeroCrossings(SDDiff* pSDDVals, int sddCount, int &numberOfTransitions)
{
	bool bSaveZC = true;
	ofstream eevalFile;
	if(bSaveZC)
	{
		eevalFile.open("Game#1_Zero_Crossings.txt");
	}

	vector<PossessionMarker> tMarker;
	PossessionMarker temp;
	bool bCurrentPositive = true;
	bool bPrevPositive = false;
	int index = 0;
	while(index < sddCount)
	{
		if(pSDDVals[index].value < 0)
			bCurrentPositive = false;
		else
			bCurrentPositive = true;

		if(index == 0)
		{
			bPrevPositive = bCurrentPositive;
			index++;
			continue;
		}

		// Zero-crossing
		if(bCurrentPositive != bPrevPositive)
		{
			int step = 0;
			if(CheckContinuity(pSDDVals, index, step, bPrevPositive))
			{
				temp.frameNumber = bCurrentPositive ? pSDDVals[index].frameNumber: pSDDVals[index-1].frameNumber;
				temp.status = FrameStatus::TRANSITION;

				if(bSaveZC)		eevalFile<<-temp.frameNumber<<"\n";
				
				tMarker.push_back(temp);
				bPrevPositive = bCurrentPositive;
				index = index + step;
				continue;
			}
			else
			{
				index++;
				continue;
			}
		}
		bPrevPositive = bCurrentPositive;
		index++;
	}
	numberOfTransitions = (int)tMarker.size();
	if(bSaveZC)	eevalFile.close();
	return tMarker;
}

/****************************************************
/*
Summary: Find court side segments
Parameters:
	1. pSDDVals	: Side detector difference value array
	2. sddCount	: Size of array
	3. numberOfTransition: Number of detected transition
	4. min: Minimum SDD value
	5. max: Maximum SDD value
*/
/***************************************************/
vector<PossessionMarker> FindSidesSegment(SDDiff* pSDDVals, int sddCount, int &numberOfTransitions, int min, int max)
{
	cout<<"\nFinding Segments... ";
	ofstream sideSegmentFile;
	bool bSaveSS = false;
	if(bSaveSS)		sideSegmentFile.open(fSegmentFileName);
	
	vector<PossessionMarker> tMarker;
	PossessionMarker temp;
	float transFraction = 0;
	float transOffset = 0;

	if(iNormalize == 2)
	{
		transFraction = 0;
		// Percentage = 20%
		transOffset = 0.4;
	}
	else
	{
		transFraction = (float)(-min)/(max + (-min));
		transOffset = 0.045;
	}
	//cout<<"\nTransition fraction: "<<transFraction;

	int index = 0;
	bool bStarted = false;
	int lastSide = -1; // 0 = LEFT, 1 = RIGHT
	while(index < sddCount)
	{
		bool bPrint = false;
		if(IsRight(pSDDVals[index].value, transFraction, transOffset))
		{
			if(!CheckContinuous(pSDDVals, index, transFraction, transOffset))
			{
				index++;
				continue;
			}

			// Right court
			if(!bStarted)
			{
				temp.frameNumber = pSDDVals[index].frameNumber;
				temp.status = FrameStatus::RIGHT_START;
				bStarted = true;
				lastSide = 1;
				tMarker.push_back(temp);
				bPrint = true;
			}
		}
		else if(IsLeft(pSDDVals[index].value, transFraction, transOffset))
		{
			if(!CheckContinuous(pSDDVals, index, transFraction, transOffset))
			{
				index++;
				continue;
			}

			// Left court
			if(!bStarted)
			{
				temp.frameNumber = pSDDVals[index].frameNumber;
				temp.status = FrameStatus::LEFT_START;
				bStarted = true;
				lastSide = 0;
				tMarker.push_back(temp);
				bPrint = true;
			}
		}
		else
		{
			// Mid court
			int step = 0;
			if(bStarted && CheckNormContinuity(pSDDVals, index, step, lastSide, transFraction, transOffset))
			{
				temp.frameNumber = pSDDVals[index-1].frameNumber;
				if(lastSide == 1)
				{
					temp.status = FrameStatus::RIGHT_END;
				}
				else if(lastSide == 0)
				{
					temp.status = FrameStatus::LEFT_END;
				}
				lastSide = -1;
				bStarted = false;
				tMarker.push_back(temp);
				bPrint = true;
			}
			index += step;
		}

		if(bPrint && bSaveSS)
		{
			sideSegmentFile<<temp.frameNumber<<" "<<temp.status<<"\n";
		}
		index++;
	}
	cout<<"Done!"<<endl;
	sideSegmentFile.close();
	return tMarker;
}

/****************************************************
/*
Summary: Evaluate SDD with Ground Truth
Parameters:
	1. trans : Detected transitions
	2. transitionCount : Video Index (multiple clips game)
	3. pMarkerGT : Ground truth array of transition points
	4. markerCount : Number of ground truth array element
*/
/***************************************************/
void EvaluateSDD(vector<PossessionMarker> trans, int transitionCount, PossessionMarker* pMarkerGT, int markerCount)
{
	ofstream evalFile;
	evalFile.open("Evaluation Result.txt", ios_base::app);

	int gtIndex = 0;
	int missedCount = 0;
	int correctCount = 0;
	int incorrectCount = 0;
	vector<long>missed;
	vector<long>incorrect;

	for(int i = 0; i < transitionCount; i++)
	{
		bool bStartSearch = false;
		long pFrame = trans[i].frameNumber;

		while(gtIndex < markerCount)
		{
			long gFrame = pMarkerGT[gtIndex].frameNumber;
			if(abs(pFrame - gFrame) <= allowAbleOffset)
			{
				// Match found
				if(bStartSearch)
				{
					missedCount++;
					missed.push_back(pMarkerGT[gtIndex-1].frameNumber);
				}
				correctCount++;
				gtIndex++;
				break;
			}
			else
			{
				if(pFrame < gFrame)
				{
					incorrectCount++;
					incorrect.push_back(pFrame);
					if(bStartSearch)	gtIndex--;
					break;
				}
				else
				{
					if(!bStartSearch)	
					{
						bStartSearch = true;
					}
					else				
					{
						missedCount++;
						missed.push_back(gFrame);
					}
					gtIndex++;
				}
			}
		}

		if(gtIndex == markerCount)
		{
			incorrectCount++;
			incorrect.push_back(pFrame);
		}
	}

	while(gtIndex < markerCount)
	{
		missedCount++;
		missed.push_back(pMarkerGT[gtIndex].frameNumber);
		gtIndex++;
	}

	// Print results
	bool bPrint = false;
	if(bPrint)
	{
		evalFile<<"Game #1:";
		evalFile<<"\nTotal transition points (Ground Truth): "<<markerCount;
		evalFile<<"\nTotal transition points (Side Detector Difference): "<<transitionCount;
		evalFile<<"\nTrue Positive (Correct): "<<correctCount;
		evalFile<<"\nFalse Negative (Missed): "<<missedCount;
		evalFile<<"\nFalse Positive (Incorrect): "<<incorrectCount;
		evalFile<<"\nCorrect transitions percentage (Precision): "<<(float)correctCount/transitionCount;
		evalFile<<"\nRecall: "<<(float)correctCount/markerCount;
		evalFile<<"\nMissed transitions percentage: "<<(float)missedCount/transitionCount<<endl<<endl;
	}
	evalFile.close();
}

/* Extension 1: Smooth over a window */
void AverageOverWindow(SDDiff* pSDDVals, int count, int &minVal, int &maxVal)
{
	int binPositiveCount[10] = { };
	int binNegativeCount[10] = { };
	int binSize = 10;
	int binIndex = 0;
	bool bBin =true;

	ofstream ssdFile;
	if(bAvgSDD)	ssdFile.open("AverageSDD.txt");

	for(int i = 0; i < count; i++)
	{
		float val = 0;
		int wCount = 0;
		int idx = i + avgWindowSize;
		while(wCount < (2*avgWindowSize + 1))
		{
			if(idx < 0)
				val += pSDDVals[0].value;
			else if(idx > count)
				val += pSDDVals[count].value;
			else
				val += pSDDVals[idx].value;
			wCount++;
			idx--;
		}
		pSDDVals[i].value = val/(2*avgWindowSize + 1);
		
		if(bBin)
		{
			if(pSDDVals[i].value >= 0)
			{
				// Positive Values
				binIndex = (int)pSDDVals[i].value/binSize;
				if(binIndex >= binSize)
					binPositiveCount[binSize-1] += 1;
				else
					binPositiveCount[binIndex] += 1;
			}
			else 
			{
				// Negative Values
				binIndex = -(int)pSDDVals[i].value/binSize;
				if(binIndex >= binSize)
					binNegativeCount[binSize-1] += 1;
				else
					binNegativeCount[binIndex] += 1;
			}
		}

		if(bAvgSDD)
		{
			ssdFile<<pSDDVals[i].frameNumber<<" "<<pSDDVals[i].value<<"\n";
		}
	}

	// From the end, choose the bin with bincount difference > 100
	for(int i = binSize - 1; i > 0; i--)
	{
		if(binPositiveCount[i-1] - binPositiveCount[i]> 90)
		{
			maxVal = binSize * (i);
			break;
		}
	}

	for(int i = binSize - 1; i > 0; i--)
	{
		if(binNegativeCount[i-1] - binNegativeCount[i]> 90)
		{
			minVal = -binSize * (i);
			break;
		}
	}

	if(bAvgSDD)	ssdFile.close();
}

/* Extension 2: Smooth over a window */
void AverageOverWindowEx(SDDiff* pSDDVals, int count)
{
	ofstream ssdFile;
	if(bWriteAvgSDD)	ssdFile.open("AverageSDD.txt");

	for(int i = 0; i < count; i++)
	{
		float val = 0;
		int wCount = 0;
		int idx = i + avgWindowSize;
		while(wCount < (2*avgWindowSize + 1))
		{
			if(idx < 0)
				val += pSDDVals[0].value;
			else if(idx > count)
				val += pSDDVals[count].value;
			else
				val += pSDDVals[idx].value;
			wCount++;
			idx--;
		}
		pSDDVals[i].value = val/(2*avgWindowSize + 1);
		
		if(bWriteAvgSDD)
		{
			ssdFile<<pSDDVals[i].frameNumber<<" "<<pSDDVals[i].value<<"\n";
		}
	}

	if(bWriteAvgSDD)	ssdFile.close();
}

/****************************************************
/*
Summary: Normalize SDD value according to Normalize flag
Parameters:
	1. trans : Detected transitions
	2. transitionCount : Video Index (multiple clips game)
	3. pMarkerGT : Ground truth array of transition points
	4. markerCount : Number of ground truth array element
*/
/***************************************************/
void NormalizeSDD(SDDiff* SDDVals, int dataCount, int iNorm, int min, int max)
{
	// Normalization Range
	int lowerBound = 0;
	int upperBound = 0;

	if(iNorm == 0)
	{
		lowerBound = 0;
		upperBound = 1;
	}
	else if(iNorm == 1 || iNorm == 2)
	{
		lowerBound = -1;
		upperBound = 1;
	}

	ofstream normFile;
	if(bWrite2File)		normFile.open(outputFile);

	if(iNormalize == 0 || iNormalize == 1)
	{
		for(int i = 0; i < dataCount; i++)
		{
			SDDVals[i].value = lowerBound + (SDDVals[i].value - min)*(upperBound - lowerBound)/(max - min);
			if(bWrite2File)
			{
				normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
			}
		}
	}
	else if(iNormalize == 2)
	{
		for(int i = 0; i < dataCount; i++)
		{
			SDDVals[i].frameNumber = SDDVals[i].frameNumber;
			if(SDDVals[i].value <= 0)
			{
				SDDVals[i].value = lowerBound + (SDDVals[i].value - min)*(-lowerBound)/(-min);
			}
			else
			{
				SDDVals[i].value = (SDDVals[i].value)*(upperBound)/(max);
			}
			
			if(bWrite2File)
			{
				normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
			}
		}
	}

	if(bWrite2File)		normFile.close();
}

void NormalizeSDD(SDDiff* SDDVals, int dataCount, int iNorm, int min, int max, bool bNew)
{
	// Normalization Range
	int lowerBound = 0;
	int upperBound = 0;

	if(iNorm == 0)
	{
		lowerBound = 0;
		upperBound = 1;
	}
	else if(iNorm == 1 || iNorm == 2)
	{
		lowerBound = -1;
		upperBound = 1;
	}

	ofstream normFile;
	if(bWrite2File)		normFile.open(outputFile);

	if(iNormalize == 0 || iNormalize == 1)
	{
		for(int i = 0; i < dataCount; i++)
		{
			SDDVals[i].value = lowerBound + (SDDVals[i].value - min)*(upperBound - lowerBound)/(max - min);
			if(bWrite2File)
			{
				normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
			}
		}
	}
	else if(iNormalize == 2)
	{
		for(int i = 0; i < dataCount; i++)
		{
			//SDDVals[i].frameNumber = SDDVals[i].frameNumber;
			if(SDDVals[i].value < 0)
			{
				if(SDDVals[i].value < min)
					SDDVals[i].value = lowerBound;
				else
					SDDVals[i].value = SDDVals[i].value/(-min);
			}
			else
			{
				if(SDDVals[i].value > max)
					SDDVals[i].value = upperBound;
				else
					SDDVals[i].value = SDDVals[i].value/(max);
			}
			
			if(bWrite2File)
			{
				normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
			}
		}
	}

	if(bWrite2File)		normFile.close();
}

/* Normalize 3 signals with the maximum */
SDDiff* Normalize3Signals(ThreeSignal* tSignals, int dataCount, int iNorm, int &min, int &max)
{
	bool bOldNorm = false;
	ofstream diffFile;
	if(bWrite2File)		
		diffFile.open("SDD.txt");

	cout<<"\nNormalizing Signal values... ";
	SDDiff* sddiff = new SDDiff[dataCount];
	SDDiff* midVals = new SDDiff[dataCount];
	int mmin = 0;
	int mmax = 0;
	int midVal = 0;
	for(int i = 0; i < dataCount; i++)
	{
		sddiff[i].frameNumber = midVals[i].frameNumber = tSignals[i].frameNumber;
		sddiff[i].value = tSignals[i].lMatches - tSignals[i].rMatches;
		midVals[i].value = tSignals[i].mMatches;

		if(bOldNorm)
		{
			midVal = tSignals[i].mMatches;
	
			if(sddiff[i].value > max)
				max = sddiff[i].value;

			if(sddiff[i].value < min)
				min = sddiff[i].value;
			
			if(midVal > mmax)
				mmax = midVal;
			if(midVal < mmin)
				mmin = midVal;
		}

		if(bWrite2File)		
			diffFile<<sddiff[i].frameNumber<<" "<<sddiff[i].value<<"\n";
	}

	// Smoothing signal over the window size
	AverageOverWindow(sddiff, dataCount, min, max);

	if(bOldNorm)
		NormalizeSDD(sddiff, dataCount, iNorm, min, max);
	else
		NormalizeSDD(sddiff, dataCount, iNorm, min, max, true);

	cout<<"\nDone!"<<endl;
	return sddiff;
}

/* Smooth over a window */
ThreeSignal* AverageSideSignalsOverWindow(ThreeSignal* tSignals, int count)
{
	bool bWrite2File = true;
	ofstream avgSFile;
	if(bWrite2File)		
		avgSFile.open("AveragedSidesMatches.txt");

	cout<<"\nAveraging Signal values... ";
	ThreeSignal* avgSignals = new ThreeSignal[count];
	for(int i = 0; i < count; i++)
	{
		float valL = 0;
		float valR = 0;
		int wCount = 0;
		int idx = i + avgWindowSize;
		while(wCount < (2*avgWindowSize + 1))
		{
			if(idx < 0)
			{
				valL += tSignals[0].lMatches;
				valR += tSignals[0].rMatches;
			}
			else if(idx > count)
			{
				valL += tSignals[count].lMatches;
				valR += tSignals[count].rMatches;
			}
			else
			{
				valL += tSignals[idx].lMatches;
				valR += tSignals[idx].rMatches;
			}
			wCount++;
			idx--;
		}
		avgSignals[i].lMatches = valL/(2*avgWindowSize + 1);
		avgSignals[i].rMatches = valR/(2*avgWindowSize + 1);
		avgSignals[i].frameNumber = tSignals[i].frameNumber;
		
		if(bWrite2File)
		{
			avgSFile<<avgSignals[i].frameNumber<<" "<<avgSignals[i].lMatches<<" "<<avgSignals[i].rMatches<<"\n";
		}
	}

	if(bWrite2File)	avgSFile.close();
	return avgSignals;
}

/* Compute Average of side detector difference */
SDDiff* AverageSideDetectDifference(ThreeSignal* tSignals, int dataCount)
{
	ofstream diffFile;
	if(bWrite2File)		
		diffFile.open("SDD.txt");

	cout<<"\nNormalizing Signal values... ";
	SDDiff* sddiff = new SDDiff[dataCount];
	SDDiff* midVals = new SDDiff[dataCount];
	int mmin = 0;
	int mmax = 0;
	int midVal = 0;
	for(int i = 0; i < dataCount; i++)
	{
		sddiff[i].frameNumber = midVals[i].frameNumber = tSignals[i].frameNumber;
		sddiff[i].value = tSignals[i].lMatches - tSignals[i].rMatches;
		midVals[i].value = tSignals[i].mMatches;

		if(bWrite2File)		
			diffFile<<sddiff[i].frameNumber<<" "<<sddiff[i].value<<"\n";
	}

	// Smoothing signal over the window size
	AverageOverWindowEx(sddiff, dataCount);
	
	cout<<"\nDone!"<<endl;
	return sddiff;
}

/* Check if side has changed */
bool IsSideChanged(float value, int lastSide)
{
	bool bRightSide = (lastSide == 0) ? true : false;

	if(IsRight(value) && !bRightSide)
		return true;
	else if(IsLeft(value) && bRightSide)
		return true;

	return false;
}

/* Normalize the side detector difference */
SDDiff* NormalizeDifference(ThreeSignal* tSignals, int dataCount)
{
	ofstream diffFile;
	if(bWrite2File)		
		diffFile.open("NormSDD.txt");

	cout<<"\nNormalizing Signal values... ";
	SDDiff* sddiff = new SDDiff[dataCount];
	int mmin = 0;
	int mmax = 0;
	int midVal = 0;
	for(int i = 0; i < dataCount; i++)
	{
		float sum = tSignals[i].lMatches + tSignals[i].rMatches;

		sddiff[i].frameNumber = tSignals[i].frameNumber;
		sddiff[i].value = (tSignals[i].lMatches - tSignals[i].rMatches)/sum * 100;

		if(bWrite2File)		
			diffFile<<sddiff[i].frameNumber<<" "<<sddiff[i].value<<"\n";
	}
	cout<<"Done!"<<endl;

	// Smoothing signal over the window size
	AverageOverWindowEx(sddiff, dataCount);

	return sddiff;
}

/* Normalize the side detector difference */
VideoSegment NormalizeSDDExtractSegment(SDDiff* SDDVals, int start, int end, float min, float max, bool bPositive)
{
	float threshold = 0.3;

	// Normalization Range
	int lowerBound = -1;
	int upperBound = 1;

	VideoSegment seg;

	ofstream normFile;
	if(bWrite2File)		normFile.open(outputFile, ios::app);

	/* flags */
	bool bStartFound = false;
	bool bEndFound = false;
	bool bFBSearch = true;

	float lastVal = 0;
	float curVal = 0;
	int index = 0;

	if(!bFBSearch)
	{
		index = start;
		while(index <= end)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value = lowerBound + (SDDVals[index].value - min)/(-min);
				if(curVal > -threshold)
				{
					if(lastVal < curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index-1].frameNumber;
							bEndFound = true;
						}
					}
					else
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
						}
					}
				}
				lastVal = curVal;
			}
			else
			{
				curVal = SDDVals[index].value = (SDDVals[index].value)/(max);
				if(curVal < threshold)
				{
					if(lastVal < curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
						}
					}
					else
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index-1].frameNumber;
							bEndFound = true;
						}
					}
				}
				lastVal = curVal;
			}
	
			if(bWrite2File)
			{
				normFile<<SDDVals[index].frameNumber<<" "<<SDDVals[index].value<<"\n";
			}
			index++;
		}
	}

	// Write Normalized values on file
	if(bWrite2File)
	{
		float val = 0;
		for(int i = start; i <= end; i++)
		{
			if(!bPositive)
			{
				val = lowerBound + (SDDVals[i].value - min)/(-min);
			}
			else
			{
				val = (SDDVals[i].value)/(max);
			}
			normFile<<SDDVals[i].frameNumber<<" "<<val<<"\n";
		}
	}

	if(bFBSearch)
	{
		index = start;
		/* Search Forward */
		while(index <= end)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value = lowerBound + (SDDVals[index].value - min)/(-min);
				if(curVal < -threshold)
				{
					if(lastVal > curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value = (SDDVals[index].value)/(max);
				if(curVal > threshold)
				{
					if(lastVal < curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index++;
		}

		/* Search Backward */
		index = end;
		while(index >= start)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value = lowerBound + (SDDVals[index].value - min)/(-min);
				if(curVal < -threshold)
				{
					if(lastVal > curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value = (SDDVals[index].value)/(max);
				if(curVal > threshold)
				{
					if(lastVal < curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index--;
		}
	}

	if(!bStartFound)
		seg.startFrame = SDDVals[start].frameNumber;
	if(!bEndFound)
		seg.endFrame = SDDVals[end].frameNumber;

	seg.type = bPositive ? SegmentType::LEFT_SEG : SegmentType::RIGHT_SEG;
	seg.length = seg.endFrame - seg.startFrame;

	if(bWrite2File)		normFile.close();
	return seg;
}

/* Normalize the side detector difference */
void LocalNormalizeSDD(SDDiff* SDDVals, int start, int end, float min, float max, bool bPositive)
{
	// Normalization Range
	int lowerBound = -1;
	int upperBound = 1;

	/* Flags */
	bool bWritePercentile = false;

	ofstream normFile;
	if(bWrite2File)		normFile.open(outputFile, ios::app);

	if(bWritePercentile)
	{
		/* Average top 90 percentile */
		int t90PCount = 0;
		int avg = 0;
		for(int i = start; i <= end; i++)
		{
			if(bPositive)
			{
				if(SDDVals[i].value >= max * 0.9)
				{
					t90PCount++;
					avg += SDDVals[i].value;
				}
			}
			else
			{
				if(SDDVals[i].value <= min * 0.9)
				{
					t90PCount++;
					avg += SDDVals[i].value;
				}
			}
		}
		avg /= (t90PCount);
		ofstream percentileFile;
		percentileFile.open("Percentile.txt", ios::app);
		percentileFile<<avg<<"\n";
	}

	for(int i = start; i <= end; i++)
	{
		if(!bPositive)
		{
			SDDVals[i].value = lowerBound + (SDDVals[i].value - min)/(-min);
		}
		else
		{
			SDDVals[i].value = (SDDVals[i].value)/(max);
		}
		if(bWrite2File)	normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
	}
	
	if(bWrite2File)		normFile.close();
}

/* Normalize the side detector difference */
void LocalNormalizeSDD(SDDiff* SDDVals, SegmentCandidate candidate)
{
	int start		= candidate.startIndex;
	int end			= candidate.endIndex;
	bool bPositive	= candidate.bIsPositive;
	float max		= candidate.maxMagnitude;
	float min		= candidate.minMagnitude;

	// Normalization Range
	int lowerBound = -1;
	int upperBound = 1;

	ofstream normFile;
	if(bWrite2File)		normFile.open(outputFile, ios::app);

	for(int i = start; i <= end; i++)
	{
		if(!bPositive)
		{
			SDDVals[i].value = lowerBound + (SDDVals[i].value - min)/(-min);
		}
		else
		{
			SDDVals[i].value = (SDDVals[i].value)/(max);
		}
		if(bWrite2File)	normFile<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value<<"\n";
	}
	
	if(bWrite2File)		normFile.close();
}

/** Compute the Median Magnitude **/
double ComputeMedianMagnitude(vector<double> magnitudeVector)
{
	int arraySize = (int)magnitudeVector.size();
	double median = 0.;

	// Sort
	std::sort(magnitudeVector.begin(), magnitudeVector.end());

	if(arraySize%2 == 0)
	{
		median = (magnitudeVector[arraySize/2] + magnitudeVector[arraySize/2 - 1])/2;
	}
	else
	{
		median = magnitudeVector[arraySize/2];
	}

	bool bShowValues = false;
	if(bShowValues)
	{
		cout<<"\nArray size: "<<arraySize;
		cout<<"\nMagnitude:\n";
		for(int i=0; i<arraySize; i++)
		{
			cout<<magnitudeVector[i]<<endl;
		}
	}
	return median;
}

/** Validate whether the segment is a valid segment **/
bool IsValidSegment(SDDiff* pSDDVals, int start, int end, ThreeSignal* tSignals, float minVal, float maxVal, bool bIsPositive)
{
	/* Check whether average magnitude is above certain threshold */
	bool bCheckAvg = false;
	bool bCheckGradient1 = false;	/* Simple gradient: difference between consecutive (#L-#R) */
	bool bCheckGradient2 = false;	/* Gradient: difference between consecutive normalized difference */
	bool bCheckAvgNormDiff = true;
	bool bCheckMedianMag = false;
	bool bCheckMedianNormMag = false;

	if(bCheckMedianNormMag)
	{
		float medNormDiff = 0.0;
		vector<double> normMagArray;
		for(int i = start; i <= end; i++)
		{
			float normDiff = abs((float)(tSignals[i].lMatches - tSignals[i].rMatches)/(tSignals[i].lMatches + tSignals[i].rMatches) * 100);
			normMagArray.push_back(normDiff);
		}
		medNormDiff = ComputeMedianMagnitude(normMagArray);
		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<medNormDiff<<"\n";
		return true;

		if(medNormDiff < 0.35)
		{
			if(bDebug)	debugFile<<" Invalid"<<"\n";
			//return true;
			return false;
		}
		else
		{
			if(bDebug)	debugFile<<" Valid"<<"\n";
			return true;
		}
	}
	else if(bCheckMedianMag)
	{
		float medMag = 0.0;
		vector<double> magArray;
		for(int i = start; i <= end; i++)
		{
			magArray.push_back(abs(pSDDVals[i].value));
		}

		medMag = ComputeMedianMagnitude(magArray);
		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<medMag<<"\n";
		return true;
		if(medMag < 15.5)
		{
			if(bDebug)	debugFile<<" Invalid"<<"\n";
			return false;
		}
		else
		{
			if(bDebug)	debugFile<<" Valid"<<"\n";
			return true;
		}
	}
	else if(bCheckAvg)
	{
		float avg = 0.0;
		float val = 0.0;
		float max = 0.0;
		int count = 0;
		for(int i = start; i <= end; i++)
		{
			count++;
			val = abs(pSDDVals[i].value);
			avg += val;
			if(val > max)
				max = val;
		}
		//avg /= (end - start + 1);

		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<max<<" "<<avg<<" "<<avg/max<<" "<<avg/count<<"\n";
		//if(max < 25.0)
		//{
		//	if(bDebug)	debugFile<<" Invalid"<<"\n";
		//	return true;
		//}
		//else
		//{
		//	if(bDebug)	debugFile<<" Valid"<<"\n";
		//	return true;
		//}
		return true;
	}
	else if(bCheckGradient1)
	{
		float normFactor = bIsPositive ? maxVal : abs(minVal);
		float last = abs(pSDDVals[start].value);
		float avgGradient = 0.0;
		int count = 0;
		float max = 0.0;
		for(int i = start+1; i < end; i++)
		{
			float val = abs(pSDDVals[i].value);
			float gradient = (abs(val-last))/normFactor;
			last = val;

			if(gradient > max)
				max = gradient;
			avgGradient += gradient;
			count++;
		}
		if(count != 0)
			avgGradient /= count;
		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<max/avgGradient*10<<"\n"; //<<" "<<avgGradient<<"\n";
	}
	else if(bCheckGradient2)
	{
		float avgNormDiff = 0.0;
		float maxNormDiff = 0.0;
		float maxDiffGrad = 0.0;
		float avgDiffGrad = 0.0;

		int count = 0;
		float lastNormDiff = abs((float)(tSignals[start].lMatches - tSignals[start].rMatches)/(tSignals[start].lMatches + tSignals[start].rMatches)) * 100;
		for(int i = start+1; i <= end; i++)
		{
			float normDiff = abs((float)(tSignals[i].lMatches - tSignals[i].rMatches)/(tSignals[i].lMatches + tSignals[i].rMatches)) * 100;
			float sum = normDiff + lastNormDiff;
			float grad = 0.0;
			
			if(sum != 0)
				grad = abs((normDiff - lastNormDiff)/sum);

			if(maxNormDiff < grad)
				maxNormDiff = grad;

			avgNormDiff += grad;
			lastNormDiff = normDiff;
			count++;
		}
		if(count != 0)
			avgNormDiff /= count;
		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<avgNormDiff<<" "<<maxNormDiff<<"\n";
	}
	else if(bCheckAvgNormDiff)
	{
		float avgNormDiff = 0.0;
		float maxNormDiff = 0.0;
		int count = 0;
		for(int i = start; i <= end; i++)
		{
			float normDiff = abs((float)(tSignals[i].lMatches - tSignals[i].rMatches)/(tSignals[i].lMatches + tSignals[i].rMatches));
			avgNormDiff += normDiff;
			count++;
			if(maxNormDiff < normDiff)
				maxNormDiff = normDiff;
		}
		//avgNormDiff /= count;

		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<avgNormDiff<<" "<<maxNormDiff<<" "<<avgNormDiff/maxNormDiff<<"\n";
		return true;
		
		if(maxNormDiff < 0.35)
		{
			if(bDebug)	debugFile<<" Invalid"<<"\n";
			//return true;
			return false;
		}
		else
		{
			if(bDebug)	debugFile<<" Valid"<<"\n";
			return true;
		}
	}
	else
	{
		float min = pSDDVals[start].value;
		float max = pSDDVals[start].value;
		for(int i = start; i <= end; i++)
		{
			if(min > pSDDVals[i].value)
				min = pSDDVals[i].value;

			if(max < pSDDVals[i].value)
				max = pSDDVals[i].value;
		}
		float normDiff = (max-min)/(max+min);
		if(bDebug)	debugFile<<pSDDVals[start].frameNumber<<" "<<pSDDVals[end].frameNumber<<" "<<abs(normDiff)<<"\n";
	}
}

/** Get Segment using single threshold **/
VideoSegment GetSegment(SDDiff* SDDVals, int start, int end, bool bPositive, float threshold)
{
	VideoSegment seg;

	/* flags */
	bool bStartFound = false;
	bool bEndFound = false;
	bool bFBSearch = true;

	float lastVal = 0;
	float curVal = 0;
	int index = 0;

	if(bFBSearch)
	{
		index = start;

		/* Search Forward */
		while(index <= end)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value;
				if(curVal < -threshold)
				{
					if(lastVal > curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value;
				if(curVal > threshold)
				{
					if(lastVal < curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index++;
		}

		/* Search Backward */
		index = end;
		while(index >= start)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value;
				if(curVal < -threshold)
				{
					if(lastVal > curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value;
				if(curVal > threshold)
				{
					if(lastVal < curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index--;
		}
	}

	if(!bStartFound)
		seg.startFrame = SDDVals[start].frameNumber;
	if(!bEndFound)
		seg.endFrame = SDDVals[end].frameNumber;

	seg.type = bPositive ? SegmentType::LEFT_SEG : SegmentType::RIGHT_SEG;
	seg.length = seg.endFrame - seg.startFrame;

	return seg;
}

/** Get Segments using Forward and Backward Thresholds **/
VideoSegment GetSegment(SDDiff* SDDVals, int start, int end, bool bPositive, float fThreshold, float bThreshold)
{
	VideoSegment seg;

	/* flags */
	bool bStartFound = false;
	bool bEndFound = false;
	bool bFBSearch = true;

	float lastVal = 0;
	float curVal = 0;
	int index = 0;

	if(bFBSearch)
	{
		index = start;

		/* Search Forward */
		while(index <= end)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value;
				if(curVal < -fThreshold)
				{
					if(lastVal > curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value;
				if(curVal > fThreshold)
				{
					if(lastVal < curVal)
					{
						if(!bStartFound)
						{
							seg.startFrame = SDDVals[index].frameNumber;
							bStartFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index++;
		}

		/* Search Backward */
		index = end;
		while(index >= start)
		{
			if(!bPositive)
			{
				curVal = SDDVals[index].value;
				if(curVal < -bThreshold)
				{
					if(lastVal > curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			else
			{
				curVal = SDDVals[index].value;
				if(curVal > bThreshold)
				{
					if(lastVal < curVal)
					{
						if(!bEndFound)
						{
							seg.endFrame = SDDVals[index].frameNumber;
							bEndFound = true;
							break;
						}
					}
				}
			}
			lastVal = curVal;
			index--;
		}
	}

	if(!bStartFound)
		seg.startFrame = SDDVals[start].frameNumber;
	if(!bEndFound)
		seg.endFrame = SDDVals[end].frameNumber;

	seg.type = bPositive ? SegmentType::LEFT_SEG : SegmentType::RIGHT_SEG;
	seg.length = seg.endFrame - seg.startFrame;

	return seg;
}

/* Check validity of new candidate */
bool CheckNewStartCandidateValid(vector<SegmentCandidate> &candidates, int i, float &avgMax, float &avgMin, int start)
{
	int n = candidates.size();
	bool bCandidateType = candidates[i].bIsPositive;
	float candidateTypeAvg = bCandidateType ? avgMax : abs(avgMin);
	float candidateVal = bCandidateType ? candidates[i].maxMagnitude : abs(candidates[i].minMagnitude); 

	if(candidateVal < candidateTypeAvg)
	{
		/* Check if it continues to be lesser till the end */
		int sIdx = i+1;
		while(sIdx < n)
		{
			float currentCandVal = bCandidateType ? candidates[sIdx].maxMagnitude : abs(candidates[sIdx].minMagnitude); 
			if(candidates[sIdx].bIsPositive == bCandidateType)
			{
				if(currentCandVal > candidateTypeAvg)
				{
						break;
				}
			}
			sIdx++;
		}

		/* If not found */
		if(sIdx == n)
		{
			/* Camera shot angle change found */
			/* Calculate new average from i to end */
			float newAvg = 0.0;
			int counter = 0;
			for(int j=i; j<n; j++)
			{
				float currentCandVal = bCandidateType ? candidates[j].maxMagnitude : abs(candidates[j].minMagnitude); 
				if(candidates[j].bIsPositive == bCandidateType)
				{
					newAvg += currentCandVal;
					counter++;
				}
			}
			newAvg /= counter;

			if(bCandidateType)
				avgMax = newAvg;
			else
				avgMin = newAvg;

			return true;
		}
		else
		{
			if(candidateVal < .3 * candidateTypeAvg)
			{
				cout<<"Invalid";
				if(bDebug)	debugFile<<"Invalid\n";
				candidates[i].bValid = false;
			}
			else
			{
				if(bDebug)	cout<<"In Question: "<<candidateVal;

				/* Take average of 2 valid segments before and 2 after of the segment in question */
				int bIdx = i-1;
				int fIdx = i+1;
				int bCount = 0;
				int fCount = 0;
				int samples = 2;
				float sampleAvg = 0.0;

				/* Valid segments before */
				while(bIdx >= start)
				{
					if(candidates[bIdx].bValid && candidates[bIdx].bIsPositive == bCandidateType)
					{
						bCount++;
						sampleAvg += bCandidateType ? candidates[bIdx].maxMagnitude : candidates[bIdx].minMagnitude;
					}
					bIdx--;

					if(bCount == samples)
						break;
				}

				/* Segments after */
				while(fIdx < n)
				{
					if(candidates[fIdx].bIsPositive == bCandidateType)
					{
						fCount++;
						sampleAvg += bCandidateType ? candidates[fIdx].maxMagnitude : candidates[fIdx].minMagnitude;
					}
					fIdx++;

					if(fCount == samples)
						break;
				}

				sampleAvg = abs(sampleAvg)/(fCount + bCount);
				if(bDebug)	cout<<" New Avg: "<<sampleAvg;

				if(bDebug)	cout<<" P: "<<candidateVal/sampleAvg*100;
				if(candidateVal > .3 * sampleAvg)
				{
					candidates[i].bValid = true;
					if(bDebug)	debugFile<<"Valid\n";
				}
				else
				{
					cout<<"Invalid2";
					candidates[i].bValid = false;
					if(bDebug)	debugFile<<"Invalid\n";
				}
			}
		}
	}
	else
	{
		candidates[i].bValid = true;
		if(bDebug)	debugFile<<"Valid\n";
	}

	return false;
}

/* Find Local Maxima of the segment and normalize using the maxima */
void FindLocalMaximaNormalizeBuildVideoSegments(SDDiff* pSDDVals, int sddCount, ThreeSignal* tSignals)
{
	vector<SegmentCandidate> candidates;
	vector<VideoSegment> sSegments;

	/* Indeces */
	int index = 0;
	int startIdx = 0;
	int endIdx = 0;
	
	float min = 0.0;
	float max = 0.0;

	/* Counter */
	int iCount = 0;
	int invalidSegments = 0;
	int pCounter = 0;
	int nCounter = 0;

	/* Average Max and Min */
	float avgMax = 0.0;
	float avgMin = 0.0;

	/* Area Under Curve */
	int auc = 0;
	float avgAucL = 0.0;
	float avgAucR = 0.0;
	
	/* Flags */
	bool bCurrentPositive = false;
	bool bPrevPositive = false;
	bool bStarted = false;
	bool bSegFound = false;

	/* Search segments as pair of zero-crossings */
	while(index < sddCount)
	{
		float val = pSDDVals[index].value;
		if(val < 0)
			bCurrentPositive = false;
		else
			bCurrentPositive = true;

		if(index == 0)
		{
			bPrevPositive = bCurrentPositive;

			if(bCurrentPositive)
				max = val;
			else
				min = val;

			startIdx = index;
			bStarted = true;
			index++;
			continue;
		}

		auc += val;

		/* Zero-crossing */
		if(bCurrentPositive != bPrevPositive)
		{
			if(bStarted)
			{
				endIdx = index - 1;
				bStarted = false;
				bSegFound = true;
			}
			else
			{
				//startIdx = index;
				bStarted = true;
				bSegFound = false;
			}
		}
		else
		{
			if(bCurrentPositive)
			{
				if(max < val)
					max = val;
			}
			else
			{
				if(min > val)
					min = val;
			}
		}

		/* Last segment may not have zero-crossing */
		if(bStarted && index == sddCount-1)
		{
			endIdx = sddCount-1;
			bStarted = false;
			bSegFound = true;
		}

		if(bSegFound)
		{
			bSegFound = false;

			if(bPrevPositive)
			{
				pCounter++;
				avgMax += max;
				avgAucL += abs(auc);
			}
			else
			{
				nCounter++;
				avgMin += min;
				avgAucR += abs(auc);
			}

			SegmentCandidate tempCandidate;
			tempCandidate.bIsPositive = bPrevPositive;
			tempCandidate.startIndex = startIdx;
			tempCandidate.endIndex = endIdx;
			tempCandidate.maxMagnitude = bPrevPositive ? max : 0;
			tempCandidate.minMagnitude = bPrevPositive ? 0 : min;
			tempCandidate.areaUnderCurve = abs(auc);
			candidates.push_back(tempCandidate);

			startIdx = index;
			bStarted = true;
			min = max = 0.0;
			auc = 0;
		}

		bPrevPositive = bCurrentPositive;
		index++;
	}

	avgMax /= pCounter;
	avgMin /= nCounter;
	avgAucL /= pCounter;
	avgAucR /= nCounter;

	if(bDebug)	cout<<"\nAverage Max: "<<avgMax<<" Average Min: "<<avgMin;
	
	//debugFile<<"\nAverage AUC Left: "<<avgAucL<<" Average AUC Right: "<<avgAucR<<"\n";

	int nCandCount = candidates.size();
	int candIdx = 0;
	int candStartIdx = 0;
	while(candIdx < nCandCount)
	{
		if(bDebug)	cout<<"\nCandidate: #"<<candIdx+1<<" ";

		/* Found new start */
		if(CheckNewStartCandidateValid(candidates, candIdx, avgMax, avgMin, candStartIdx))
		{
			/* Recheck Candidate validity */
			candStartIdx = candIdx;
			continue;
		}
		candIdx++;
	}

	for(int i=0; i<candidates.size(); i++)
	{
		//debugFile<<candidates[i].areaUnderCurve<<"\n";
		if(candidates[i].bValid)
		{
			LocalNormalizeSDD(pSDDVals, candidates[i]);
			if(bUse2Threshold)
				sSegments.push_back(GetSegment(pSDDVals, candidates[i].startIndex, candidates[i].endIndex, candidates[i].bIsPositive, 0.50, 0.3));
			else
				sSegments.push_back(GetSegment(pSDDVals, candidates[i].startIndex, candidates[i].endIndex, candidates[i].bIsPositive, 0.3));
			iCount++;
		}
	}
		
	cout<<"\nWriting segments to file...";
	cout<<"\nTotal number of segments: "<<iCount;
	cout<<"\nTotal number of invalid segments: "<<candidates.size()-iCount;
	ofstream segmentFile;
	segmentFile.open(fSegName);
	for(int i=0; i<sSegments.size(); i++)
	{
		string side = sSegments[i].type == SegmentType::LEFT_SEG ? "L" : "R";
		segmentFile<<sSegments[i].startFrame<<" "<<sSegments[i].endFrame<<" "<<side<<"\n";
	}
	cout<<"\nDone!!"<<endl;
	segmentFile.close();
}

/* Segment Evaluation with Ground Truth */
void EvaluateSegments2(vector<VideoSegment> segments, PossessionMarker* pMarkerGT, int markerCount)
{
	ofstream resultFile;
	resultFile.open("Segment_Evaluation_Result2.txt", std::ios::app);	
	resultFile<<"\nGame#9";//<<gameIdx;
	int segCount = (int)segments.size();
	int truePositive = 0;
	int falsePositive = 0;
	int falseNegative = 0;	
	int gtSegCount = markerCount;
	
	int gtIndex = 0;
	for(int i=0; i<segCount; i++)
	{
		if(gtIndex < markerCount)
		{
			if(gtIndex == 0)
			{
				if(segments[i].startFrame >= pMarkerGT[gtIndex].frameNumber)
				{
					gtSegCount--;
					gtIndex++;
				}
				//else { i--; gtIndex++; }
			}

			if(i != segCount-1)
			{
				if(segments[i].endFrame <= pMarkerGT[gtIndex].frameNumber && segments[i+1].endFrame > pMarkerGT[gtIndex].frameNumber)
				{
					truePositive++;
						gtIndex++;
				}
				else if(segments[i].endFrame > pMarkerGT[gtIndex].frameNumber)
				{
					if(segments[i+1].endFrame < pMarkerGT[gtIndex].frameNumber)
					{
						falsePositive++;
					}
					else
					{
						/* find the next transition that is greater than segments[i].end */
						int start = gtIndex;
						long end = segments[i].endFrame;
						while(end > pMarkerGT[gtIndex].frameNumber && gtIndex <markerCount)
						{
							gtIndex++;
						}

						if(gtIndex < markerCount)
						{
							truePositive++;
							falseNegative += (gtIndex)-start;
							gtIndex++;
						}
						else
						{
							falseNegative += (gtIndex)-start;
						}
					}
				}
				else
				{
					falsePositive++;
				}
			}
			else
			{
				falsePositive++;
			}
		}
		else
		{
			falsePositive++;
		}
	}

	resultFile<<"\nTotal number of detected segments: "<<segCount;
	resultFile<<"\nTotal number of segments (Ground Truth): "<<markerCount-1;
	resultFile<<"\nTrue Positive: "<<truePositive;
	resultFile<<"\nFalse Positive: "<<falsePositive;
	resultFile<<"\nFalse Negative: "<<falseNegative;
	resultFile<<"\nPrecision: "<<(float)truePositive/(truePositive + falsePositive);
	resultFile<<"\nRecall: "<<(float)truePositive/(truePositive + falseNegative) <<"\n";
}

/* Segment Evaluation with Ground Truth */
void EvaluateSegments(vector<VideoSegment> segments, PossessionMarker* pMarkerGT, int markerCount)
{
	ofstream resultFile;
	resultFile.open("Segment_Evaluation_Result_3.txt", std::ios::app);	
	resultFile<<"Game#9";//<<gameIdx;
	resultFile<<"\nFalse Positive Segments: \n";
	int segCount = (int)segments.size();
	int truePositive = 0;
	int falsePositive = 0;
	int gtIndex = 0;
	int match = 0;
	vector<bool> matched(markerCount, false);
	int gtSegCount = markerCount - 1;

	for(int i = 0; i < segCount; i++)
	{
		long start = segments[i].startFrame;
		long end = segments[i].endFrame;
		while(gtIndex < markerCount)
		{
			long frame = pMarkerGT[gtIndex].frameNumber;

			if(start < frame && end < frame)
			{
				resultFile<<start<<"-"<<end<<"\n";
				falsePositive++;
				break;
			}
			else
			{
				if(!matched[gtIndex])
				{
					match++;
					matched[gtIndex] = true;
					truePositive++;
					gtIndex++;
				}
				else
				{
					cout<<"\nCannot match twice"<<endl;
				}
				break;
			}
		}
	}

	/* False Negative is not quite right */
	int falseNegative = markerCount - match;	

	resultFile<<"\nTotal number of detected segments: "<<segCount;
	resultFile<<"\nTotal number of segments (Ground Truth): "<<markerCount;
	resultFile<<"\nTrue Positive: "<<truePositive;
	resultFile<<"\nFalse Positive: "<<falsePositive;
	resultFile<<"\nFalse Negative: "<<falseNegative;
	resultFile<<"\nPrecision: "<<(float)truePositive/(truePositive + falsePositive);
	resultFile<<"\nRecall: "<<(float)truePositive/(truePositive + falseNegative) <<"\n";
}

/* Build vector of video segments from the possession data */
vector<VideoSegment> BuildVideoSegments(vector<PossessionMarker> transVec, bool bGT)
{
	cout<<"\nBuilding Video Segments... ";
	bool bPrintTransSeg = false;

	ofstream segmentFile;
	if(bGT)		segmentFile.open(fGTSegName);
	else		segmentFile.open(fSegName);

	vector<VideoSegment> sSegments;
	vector<VideoSegment> tSegments;
	VideoSegment temp;
	int index = 0;
	bool bStart = true;
	
	//segmentFile<<"\nSides video Segments:\n";
	while(index < (int)transVec.size() - 1)
	{
		temp.startFrame = transVec[index].frameNumber;
		temp.endFrame = transVec[index + 1].frameNumber;
		temp.length = temp.endFrame - temp.startFrame;
		
		/*
		if(transVec[index].status == FrameStatus::LEFT_START)
		{
			temp.type = SegmentType::LEFT_SEG;
		}
		else if(transVec[index].status == FrameStatus::RIGHT_START)
		{
			temp.type = SegmentType::RIGHT_SEG;
		}
		*/		

		temp.type = (transVec[index].status == FrameStatus::LEFT_START) ? SegmentType::LEFT_SEG : SegmentType::RIGHT_SEG;
		string side = (transVec[index].status == FrameStatus::LEFT_START) ? "L" : "R";
		segmentFile<<temp.startFrame<<" "<<temp.endFrame<<" "<<side<<"\n";
		
		sSegments.push_back(temp);
		index += 2;
	}

	if(bPrintTransSeg)
	{
		segmentFile<<"\n\nTransition video Segments:\n";
		for(int i = 0; i < (int)sSegments.size() - 1; i++)
		{
			temp.startFrame = sSegments[i].endFrame + 1;
			temp.endFrame = sSegments[i+1].startFrame - 1;
			temp.length = temp.endFrame - temp.startFrame;
			temp.type = SegmentType::TRANS_SEG;
			segmentFile<<temp.startFrame<<" "<<temp.endFrame<<" "<<temp.length<<"\n";
		}
	}

	segmentFile.close();
	cout<<"Done!"<<endl;
	return sSegments;
}

/* Find percentage of agreement between results of Manual and Automatic Reference */
void FindAgreementPercentage(vector<VideoSegment> gtSegments, vector<VideoSegment> segments)
{
	ofstream evalFile;
	evalFile.open("EvalOverlapResult.txt", ios::app);
	long diff = 0;
	long ovlStart = 0;
	long ovlEnd = 0;

	int gtSegCount = gtSegments.size();
	int segCount = segments.size();
	float percentage = 0;

	if(segCount != gtSegCount)		cout<<"\nUnequal number of segments in GT and Result"<<endl;

	int gtIndex = 0;
	int extraSegCount = 0;
	int missedGTSegCount = 0;
	for(int i=0; i < segCount; i++)
	{
		while(gtIndex < gtSegCount)
		{
			if(gtSegments[gtIndex].type != segments[i].type)
			{
				cout<<"\nWarning: Mismatch @ frame: Auto Index#"<<i<<" and GT Index#"<<gtIndex;
				if(segments[i].endFrame < gtSegments[gtIndex].startFrame)
				{
					cout<<": Extra Segment";
					extraSegCount++;
					break;
				}
				else if(gtSegments[gtIndex].endFrame < segments[i].startFrame)
				{
					cout<<":GT segment missed";
					missedGTSegCount++;
					gtIndex++;
					continue;
				}
				gtIndex++;
				continue;
			}
			else
			{
				if(segments[i].endFrame < gtSegments[gtIndex].startFrame)
				{
					cout<<"\n*Extra Segment*:"<<i;
					extraSegCount++;
					break;
				}
				else if(gtSegments[gtIndex].endFrame < segments[i].startFrame)
				{
					cout<<"\n*GT segment missed*:"<<gtIndex;
					missedGTSegCount++;
					gtIndex++;
					continue;
				}

				ovlStart = max(gtSegments[gtIndex].startFrame, segments[i].startFrame);
				ovlEnd = min(gtSegments[gtIndex].endFrame, segments[i].endFrame);
				if(ovlEnd < ovlStart)
				{
					cout<<"\nError: Frame: Auto Index#"<<i<<" and GT Index#"<<gtIndex;
					cout<<"\nSomething is wrong"<<endl;
				}
				percentage += (float)(ovlEnd - ovlStart)/segments[i].length;
				gtIndex++;
				break;
			}
		}
	}

	percentage = (percentage/(segCount-extraSegCount)) * 100;
	evalFile<<"Game #"<<gameIdx<<"\nTotal segments: "<<segCount<<"\nPercentage of agreement: "<<percentage<<"%"<<"\nAdditional segments: "<<extraSegCount<<"\n\n";
	cout<<"\nAverage percentage overlap: "<<percentage<<"%"<<endl;
	evalFile.close();
}

/* Evaluate Detected Segments */
void EvaluateDetectedSegments()
{
	// Read Ground Truth file
	int numberOfMarkers = 0;
	FileReader readerGT = FileReader("E:\\Research\\ResultData\\GT\\G9\\Full.txt", true);
	PossessionMarker *pMarkerGT = readerGT.ExtractPossessionMarker(numberOfMarkers);

	string dir = "E:\\Research\\ResultData\\Track Data\\Track_Eval_Segs\\";
	vector<VideoSegment> segs = ExtractSegments(dir + "Game#09_Segments.txt");

	//EvaluateSegments(segs, pMarkerGT, numberOfMarkers);
	EvaluateSegments2(segs, pMarkerGT, numberOfMarkers); 
}

/* Evaluate Ball Entry Detection */
void EvaluateBallEntryDetection()
{
	char buf[100];
	string gtDirectory = "E:\\Research\\ResultData\\BallEntryGroundTruth\\";
	string dtDirectory = "E:\\Research\\Projects\\StruckTracker\\Release\\Results_Fall_2015_Cluster\\";

	ofstream resultFile;
	resultFile.open("Ball_Detection_Evaluation_Result_Cluster.txt", std::ios::app);	
	resultFile<<"\nGame#"<<gameIdx;
	multimap<int,Shot> gt, dt;

	multimap<int,Shot>::iterator it;

	/* Ground Truth */
	sprintf(buf, "G#%d_BallEntryGroundTruth.txt", gameIdx);
	string filePath = gtDirectory + buf;
	int gtLastSeg = -1;
	FileReader beGT = FileReader(filePath.c_str(), true);
	gt = beGT.ReadBallEntryFile(gtLastSeg);
	if(gt.empty()) { exit(0); }

	/* Detected */
	int noShotSegs = 0;
	int initFailSegs = 0;
	int dtLastSeg = -1;
	sprintf(buf, "G#%d_BallEntryResult.txt", gameIdx);
	filePath = dtDirectory + buf;
	FileReader beDT = FileReader(filePath.c_str(), false);
	dt = beDT.ReadBallEntryDetectedFile(noShotSegs, initFailSegs, dtLastSeg);
	if(dt.empty()) { exit(0); }

	int tp = 0, fp = 0,	fn = 0;
	int seg = -1;
	long frame = 0;
	int j = 0;
	vector<bool> visited;

	/* Check out all segments in the ground truth */
	int i = 0;
	for(; i<=gtLastSeg; i++)
	{
		int gtc = gt.count(i);
		int dtc = dt.count(i);
		if(gtc!= 0 && dtc != 0)
		{
			if(gtc == dtc)
			{
				tp += gtc;
			}
			else
			{
				/* If number of shots in ground truth is less than detected we have a case of false positive */
				if(gtc < dtc)
				{
					tp += gtc;
					fp += dtc - gtc;
					cout<<"\nFP: "<<i<<" count: "<<dtc-gtc;
				}
				else /* If the number of shots in ground truth is greater than detected we have a case of false negative */
				{
					tp += dtc;
					fn += gtc - dtc;
					cout<<"\n*FN: "<<i<<" count: "<<gtc-dtc;
				}
			}
		}
		else
		{
			/* If a segment is present in the ground truth but not detected it is a false negative */
			if(gtc != 0 && dtc == 0)
			{
				cout<<"\n*FN: "<<i;
				fn += gtc;
			}
			/* If a segment is not present in the ground truth but detected it is a false positive */
			else if(gtc == 0 && dtc != 0)
			{
				fp += dtc;
				cout<<"\nFP: "<<i;
			}
		}
	}

	while(i<=dtLastSeg)
	{
		fp += dt.count(i);
		cout<<"\nFP: "<<i;
		i++;
	}

	resultFile<<"\na. Total entries: "<<dt.size() + noShotSegs + initFailSegs<<" (b+c+d)";
	resultFile<<"\nb. Total number of detected shots: "<<dt.size()<<" (a-c-d)";
	resultFile<<"\nc. #Segments with no shots detected: "<<noShotSegs;
	resultFile<<"\nd. #Segments with BB outside the frame: "<<initFailSegs;
	resultFile<<"\ne. Total number of shots (Ground Truth): "<<gt.size();
	resultFile<<"\nf. True Positive: "<<tp<<" (e-h)";
	resultFile<<"\ng. False Positive: "<<fp<<" (b-f)";
	resultFile<<"\nh. False Negative: "<<fn<<" (e-f)";
	resultFile<<"\ni. Precision: "<<(float)tp/(tp + fp);
	resultFile<<"\nj. Recall: "<<(float)tp/(tp + fn);

	resultFile<<"\n";
}

/************************************************************
/* ======================== MAIN ========================= */
/* Arguments:
	1. -g : game id
	2. -n : normalization type
	3. -t : three signals
*/
/***********************************************************/
int main( int argc, char** argv )
{	
	ReadCommandLine(argc, argv);
	int mode = -1;
	cout<<"\nAvailable modes: \n1. Find segments\n2. Evaluate Segment Result\n3. Evaluate Ball Entry Detection\nEnter mode:";
	cin >> mode;

	switch(mode)
	{
	case 1:
		{			
			bool bFindLocalMax = true;

			/* 1. Average #L Matches and #R Matches */
			/* 2. Normalize the matches: #LN = #L/(#L+#R), #RN = #R/(#L+#R) */
			/* 3. Normalized Difference Percentage = (#LN - #RN)/100 */
			bool bMethod2 = false;

			// Detect Sides and Transition segments and Evaluate with Ground Truth
			vector<PossessionMarker> transVec;
			int min = 0;
			int max = 0;
			int dataCount = 0;
	
			if(bMethod2)
			{
				ThreeSignal* tSignals;
				string tempFile = sThreeSignalsDirPath + fGTName;
				tempFile = sAutoRefSignalDirPath + fName;
				FileReader reader3Sig = FileReader(tempFile.c_str(), false);
				tSignals = reader3Sig.Extract3Signals(dataCount);

				/* Normalize difference */
				SDDiff* normSDDiff = NormalizeDifference(tSignals, dataCount);
		
				/* Open Debug File */
				if(bDebug)	debugFile.open("Debug.txt");

				/* Find local maxima and detect start and end of segments */
				FindLocalMaximaNormalizeBuildVideoSegments(normSDDiff, dataCount, tSignals);

				/* Close debug file */
				if(bDebug)	debugFile.close();
				return 0;
			}

			/* Find Intervals of Local Maxima */
			if(bFindLocalMax)
			{
				ThreeSignal* tSignals;
				string tempFile = sAutoRefSignalDirPath + fName;
				FileReader reader3Sig = FileReader(tempFile.c_str(), false);
				tSignals = reader3Sig.Extract3Signals(dataCount);

				/* Average over a window to smooth the signal */
				SDDiff* pSDDVals = AverageSideDetectDifference(tSignals, dataCount);

				/* Open Debug File */
				if(bDebug)	debugFile.open("Debug.txt");

				/* Find local maxima and detect start and end of segments */
				FindLocalMaximaNormalizeBuildVideoSegments(pSDDVals, dataCount, tSignals);

				/* Close debug file */
				if(bDebug)	debugFile.close();
				getchar();
				break;
			}

			if(bUseMRefAsGT)
			{
				vector<PossessionMarker> transVecGT;
				if(i3Signals == 1)
				{
					ThreeSignal* tSignals;
					vector<VideoSegment> segmentsGT;
					string tempFile;

					// Ground Truth: Manual Segments
					bool bIgnoreGT = true;
					if(!bIgnoreGT)
					{
						tempFile = sThreeSignalsDirPath + fGTName;
						FileReader reader3SigGT = FileReader(tempFile.c_str(), false);
						tSignals = reader3SigGT.Extract3Signals(dataCount);
						SDDiff* pSDDValsGT = Normalize3Signals(tSignals, dataCount, iNormalize, min, max);
						transVecGT = FindSidesSegment(pSDDValsGT, dataCount, dataCount, min, max);
						segmentsGT = BuildVideoSegments(transVecGT, true);
					}

					// Auto Segments
					tempFile = sAutoRefSignalDirPath + fName;
					FileReader reader3Sig = FileReader(tempFile.c_str(), false);
					tSignals = reader3Sig.Extract3Signals(dataCount);
					SDDiff* pSDDVals = Normalize3Signals(tSignals, dataCount, iNormalize, min, max);
					transVec = FindSidesSegment(pSDDVals, dataCount, dataCount, min, max);
					vector<VideoSegment> segments = BuildVideoSegments(transVec, false);

					if(!bIgnoreGT)	FindAgreementPercentage(segmentsGT, segments);
				}
			}
			else
			{
				// Read Ground Truth file
				int numberOfMarkers = 0;
				FileReader readerGT = FileReader("E:\\Research\\ResultData\\GT\\G1\\Full.txt", true);
				PossessionMarker *pMarkerGT = readerGT.ExtractPossessionMarker(numberOfMarkers);

				if(i3Signals == 0)
				{
					// Read SSD (Side Detection Difference)
					FileReader readerSDD = FileReader("E:\\Research\\ResultData\\SDDData\\G3\\1.txt", false);
					SDDiff * pSDDVals= readerSDD.ExtractSideDetectorData(dataCount, iNormalize, min, max);
		
					int numberOfTransitions = 0;

					if(iNormalize == -1)
					{
						transVec = FindZeroCrossings(pSDDVals, dataCount, numberOfTransitions);
						EvaluateSDD(transVec, numberOfTransitions, pMarkerGT, numberOfMarkers);
					}
					else if(iNormalize == 0 || iNormalize == 2)
					{
						cout<<"\nNormalized: Lets find some segments"<<endl;
						NormalizeSDD(pSDDVals, dataCount, iNormalize, min, max);
						transVec = FindSidesSegment(pSDDVals, dataCount, numberOfTransitions, min, max);
					}
				}
				else
				{
					ThreeSignal* tSignals;
					string tempFile = sThreeSignalsDirPath + fName;
					FileReader reader3Sig = FileReader(tempFile.c_str(), false);
					tSignals = reader3Sig.Extract3Signals(dataCount);
					//SDDiff* midVals = new SDDiff[dataCount];
					SDDiff* pSDDVals = Normalize3Signals(tSignals, dataCount, iNormalize, min, max);
					transVec = FindSidesSegment(pSDDVals, dataCount, dataCount, 0, 0);
					vector<VideoSegment> segments = BuildVideoSegments(transVec, false);
		
					if(bEvaluate)
						EvaluateSegments(segments, pMarkerGT, numberOfMarkers);
				}
			}
			fprintf(stderr, "\nDone!!\n\n");
		}
		break;
	case 2:
		{
			cout<<"\nStart evaluating segments...";
			EvaluateDetectedSegments();
			cout<<"\nDone!!"<<endl;
		}
		break;
	case 3:
		{
			cout<<"\nEnter game id: ";
			cin >> gameIdx;
			cout<<"\nStart evaluating ball detection...";
			EvaluateBallEntryDetection();
			cout<<"\nDone!!"<<endl;
		}
		break;
	default:
		cout<<"\nInvalid mode!\n";
		break;
	}
	getchar();
	return 1;
}