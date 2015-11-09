/* 
* Author	: Vikedo Terhuja
* Date		: April 2015
* Summary	: Tracks backboard (using Structured output with Kernels) on each segment, stores backboard clips 
*			  and performs post-processing for ball detection using active cells information from GridOpticalFlow.
*/
 
#include "Tracker.h"
#include "Config.h"
#include "FileDataReader.h"
#include "FrameMatcherSurf.h"
#include "GridOpticalFlow.h"
#include "Utils.h"
#include <omp.h>
#include<sstream>

#define NDEBUG
#include <cassert>

using namespace std;
using namespace cv;

/*************************** Constants **************************/
const char *LEFT_REF_ARG			= "-l";
const char *RIGHT_REF_ARG			= "-r";
const char *LEFT_REF_FRAME_ARG		= "-al";
const char *RIGHT_REF_FRAME_ARG		= "-ar";
const char *GAME_ID_ARG				= "-g";
const char *SEGMENT_ID_ARG			= "-s";

/************************* Global Variables *********************/
vector<string> imgFile;
map<int, string> refFiles;
map<int, long> autoRefFrames;

/* Tracking Box */
Rect boxL;
Rect boxR;

/* Scale factor */
bool bScaleImage = false;
float scaleW = 1.f;
float scaleH = 1.f;

/* Video parameters */
string video;
VideoCapture vc;

/* Configuration */
Config conf;

/* Flags */
bool drawing_box = false;
bool gotBB = false;				// got tracking box or not
bool fromfile = false;
bool bEvalwGT = false;
bool bBBFromFile = false;
bool bSaveImage = false;
bool bSaveImg4pp = true;		// Save image for post processing
bool bFixedView = false;		// == true means stop using tracker's box and fixed the view especially when there's no camera motion
bool bEval = false;
bool bDebug = false;
bool useCamera = false;

/* Track Backboard + Basket */
bool bTrackBoth = false;
int  trackId = 0;		// 0 = Backboard; 1 = Basket; 2 = Small Square

/* Ground Truth Folder */
string gtDirectory = "E:\\Research\\ResultData\\Track Data\\Tracks_GT\\";
string refBBFile;
string refBSFile;
string gtBBFile;
string gtBSFile;

/* Reference Directory */
string refDirectory = "E:\\Research\\AutoReference\\";
string lRefFile;
string rRefFile;

/* Segments Directory */
string segDirectory = "E:\\Research\\ResultData\\Track Data\\Track_Eval_Segs\\";
string segFile;

/* Result Files */
string resultBBFileName;
string resultBSFileName;

/* Tracker Output Directory */
string trackerDirectory = "E:\\Research\\Projects\\StruckTracker\\Release\\tOutput\\";
string segsFolder = "Segs";
string stabFolder = "Stab";

/* Initialization tracker box file */
string initTrackerBBFile;

/* Collect Training data */
bool bCollectTrain = false;

/* Game ID */
int gId = 0;

/* File handle */
ofstream outFile;
ofstream resultBBFile, resultBSFile;
ofstream lTrScoreFile, rTrScoreFile;

/* Ground Truth */
vector<FrameBox> gt1, gt2;

/**************************** Functions **************************/

/* Get Frame Box */
FrameBox GetFrameBox(string rowString)
{
	FrameBox temp;
	string x1, y1, w1, h1, sFrameNumber;
	int x, y, w, h;
	istringstream linestream(rowString);
	getline(linestream, sFrameNumber, SPACE_SYMBOL);
	getline(linestream, x1, SPACE_SYMBOL);
	getline(linestream, y1, SPACE_SYMBOL);
	getline(linestream, w1, SPACE_SYMBOL);
	getline(linestream, h1, SPACE_SYMBOL);
	temp.frameNumber = atol(sFrameNumber.c_str());
	x = atoi(x1.c_str());
	y = atoi(y1.c_str());
	w = atoi(w1.c_str());
	h = atoi(h1.c_str());

	if(bScaleImage)
		temp.box = Rect(x * scaleW, y * scaleH, w * scaleW, h * scaleH);
	else
		temp.box = Rect(x, y, w, h);

	return temp;
}

/* Read Ground Truth Files */
vector<FrameBox> readGT(const char* file)
{
	vector<FrameBox> gtFB;
	ifstream fs(file);
	string rowString;
	if(fs.is_open())
	{
		while(!fs.eof())
		{
			getline(fs, rowString);
			gtFB.push_back(GetFrameBox(rowString));
		}
	}
	return gtFB;
}

/* Read Bounding box from file */
void readBB(const char* file, FrameMatcherSurf &fm, int tBId)	// get tracking box from file
{
	ifstream tb_file (file);
	string line;

	/* Left Side box */
	getline(tb_file, line);
	istringstream linestream(line);
	string x1, y1, w1, h1;
	getline(linestream, x1, SPACE_SYMBOL);
	getline(linestream, y1, SPACE_SYMBOL);
	getline(linestream, w1, SPACE_SYMBOL);
	getline(linestream, h1, SPACE_SYMBOL);
	int x = atoi(x1.c_str());
	int y = atoi(y1.c_str());
	int w = atoi(w1.c_str());
	int h = atoi(h1.c_str());

	if(bScaleImage)
		boxL = Rect(x * scaleW, y * scaleH, w * scaleW, h * scaleH);
	else
		boxL = Rect(x, y, w, h);

	if(tBId == 1)
		fm.AddTrackingBox(SegmentType::LEFT_SEG, boxL);
	else
		fm.AddTrackingBox2(SegmentType::LEFT_SEG, boxL);

	/* Right Side box */
	getline(tb_file, line);
	istringstream linestream1(line);
	getline(linestream1, x1, SPACE_SYMBOL);
	getline(linestream1, y1, SPACE_SYMBOL);
	getline(linestream1, w1, SPACE_SYMBOL);
	getline(linestream1, h1, SPACE_SYMBOL);
	x = atoi(x1.c_str());
	y = atoi(y1.c_str());
	w = atoi(w1.c_str());
	h = atoi(h1.c_str());
	if(bScaleImage)
		boxR = Rect(x * scaleW, y * scaleH, w * scaleW, h * scaleH);
	else
		boxR = Rect(x, y, w, h);

	if(tBId == 1)
		fm.AddTrackingBox(SegmentType::RIGHT_SEG, boxR);
	else
		fm.AddTrackingBox2(SegmentType::RIGHT_SEG, boxR);
}

/* Print Help */
void print_help(void)
{
	cerr << "StruckTracker [-l left_reference] [-r right_reference ] [-g game_id] "
    << " -l left reference image " << endl
    << " -r right reference image "<< endl
	<< " -b tracking box file " <<endl
	<< " -g game_id " << endl
	<< " -s segments " <<endl;
}

/* Check argument */
int check_arg(int i, int max)
{
    if (i == max) {
        print_help();
        exit(1);
    }
    return i + 1;
}

/* Format files */
void FormatFileNames(int gId)
{
	/* Initial tracker boxes & Ground truth */
	char buff1[50], buff2[50], buff3[50], buff4[50];
	sprintf(buff1, "G#%d\\G#%d_Ref_Track_Board.txt", gId, gId);
	refBBFile = gtDirectory + buff1;
	sprintf(buff2, "G#%d\\G#%d_Ref_Track_Basket.txt", gId, gId);
	refBSFile = gtDirectory + buff2;
	sprintf(buff3, "G#%d\\G#%d_Track_GT_BackBoard.txt", gId, gId);
	gtBBFile = gtDirectory + buff3;
	sprintf(buff4, "G#%d\\G#%d_Track_GT_Basket.txt", gId, gId);
	gtBSFile = gtDirectory + buff4;

	/* Reference Images */
	char lBuf[50], rBuf[50];
	sprintf(lBuf, "G#%d\\G#%d_LeftRef.jpg", gId, gId);
	lRefFile = refDirectory + lBuf;
	refFiles.insert(pair<int, string>(SegmentType::LEFT_SEG, lRefFile));
	sprintf(rBuf, "G#%d\\G#%d_RightRef.jpg", gId, gId);
	rRefFile = refDirectory + rBuf;
	refFiles.insert(pair<int, string>(SegmentType::RIGHT_SEG, rRefFile));

	/* Left/Right segments file */
	char sBuf[50];
	sprintf(sBuf, "Game#%d_Segments.txt", gId);
	segFile = segDirectory + sBuf;

	/* Result files */
	char resBuf1[50], resBuf2[50];
	sprintf(resBuf1, "G#%d_Struck_Track_BB.txt", gId);
	resultBBFileName = resBuf1;
	sprintf(resBuf2, "G#%d_Struck_Track_BS.txt", gId);
	resultBSFileName = resBuf2;

	/* Init Tracker Box file */
	char initBuf[50];
	sprintf(initBuf, "G#%d_Init_BB.txt", gId);
	initTrackerBBFile = initBuf;
}

/* Read Command Line options */
void read_options(int argc, char** argv, VideoCapture &capture, FileReader &vf)
{
	cout<<"\nReading Command line arguments"<<endl;
	for (int i=0; i<argc; i++)
	{
		if (strcmp(argv[i], "-b") == 0)	// read tracking box from file
		{
			if (argc>i)
			{
				gotBB = true;
			}
			else
			{
				print_help();
			}
		}
		else if (strcmp(argv[i], "-v") == 0)	// read video from file
		{
			cout<<"\nReading video file path";
			if (argc > i)
			{
				video = string(argv[i+1]);
				capture.open(video);
				fromfile = true;
			}
			else
			{
				print_help();
			}
		}
		else if (!strcmp(argv[i], LEFT_REF_ARG))
		{
			cout<<"\nReading left reference image file path";
			i = check_arg(i, argc);
			refFiles.insert(pair<int, string>(SegmentType::LEFT_SEG, string(argv[i])));
		}
		else if (!strcmp(argv[i], RIGHT_REF_ARG))
		{
			cout<<"\nReading right reference image file path";
			i = check_arg(i, argc);
			refFiles.insert(pair<int, string>(SegmentType::RIGHT_SEG, string(argv[i])));
		}
		else if (!strcmp(argv[i], LEFT_REF_FRAME_ARG))
		{
			i = check_arg(i, argc);
			autoRefFrames.insert(pair<int, long>(SegmentType::LEFT_SEG, atol(argv[i])));
		}
		else if (!strcmp(argv[i], RIGHT_REF_FRAME_ARG))
		{
			i = check_arg(i, argc);
			autoRefFrames.insert(pair<int, long>(SegmentType::RIGHT_SEG, atol(argv[i])));
		}
		else if(!strcmp(argv[i], GAME_ID_ARG))
		{
			i = check_arg(i, argc);
			gId = atoi(argv[i]);
			video = vf.GetVideoFileName(atoi(argv[i]));
			capture.open(video);
			fromfile = true;
			FormatFileNames(atoi(argv[i]));
		}
		else if(!strcmp(argv[i], SEGMENT_ID_ARG))
		{
			i = check_arg(i, argc);
			segsFolder = argv[i];
		}
	}
}

/* tracking box mouse callback - scale not implemented */
void mouseHandlerLeft(int event, int x, int y, int flags, void *param)
{
	switch (event)
	{
	case CV_EVENT_MOUSEMOVE:
		if (drawing_box)
		{
			boxL.width = x - boxL.x;
			boxL.height = y - boxL.y;
		}
		break;
	case CV_EVENT_LBUTTONDOWN:
		drawing_box = true;
		boxL = Rect(x, y, 0, 0);
		break;
	case CV_EVENT_LBUTTONUP:
		drawing_box = false;
		if (boxL.width < 0)
		{
			boxL.x += boxL.width;
			boxL.width *= -1;
		}
		if( boxL.height < 0 )
		{
			boxL.y += boxL.height;
			boxL.height *= -1;
		}
		gotBB = true;
		break;
	default:
		break;
	}
}

/* tracking box mouse callback - scale not implemented */
void mouseHandlerRight(int event, int x, int y, int flags, void *param)
{
	switch (event)
	{
	case CV_EVENT_MOUSEMOVE:
		if (drawing_box)
		{
			boxR.width = x - boxR.x;
			boxR.height = y - boxR.y;
		}
		break;
	case CV_EVENT_LBUTTONDOWN:
		drawing_box = true;
		boxR = Rect(x, y, 0, 0);
		break;
	case CV_EVENT_LBUTTONUP:
		drawing_box = false;
		if (boxR.width < 0)
		{
			boxR.x += boxR.width;
			boxR.width *= -1;
		}
		if( boxR.height < 0 )
		{
			boxR.y += boxR.height;
			boxR.height *= -1;
		}
		gotBB = true;
		break;
	default:
		break;
	}
}

/* Set the trackers on the reference images */
bool SetTrackBoxOnReferences(FrameMatcherSurf &fm, Config conf)
{
	ofstream initBBFile(initTrackerBBFile);
	Mat frame, first, result;
	/* Set tracking box on both Left Reference */
	namedWindow("Left Reference", CV_WINDOW_AUTOSIZE);
	/* Register mouse callback to draw the tracking box */
	setMouseCallback("Left Reference", mouseHandlerLeft, NULL);

	frame = imread(refFiles[0], 1);
	if(bScaleImage)
	{
		Mat sFrame;
		resize(frame, sFrame, Size(conf.frameWidth, conf.frameHeight));
		sFrame.copyTo(result);
	}
	else
	{
		frame.copyTo(result);
	}

	result.copyTo(first);
	while(!gotBB)
	{
		first.copyTo(result);
		rectangle(result, boxL, Scalar(0,0,255));
		cv::imshow("Left Reference", result);
		if (cvWaitKey(33) == 'q') {	return false; }
	}

	/* Remove callback */
	rectangle(frame, boxL, Scalar(0,0,255));
	cvSaveImage("Left_Ref_Frame.jpg", &(IplImage(frame)));
	frame.release();
	drawing_box = false;
	setMouseCallback("Left Reference", NULL, NULL);
	printf("\nInitial Left Tracking Box = x:%d y:%d h:%d w:%d", boxL.x, boxL.y, boxL.width, boxL.height);		
	fm.AddTrackingBox(SegmentType::LEFT_SEG, boxL);
	initBBFile<<boxL.x<<" "<<boxL.y<<" "<<boxL.width<<" "<<boxL.height<<endl;
	destroyWindow("Left Reference");
	gotBB = false;

	/* Set tracking box on both Left Reference */
	namedWindow("Right Reference", CV_WINDOW_AUTOSIZE);
	/* Register mouse callback to draw the tracking box */
	setMouseCallback("Right Reference", mouseHandlerRight, NULL);

	frame = imread(refFiles[1], 1);
	if(bScaleImage)
	{
		Mat sFrame;
		resize(frame, sFrame, Size(conf.frameWidth, conf.frameHeight));
		sFrame.copyTo(result);
	}
	else
	{
		frame.copyTo(result);
	}

	result.copyTo(first);
	while(!gotBB)
	{
		first.copyTo(result);
		rectangle(result, boxR, Scalar(0,0,255));
		cv::imshow("Right Reference", result);
		if (cvWaitKey(33) == 'q') {	return false; }
	}

	/* Remove callback */
	rectangle(frame, boxR, Scalar(0,0,255));
	cvSaveImage("Right_Ref_Frame.jpg", &(IplImage(frame)));
	frame.release();
	drawing_box = false;
	setMouseCallback("Right Reference", NULL, NULL);
	printf("\nInitial Right Tracking Box = x:%d y:%d h:%d w:%d\n", boxR.x, boxR.y, boxR.width, boxR.height);		
	fm.AddTrackingBox(SegmentType::RIGHT_SEG, boxR);
	initBBFile<<boxR.x<<" "<<boxR.y<<" "<<boxR.width<<" "<<boxR.height<<endl;
	destroyWindow("Right Reference");

	first.release();
	return true;
}

/* Check whether the box is valid and bounded within the size of image */
bool IsValidBoundedBox(Rect box, int width, int height, Rect refBox)
{
	if(box.x < 0 || box.y < 0 || box.width <= 0 || box.height <= 0)
		return false;

	if(box.x + box.width > width || box.y + box.height > height)
		return false;

	if(abs(box.width - refBox.width) > .75*refBox.width || abs(box.height - refBox.height) > .75*refBox.height)
		return false;

	return true;
}

/* Initialize Trackers: Mapping reference bounding boxes to current frame */
bool InitializeTrackers(int &fCount, double start_Frame, int segLength, Mat &output, int segmentType, FrameMatcherSurf &fm, Rect &box1, Rect &box2, Config conf, Tracker &trackerBBL, Tracker &trackerBBR, Tracker &trackerBSL, Tracker &trackerBSR)
{
	cout<<"\nInitializing tracker at: "<<start_Frame+fCount;
	Mat gray, frame, result;
	FloatRect initBB;

	/* Find a good initialization frame - Full backboard visible */
	bool bGoodInitialBox = false;
	//vc.set(CV_CAP_PROP_POS_FRAMES, start_Frame + fCount);
	while(!bGoodInitialBox && fCount < segLength)
	{
		vc.set(CV_CAP_PROP_POS_FRAMES, start_Frame + fCount);
		vc.read(frame);
		//cout<<"\n"<<start_Frame + fCount<<endl;

		if(bScaleImage)
		{
			Mat sFrame;
			resize(frame, sFrame, Size(conf.frameWidth, conf.frameHeight));
			sFrame.copyTo(result);
		}
		else
		{
			frame.copyTo(result);
		}

		/* Find feature matches and compute Homography matrix *
		/* Transform Tracking box position in reference frame to current frame */ 
		if(fm.GetTrackingBoxLocation(segmentType, result, box1, box2))
		{
			if(bSaveImage)
			{
				char buf[20];
				int fn = (int)start_Frame + fCount;
				sprintf(buf, "Frame_%d.jpg", fn);
				string fileName = buf;
				rectangle(result, box1, Scalar(0,0,255));
				cvSaveImage(fileName.c_str(), &(IplImage(result)));
			}

			if(IsValidBoundedBox(box1, result.cols, result.rows, fm.GetReferenceBox(segmentType)))
			{		
				cout<<" Initialization Success!";
				bGoodInitialBox = true;
				cvtColor(result, gray, CV_RGB2GRAY);

				/* Initialize Trackers */
				if(segmentType == 0)
				{
					trackerBBL.Reset();
					initBB = IntRect(box1.x, box1.y, box1.width, box1.height);
					trackerBBL.Initialise(gray, initBB);
					if(bTrackBoth)
					{
						trackerBSL.Reset();
						initBB = IntRect(box2.x, box2.y, box2.width, box2.height);
						trackerBSL.Initialise(gray, initBB);
					}
				}
				else
				{
					trackerBBR.Reset();
					initBB = IntRect(box1.x, box1.y, box1.width, box1.height);
					trackerBBR.Initialise(gray, initBB);
					if(bTrackBoth)
					{
						trackerBSR.Reset();
						cout<<"\nReseting BS ... ";
						initBB = IntRect(box2.x, box2.y, box2.width, box2.height);
						trackerBSR.Initialise(gray, initBB);
						cout<<"Initialization BS Done"<<endl;
					}
				}
				cout<<"\nTracking...\n";
			}
		}

		/* Release memory */
		frame.release();
		gray.release();
		if(!bGoodInitialBox)	
		{
			//cout<<"\nInit box not found\n";
			fCount += 15;
			result.release();
		}
	}

	if(bGoodInitialBox)
	{
		result.copyTo(output);
		return true;
	}
	//cout<<"\nInitializatioin Failed!";
	return false;
}

/* Display trackers */
void DisplayTrackers(vector<FrameBox> gt1, vector<FrameBox> gt2, Mat &frame, Rect box1, Rect box2, int &gtIdx, long frameNumber, bool bReInit)
{
	Rect gtBox;
	bool bFound = false;
	int idx = 0;

	if(bEvalwGT)
	{
		while(gtIdx < gt1.size())
		{
			if(frameNumber == gt1[gtIdx].frameNumber)
			{
				bFound  = true;
				idx = gtIdx;
				break;
			}
			else if(frameNumber < gt1[gtIdx].frameNumber)
				break;

			gtIdx++;
		}

		if(bFound)
		{
			gtBox = gt1[idx].box;
			rectangle(frame, gtBox, Scalar(0,0,255));
			if(bTrackBoth)
			{
				gtBox = gt2[idx].box;
				rectangle(frame, gtBox, Scalar(0,0,255));
			}
		}
	}

	if(bReInit)
	{
		Rect rBox;
		rBox.x = 50;
		rBox.y = 50;
		rBox.width = frame.cols - 100;
		rBox.height = frame.rows - 100;
		rectangle(frame, rBox, Scalar(0,255,255));
	}
	// Draw Points
	rectangle(frame, box1, Scalar(255,0,0));
	if(bTrackBoth)	rectangle(frame, box2, Scalar(0,255,0));
	// Display
	cv::imshow("Backboard Tracker", frame);
	//printf("Current Tracking Box = x:%d y:%d h:%d w:%d\n", box.x, box.y, box.width, box.height);
}

/* Load GT and Segment files for debugging tracker */
void DebugSingleTracker(vector<FrameBox> &gt1, FrameMatcherSurf &fm)
{
	switch(trackId)
	{
	case 0:
		readBB("E:\\Research\\ResultData\\Track Data\\Tracks_GT\\G#1_Ref_Track_GT.txt", fm, 1);
		/* Read Ground Truth */
		gt1 = readGT("C:\\Vikedo\\Research\\Eval_Track\\G#1_Track_GT_12_Segments.txt");
		break;
	case 1:
		readBB("E:\\Research\\ResultData\\Track Data\\Tracks_GT\\G#1_Ref_Track_GT_Basket.txt", fm, 1);
		/* Read Ground Truth */
		gt1 = readGT("C:\\Vikedo\\Research\\Eval_Track\\G#1_Track_GT_Basket.txt");
		break;
	case 2:
		readBB("E:\\Research\\ResultData\\Track Data\\Tracks_GT\\G#1_Ref_Track_Mini_Square.txt", fm, 1);
		/* Read Ground Truth */
		gt1 = readGT("E:\\Research\\ResultData\\Track Data\\Tracks_GT\\G#1_Track_GT_Mini_Square.txt");	
		break;
	}
	gotBB = true;
}

/* Initialize Trackers on the Reference Images */
bool InitializeTrackerOnReference(vector<FrameBox> &gt1, vector<FrameBox> &gt2, FrameMatcherSurf &fm, Config conf)
{
	/* Initialization of tracking box */
	if(!SetTrackBoxOnReferences(fm, conf))	return false;
	
	/* Only for Evaluation Purposes */
	if(bEvalwGT)
	{
		readBB("E:\\Research\\ResultData\\Track Data\\Tracks_GT\\G#1_Ref_Track_GT_Basket.txt", fm, 2);		// Basket

		/* Read Ground Truth */
		gt1 = readGT("C:\\Vikedo\\Research\\Eval_Track\\G#1_Track_GT_12_Segments.txt");						// Backboard
		gt2 = readGT("C:\\Vikedo\\Research\\Eval_Track\\G#1_Track_GT_Basket.txt");							// Basket
	}
	return true;
}

/* Remove old tracker output folder and create new folder */
void ResetOutputFolder()
{
	/* Remove Segs and Stab folders */
	string cmd = "rmdir " + trackerDirectory + segsFolder + " /s /q";
	system(cmd.c_str());
	cmd = "rmdir " + trackerDirectory + stabFolder + " /s /q";
	system(cmd.c_str());
	/* Recreate Segs and Stab folders */
	cmd = "mkdir " + trackerDirectory + segsFolder;
	system(cmd.c_str());	
	cmd = "mkdir " + trackerDirectory + stabFolder;
	system(cmd.c_str());
}

/* Create temporary directory for storing backboard clips */
void CreateSegDirectory(int segId)
{
	char tBuf[20];
	sprintf(tBuf, "_S%02d", segId);
	string cmd = "mkdir " + trackerDirectory + segsFolder + tBuf;
	system(cmd.c_str());
}

/* Display the trackers' BB extracted from file */
void DisplayTrackerBBFromFile()
{
	Mat frame = imread(refFiles[0], 1);
	rectangle(frame, boxL, Scalar(0,0,255));
	cv::imshow("Left Reference", frame);
	if (cvWaitKey(0) == 'q') {	exit(-1); }
	destroyWindow("Left Reference");

	frame = imread(refFiles[1], 1);
	rectangle(frame, boxR, Scalar(0,0,255));
	cv::imshow("Right Reference", frame);
	if (cvWaitKey(0) == 'q') {	exit(-1); }
	destroyWindow("Right Reference");
}

/* Check for tracker initialization box File */
void CheckInitTrackerBBFile()
{
	ifstream initFile(initTrackerBBFile);
	if(initFile == NULL)	bBBFromFile =  false;
	else					bBBFromFile = true;
}

/* Method to put test cases */
int TestMethods()
{
	bool bTest = true;
	if(bTest)
	{
		double start = omp_get_wtime();
		string dirName = "E:\\Research\\Projects\\StruckTracker\\Release\\tOutput\\Segs"; 
		DirectoryReader outDir = DirectoryReader(dirName);
		vector<string> images = outDir.Read();
		Mat temp = imread(images[0], 1);
		GridOpticalFlow g = GridOpticalFlow(dirName, "", temp.cols, temp.rows);
		vector<long> segments;
		for(int i=0; i<images.size(); i++)
		{
			segments.push_back(i);
		}
		vector<long> entryFrames = g.DetectBallEntry(segments,7);
		cout<<"\nEntry frames: ";
		for(int i=0; i<entryFrames.size(); i++)
		{
			cout<<entryFrames[i]<<" ";
		}
		double end = omp_get_wtime();
		cout<<"\nTime elapsed: "<<end - start<<endl;
		getchar();
		return 0;

		if(bCollectTrain)
		{
			string dirName = "E:\\Research\\Projects\\StruckTracker\\Release\\tOutput\\Train\\";
			char buf[20];
			string fileName;
			sprintf(buf, "G#%02d\\", gId);
			dirName = dirName + buf + segsFolder;
			GridOpticalFlow g = GridOpticalFlow(dirName, segsFolder, gId);
			g.GetTrainingVectors();
		}
		else
		{
			string dirName = "E:\\Research\\Projects\\StruckTracker\\Release\\tOutput\\Segs";
			GridOpticalFlow g = GridOpticalFlow(dirName, segsFolder, gId);
			g.GetTrainingVectors();
		}
	}
	return -1;
}

/* Setup the configuration for all the processes */
bool SetupConfigureProcess(int argc, char* argv[])
{
	/* Read config file */
	string configPath = "config.txt";
	conf = Config(configPath);
	
	FileReader vidFiles = FileReader("E:\\Research\\Projects\\HUDL_Games.txt");
	vidFiles.ReadVideoFileNames();

	if (conf.features.size() == 0)
	{
		cout << "error: no features specified in config" << endl;
		return false;
	}
	
	if (conf.resultsPath != "")
	{
		outFile.open(conf.resultsPath.c_str(), ios::out);
		if (!outFile)
		{
			cout << "error: could not open results file: " << conf.resultsPath << endl;
			return false;
		}
	}
	
	/* Read command line options */
	read_options(argc, argv, vc, vidFiles);

	// if no sequence specified then use the camera
	useCamera = (conf.sequenceName == "" && !fromfile);

	/* Scale image - mostly for debugging and view tracker on GUI */
	if(bScaleImage)
	{
		Mat tmp;
		vc >> tmp;
		scaleW = (float)conf.frameWidth/tmp.cols;
		scaleH = (float)conf.frameHeight/tmp.rows;
	}
	
	/* Init camera */
	assert(vc.isOpened());
	if (!vc.isOpened())
	{
		cout << "capture device failed to open!" << endl;
		return false;
	}
	return true;
}

/* Initialize bounding box from file */
void InitializeBoundingBoxFromFile(FrameMatcherSurf &fm)
{
	if(bTrackBoth)
	{
		readBB(refBBFile.c_str(), fm, 1);				// Backboard
		readBB(refBSFile.c_str(), fm, 2);				// Basket
		gotBB = true;

		/* Read Ground Truth */
		if(bEvalwGT)
		{
			gt1 = readGT(gtBBFile.c_str());				// Backboard
			gt2 = readGT(gtBSFile.c_str());				// Basket
		}
	}
	else
	{
		if(bEvalwGT)
			DebugSingleTracker(gt1, fm);
		else
			readBB(initTrackerBBFile.c_str(), fm, 1);
		DisplayTrackerBBFromFile();
	}
}

/* Initialize debugging */
void InitDebugFiles()
{
	/* Files to write results */
	if(bDebug)
	{
		resultBBFile.open(resultBBFileName);
		resultBSFile.open(resultBSFileName);
		rTrScoreFile.open("RightTrackerScore.txt");
		lTrScoreFile.open("LeftTrackerScore.txt");
	}
}

/* Program starts here */
int main(int argc, char* argv[])
{
	if(!SetupConfigureProcess(argc, argv)) return EXIT_FAILURE;

	/* Initialize Frame Matcher */
	FrameMatcherSurf fm = FrameMatcherSurf(autoRefFrames, refFiles, vc, bTrackBoth);
	
	/* Initialize tracker on backboard: manual/auto */
	CheckInitTrackerBBFile();
	if(!bBBFromFile)
	{
		if(!InitializeTrackerOnReference(gt1, gt2, fm, conf)) return -1;
	}
	else	{ InitializeBoundingBoxFromFile(fm); }		

	/* Initialize debugging files if enabled */
	InitDebugFiles();

	/* Initialize Frame Matcher */
	fm.InitReferenceFrames(bScaleImage, conf.frameWidth, conf.frameHeight);
	
	/* Read Segment files */
	FileReader fd = FileReader(segFile.c_str());
	vector<VideoSegment> segments = fd.ExtractSegments();

	/* Initialize Util object */
	Utils ut(bDebug, bDebug, bCollectTrain, trackerDirectory, segsFolder);

	/* Create tracker objects: Backboard (Left), Basket (Left), Backboard (Right), Basket (Right) */
	Tracker tracker(conf);
	Tracker trackerBBL(conf);
	Tracker trackerBBR(conf);
	Tracker trackerBSL(conf);
	Tracker trackerBSR(conf);
	
	/**** Initialization End ****/
	int nSegments = segments.size();
	cout<<"\nTotal segments:" <<nSegments<<endl;
	int gtIdx = 0;

	/* Selected segments section */
#if 0
	int selectedSegs[] = {154};
	vector<int> sSegs(selectedSegs, selectedSegs + sizeof(selectedSegs) / sizeof(int));
#endif

	Mat frame, bbFrame, iFrame;
	Mat current_gray;
	int interval = 1;
	double start = omp_get_wtime();
	int reInit = 0;
	vector<long> segmentFrames;

#if 0
	for(int i=0; i < sSegs.size(); i++)
#endif
#if 1
	for(int i=0; i < segments.size(); i++)
#endif
	{
		/* Clear the Temporary Folder */
		ResetOutputFolder();

		int fCount = 0, fIdx = 0;
		double start_Frame = 0;
		Rect  newBox1, newBox2;
		
		/* Selected segments section */
#if 0	
		int segLength =  segments[sSegs[i]].endFrame - segments[sSegs[i]].startFrame + 1;
		int segmentType = (int)segments[sSegs[i]].type;
		start_Frame = segments[sSegs[i]].startFrame;
		int segId = sSegs[i];
#endif	

		/* All segments section */		
#if 1
		int segLength =  segments[i].endFrame - segments[i].startFrame + 1;
		int segmentType = (int)segments[i].type;
		start_Frame = segments[i].startFrame;
		int segId = i;
#endif	
		
		cout<<"\n"<<i<<": Tracking in Segment #"<<segId<<" . . . : "<<start_Frame;

		/* Initialize Trackers */
		bool bInitSuccess = InitializeTrackers(fCount, start_Frame, segLength, iFrame, segmentType, fm, newBox1, newBox2, conf, trackerBBL, trackerBBR, trackerBSL, trackerBSR);
		
		if(!bInitSuccess)
		{ 
			/* Backboard not fully visible */
			/* Definition: BallEntry Format
			*  Segment ID	#Frame
			*  #Frame = -1 (No ball entry/no shot in the segment)
			*  #Frame = -2 (Tracker failed to initialize in the segment: Due to partial view of backboard)
			*/
			ofstream resFile;
			resFile.open("BallEntryResult.txt", ios::app);
			resFile<<segId<<" "<<-2<<"\n";
			cout<<" Initialization Failed. Skip segment!";
			continue;
		}
		GridOpticalFlow gof = GridOpticalFlow(trackerDirectory + segsFolder, "", newBox1.width, newBox1.height);

		if(bSaveImg4pp)	ut.SaveImageToFile(iFrame, newBox1, segId, fIdx);
		segmentFrames.push_back(fCount + start_Frame);
		fCount++;
		
		while(bInitSuccess && fCount <= segLength)
		{
			double minScore = DBL_MAX, maxScore = -DBL_MAX;
			bool bTracked = false;
			Rect bb, bs;
			bool bReInit = false;
			/* In case we want to use interval */
			//current_Frame = start_Frame + fCount;
			//cap.set(CV_CAP_PROP_POS_FRAMES, current_Frame);
			//cap.read(frame);
			vc.read(frame);
			Mat result;
			if(bScaleImage)
			{
				Mat sFrame;
				resize(frame, sFrame, Size(conf.frameWidth, conf.frameHeight));
				cvtColor(sFrame, current_gray, CV_RGB2GRAY);
				sFrame.copyTo(result);
			}
			else
			{
				cvtColor(frame, current_gray, CV_RGB2GRAY);
				frame.copyTo(result);
			}

			/* Process Frame */
			if(segmentType == 0)
			{
				if(trackerBBL.IsInitialised())
				{					
					if(bDebug)
					{
						bTracked = trackerBBL.TrackDebug(current_gray, maxScore, minScore);
						lTrScoreFile << start_Frame + fCount << " "<<maxScore<<"\n";
					}
					else
						bTracked = trackerBBL.TrackValidate(current_gray);

					if (!conf.quietMode && conf.debugMode)
						trackerBBL.Debug();
				}

				if(trackerBSL.IsInitialised())
				{
					trackerBSL.Track(current_gray);
					if (!conf.quietMode && conf.debugMode)
						trackerBSL.Debug();
				}
			}
			else
			{
				if(trackerBBR.IsInitialised())
				{					
					if(bDebug)
					{
						bTracked = trackerBBR.TrackDebug(current_gray, maxScore, minScore);
						rTrScoreFile << start_Frame + fCount << " "<<maxScore<<"\n";
					}
					else
						bTracked = trackerBBR.TrackValidate(current_gray);

					if (!conf.quietMode && conf.debugMode)
						trackerBBR.Debug();
				}

				if(trackerBSR.IsInitialised())
				{
					trackerBSR.Track(current_gray);
					if (!conf.quietMode && conf.debugMode)
						trackerBSR.Debug();
				}
			}

			/* Re-initialization */
			if(!bTracked)
			{	
				/* Reinitialize Trackers */
				bReInit = InitializeTrackers(fCount, start_Frame, segLength, result, segmentType, fm, newBox1, newBox2, conf, trackerBBL, trackerBBR, trackerBSL, trackerBSR);
				reInit++;
			}
				
			/* Tracking failed && Reinitialization failed:  Stop Tracking */
			if(!bTracked && !bReInit)
			{
				cout<<" Backboard beyond the frame!";
				current_gray.release();
				frame.release();
				break;
			}
			
			const FloatRect& bb1 = segmentType == 0 ? trackerBBL.GetBB() : trackerBBR.GetBB();
			const FloatRect& bb2 = segmentType == 0 ? trackerBSL.GetBB() : trackerBSR.GetBB();
			bb = ut.GetRectBox(bb1);
			bs = ut.GetRectBox(bb2);

			if(!bFixedView & bEval)	ut.WriteToFile(resultBBFile, resultBSFile, start_Frame, fCount, bb1, bb2, scaleW, scaleH);
		
			/* Save the backboard image for post processing */
			if(bSaveImg4pp)	ut.SaveImageToFile(result, bb, segId, fIdx);
			segmentFrames.push_back(fCount + start_Frame);
	
			/* Display the trackers */
			if (!conf.quietMode)
			{
				DisplayTrackers(gt1, gt2, result, bb, bs, gtIdx, start_Frame + fCount, bReInit);
				int key = cvWaitKey(33);
				if (key == 'q') { return 0; }
				else if(key == 'n') { break; }
			}
			fCount++;
			result.release();
			current_gray.release();
			frame.release();
		}
		
		/* Detect Ball Entry */
		gof.DetectBallEntry(segmentFrames, i);
		segmentFrames.clear();
	}
	
	if (outFile.is_open())	outFile.close();
	double end = omp_get_wtime();
	ofstream perFile;
	perFile.open("Performance.txt", ios::app);
	perFile<<"Game: "<<video<<endl;
	perFile<<"Computation time: "<<end - start<<endl;
	perFile<<"Average time: "<<(end-start)/segments.size()<<endl<<endl;
	perFile.close();
	return EXIT_SUCCESS;
}
