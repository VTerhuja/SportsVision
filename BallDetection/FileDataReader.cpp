#include "FileDataReader.h"

/***************************************************************
/* ************** Class: FileReader  Constructor ***************
/***************************************************************/
FileReader::FileReader(const char* name, bool bIsGT)
{
	fileName = name;
	bGroundTruth = bIsGT;
}

FileReader::FileReader(const char* name)
{
	fileName = name;
}

/* Get the number of lines in the file */
int FileReader::GetLinesCount(string fileName)
{
	ifstream fs(fileName);
	int number_of_lines = 0;
	string rowString;
	if(fs.is_open())
	{
		while (getline(fs, rowString))
		{
			++number_of_lines;
		}
		fs.close();
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
		return NULL;
	}
	return number_of_lines;
}

/*********************************************************************
/*
Summary: Extract Ground Truth transition points
Parameters:
	1. numberOfMarkers : reference that holds no. of transition points
*/
/*********************************************************************/
PossessionMarker* FileReader::ExtractPossessionMarker(int &numberOfMarkers)
{
	PossessionMarker *pMarker;
	string rowString;
	int number_of_lines = GetLinesCount(fileName);	
	numberOfMarkers = number_of_lines;

	ifstream fs(fileName);
	if(fs.is_open())
	{
		pMarker = new PossessionMarker[number_of_lines];

		int pIndex = 0;
		// Get the data
		while(!fs.eof())
		{
			getline(fs, rowString);
			int nextIndex;
			pMarker[pIndex].frameNumber = ExtractFrameNumber(rowString, nextIndex);
			ExtractFrameStatus(rowString, pMarker[pIndex], nextIndex);

			pIndex++;
		}
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
	}
	fs.close();

	// Just for debug
	bool bDebug = false;
	if(bDebug)
	{
		for(int i=0; i<10; i++)
		{
			cout<<"\n"<<pMarker[i].frameNumber<<" "<<pMarker[i].status;
		}
		return NULL;
	}				
	return pMarker;
}

/****************************************************
/*
Summary: Extract Side Detector Difference Data
Parameters:
	1. numberOfLines : SDD array size
	2. bNormalize : Normalize value
	3. min: Minimum SDD value (reference)
	4. max: Maximum SDD value (reference)
*/
/***************************************************/
SDDiff* FileReader::ExtractSideDetectorData(int &numberOfLines, int iNormalize, int &min, int &max)
{
	SDDiff* SDDVals;

	ifstream fs(fileName);
	int number_of_lines = 0;
	string rowString;
			
	// Get number of lines
	if(fs.is_open())
	{
		while (getline(fs, rowString))
		{
			++number_of_lines;
		}
		fs.close();
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
		return NULL;
	}

	numberOfLines = number_of_lines;

	max = 0;
	min = 0;			
	int maxFrame = 0;
	int minFrame = 0;

	fs.open(fileName);
	if(fs.is_open())
	{
		SDDVals = new SDDiff[number_of_lines];

		int value = 0;
		int pIndex = 0;
		// Get the data
		while(!fs.eof())
		{
			getline(fs, rowString);
			int nextIndex;
			SDDVals[pIndex].frameNumber = ExtractFrameNumber(rowString, nextIndex);
			value = ExtractSDDValue(rowString, SDDVals[pIndex], nextIndex);

			if(value > max)
			{
				max = value;
				maxFrame = SDDVals[pIndex].frameNumber;
			}

			if(value < min)
			{
				min = value;
				minFrame = SDDVals[pIndex].frameNumber;
			}

			pIndex++;
		}
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
	}
	fs.close();

	cout<<"\nMaximum Frame: "<<maxFrame<<" Minimum Frame: "<<minFrame;
	cout<<"\nMaximum: "<<max<<" Minimum: "<<min<<endl;

	// Just for debug
	bool bDebug = false;
	if(bDebug)
	{
		for(int i=0; i<60; i++)
		{
			cout<<"\n"<<SDDVals[i].frameNumber<<" "<<SDDVals[i].value;
		}
		return NULL;
	}	

	return SDDVals;
}

/****************************************************
/*
Summary: Extract Frame Status from the string
Parameters:
	1. rowString : Frame and status value string
	2. pM : Possession Marker
	3. nextIndex : Next index
*/
/***************************************************/
void FileReader::ExtractFrameStatus(string rowString, PossessionMarker &pM, int nextIndex)
{
	string value = rowString.substr(nextIndex, rowString.length());
	pM.status = (FrameStatus)atoi(value.c_str());
}

/****************************************************
/*
Summary: Extract Frame Number from the string
Parameters:
	1. rowString : Frame and SDD value string
	2. sdd : SDD array
*/
/***************************************************/
long FileReader::ExtractFrameNumber(string rowString, int &nextIndex)
{
	nextIndex = 0;
	int startIndex = 0;
	nextIndex = rowString.find_first_of(SPACE_SYMBOL, startIndex);
	string value = rowString.substr(startIndex, nextIndex-startIndex);
	nextIndex = nextIndex + 1;
	return atol(value.c_str());
}

/****************************************************
/*
Summary: Extract SDD value from the string
Parameters:
	1. rowString : Frame and SDD value string
	2. sdd : SDD array
	3. nextIndex : Next index
*/
/***************************************************/
int FileReader::ExtractSDDValue(string rowString, SDDiff &sdd, int nextIndex)
{
	string value = rowString.substr(nextIndex, rowString.length());
	sdd.value = atof(value.c_str());
	return sdd.value;
}

/****************************************************
/*
Summary: Extract Signal value
Parameters:
	1. rowString : Frame and SDD value string
	2. nextIndex : Next index
*/
/***************************************************/
int FileReader::ExtractSignal(string rowString, int &nextIndex)
{
	int startIndex = nextIndex;
	nextIndex = rowString.find_first_of(SPACE_SYMBOL, startIndex);
	string value = rowString.substr(startIndex, nextIndex-startIndex);
	nextIndex = nextIndex + 1;
	return atoi(value.c_str());
}

/****************************************************
/*
Summary: Extract Three Signals from the file
Parameters:
	1. count : Array size (ref)
	3. min: Minimum SDD value (ref)
	4. max: Maximum SDD value (ref)
*/
/***************************************************/
ThreeSignal* FileReader::Extract3Signals(int &count)
{
	cout<<"Reading file: \n"<<fileName;
	ThreeSignal* tSignals;
	
	ifstream fs(fileName);
	int number_of_lines = 0;
	string rowString;
			
	// Get number of lines
	if(fs.is_open())
	{
		while (getline(fs, rowString))
		{
			++number_of_lines;
		}
		fs.close();
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
		return NULL;
	}

	count = number_of_lines;

	fs.open(fileName);
	if(fs.is_open())
	{
		tSignals = new ThreeSignal[number_of_lines];

		int value = 0;
		int pIndex = 0;

		// Get the data
		while(!fs.eof())
		{
			getline(fs, rowString);
			int nextIndex;
			tSignals[pIndex].frameNumber = ExtractFrameNumber(rowString, nextIndex);
			tSignals[pIndex].lMatches = ExtractSignal(rowString, nextIndex);
			tSignals[pIndex].rMatches = ExtractSignal(rowString, nextIndex);
			tSignals[pIndex].mMatches = ExtractSignal(rowString, nextIndex);

			/*if(value > max)
			{
				max = value;
				maxFrame = SDDVals[pIndex].frameNumber;
			}

			if(value < min)
			{
				min = value;
				minFrame = SDDVals[pIndex].frameNumber;
			}
			*/

			pIndex++;
		}
	}
	else
	{
		cout<< "Error: File: "<< fileName << " not found\n";
	}
	fs.close();
	cout<<"\nDone!"<<endl;

	return tSignals;
}

/* Extract Segment information */
VideoSegment FileReader::ExtractSegmentData(string rowString)
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
vector<VideoSegment> FileReader::ExtractSegments()
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

/* Read file names of all the video files */
void FileReader::ReadVideoFileNames()
{
	ifstream fs(fileName);
	if(fs.is_open())
	{ 
		string rowString;
		int idx = 0;
		while(!fs.eof())
		{
			idx++;
			getline(fs, rowString);
			videoFiles.insert(pair<int,string>(idx,rowString));
		}
	}
}

/* Retrieve a video file name by Id */
string FileReader::GetVideoFileName(int gId)
{
	return videoFiles[gId];
}