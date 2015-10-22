#include <iostream>
#include <stdio.h>
#include "CourtSideIdentifier.h"
#include <omp.h>
#include <fstream>
#include "prtable.h"
#include "Utilities.h"

using namespace std;

/*************************** Constants **************************/
const char *LEFT_REF_ARG	= "-l";
const char *RIGHT_REF_ARG	= "-r";
const char *MID_REF_ARG		= "-m";
const char *GAME_ARG		= "-g";
const char *VIDEO_ARG		= "-v";
const char *INTERVAL_ARG	= "-i";
const char *SCALE_ARG		= "-s";
const char *START_FRAME_ARG = "-sf";
const char *END_FRAME_ARG	= "-ef";
const char *HELP			= "/?";

/**************************** Global Variable ******************************/
string		strOutputFile	= "Game#";
string		strDiffFile		= "Game#";
string		strPRInputFile	= "10_PageRank_Game#8_i60_75.txt";
string		strCurGame		= "";
long		start_frame		= 1;
long		end_frame		= 10000;
int			intervalSize	= 15;
char		img_file[REF_IMG_COUNT][MAX_LENGTH_FILE];
int			gameIdx			= 3;
int			videoIdx		= 0;
Video		v;

bool		bRescale		= true;
bool		bVerbose		= false;
bool		bIsFile			= false;
float		scaleFactor		= 3.0;
int			rescaleMinSize	= 1;

// Visualization
bool		bDrawMatches	= true;
vector<MatchPoint> good_L_matches;
vector<MatchPoint> good_R_matches;
vector<MatchPoint> good_M_matches;
bool		bDrawFeatures	= false;
bool		bWrite2File		= true;

// Peformance
bool		bShowTimer		= false;
double		Start			= 0;
double		End				= 0;
	
// Auto Reference
map<long, int>				frame2Index;
long						autoRefFrames[REF_IMG_COUNT];	// Index 0: Left, 1: Right, 2: Mid

// Counter
int pairMatchCounter		= 0;
/***************************************************************************/

/****************************************************
/*
Summary: Methods to parse the commandline arguments
*/
/***************************************************/
void help()
{
    cerr << "CourtSideIdentifier [-l left_reference] [-r right_reference ] [-g game_id] [-v video_id] "
         << "[-i interval_size] [-s scale] [-sf start_frame] [-ef end_frame]" << endl
         << " -l left reference image " << endl
         << " -r right reference image "<< endl
		 << " -m mid reference image "<<endl
         << " -g game id " << endl
         << " -v video id " << endl
         << " -i interval size " << endl
         << " -s scale factor " << endl
         << " -sf start frame " << endl
         << " -ef end frame" << endl;
}

int check_arg(int i, int max)
{
    if (i == max) {
        help();
        exit(1);
    }
    return i + 1;
}

/* Read command line arguments */
void ReadCommandLine(int argc, char** argv)
{
	int i = 1;
    while (i < argc)
	{
        if (!strcmp(argv[i], LEFT_REF_ARG))
		{
			i = check_arg(i, argc);
            strcpy(img_file[0], argv[i]);
		}
		else if (!strcmp(argv[i], RIGHT_REF_ARG))
		{
			i = check_arg(i, argc);
            strcpy(img_file[1], argv[i]);
		}
		else if (!strcmp(argv[i], MID_REF_ARG))
		{
			i = check_arg(i, argc);
			if(REF_IMG_COUNT == 3)
				strcpy(img_file[2], argv[i]);
		}
        else if (!strcmp(argv[i], GAME_ARG))
		{
			i = check_arg(i, argc);
            gameIdx = atoi(argv[i]);
		}
        else if (!strcmp(argv[i], VIDEO_ARG))
		{
			i = check_arg(i, argc);
			videoIdx = atoi(argv[i]);
		}
        else if (!strcmp(argv[i], INTERVAL_ARG))
		{
			i = check_arg(i, argc);
			int interval = atoi(argv[i]);
			if(interval > 0)
				intervalSize = interval;
		}
		else if (!strcmp(argv[i], SCALE_ARG))
		{
			i = check_arg(i, argc);
			float scale = atof(argv[i]);
			if(scale > 0)
			{
				scaleFactor = scale;
				bRescale = true;
			}
		}
		else if (!strcmp(argv[i], START_FRAME_ARG)) 
		{
			i = check_arg(i, argc);
			start_frame = atol(argv[i]);

        } else if (!strcmp(argv[i], END_FRAME_ARG))
		{
			i = check_arg(i, argc);
			end_frame = atof(argv[i]);
        } else if (!strcmp(argv[i], HELP))
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
}

/* Print the command line arguments */
void PrintCmdLineArgs(int argc, char** argv, ofstream file)
{
	file<<"\n";
	for(int i = 0; i < argc; i++)
	{
		file<<argv[i]<<" ";
	}
	file<<"\n";
}

/****************************************************
/*
Summary: Select video based on the video identifier
Parameters:
	1. gameId	: Game Id
	2. vIdx		: Video Index (multiple clips game)
*/
/***************************************************/
void SelectVideo(int gameId, int vIdx)
{
	char buff[100];
	// Format: Left Right Mid (LRM)
	sprintf(buff, "%d_Auto_i%d_s%.2f.txt", gameId, intervalSize, scaleFactor);
	string fName = buff;
	strOutputFile.append(fName);
	sprintf(buff, "%d_Diff.txt", gameId, intervalSize, scaleFactor);
	fName = buff;
	strDiffFile.append(fName);
	
	HFileReader vidFiles = HFileReader("E:\\Research\\Projects\\HUDL_Games.txt");
	vidFiles.ReadVideoFileNames();

	strCurGame = vidFiles.GetVideoFileName(gameId);
}

/****************************************************
/*
Summary: Load video and extracts video parameters
Parameters:
	1. gameId	: Game Id
	2. vIdx		: Video Index (multiple clips game)
*/
/***************************************************/
Video LoadVideo(int gameId, int idx)
{
	SelectVideo(gameId, idx);
	Video v;
	v.vc= cvCaptureFromFile(strCurGame.c_str());
	if (v.vc != NULL)
	{
		cout<<"\nGame : "<<strCurGame<<endl;
		// Read the video's frame size out of the video
		v.frame_size.height =
			(int) cvGetCaptureProperty( v.vc, CV_CAP_PROP_FRAME_HEIGHT );
		v.frame_size.width =
			(int) cvGetCaptureProperty( v.vc, CV_CAP_PROP_FRAME_WIDTH );

		// Determine the number of frames in the video
		long number_of_frames;

		// Go to the end of the video (ie: the fraction is "1")
		cvSetCaptureProperty( v.vc, CV_CAP_PROP_POS_AVI_RATIO, 1. );
		// Now that we're at the end, read the video position in frames 
		number_of_frames = (int) cvGetCaptureProperty( v.vc, CV_CAP_PROP_POS_FRAMES );

		// Return to the beginning
		cvSetCaptureProperty( v.vc, CV_CAP_PROP_POS_FRAMES, 0. );
	}
	return v;
}

/*****************************************************************
/*
Summary: Rescales an image to fit the specified minimum dimensions
Parameters:
	1. img: the image to be resized
	2. min_size: the minimum dimension of the image (width or height)
*/
/****************************************************************/
IplImage* rescale_image(IplImage* img, int min_size)
{
	IplImage* tmp = NULL;
	int scale_h, scale_w;
	if(img->width >= img->height) 
	{
		scale_h = min_size;
		scale_w = scale_h * (((double) img->width)/img->height);
	}
	else
	{
		scale_w = min_size;
		scale_h = scale_w * (((double) img->height)/img->width);
	}
	tmp = cvCreateImage(cvSize(scale_w, scale_h), 8, 3);
	cvResize(img, tmp, CV_INTER_LINEAR);
	return tmp;
}

/***********************************************************************
/* 
Summary: Find distinctive feature matches between all pairs of images
Parameters:
	1.refImg	: array of reference image structures with img, feat, n
	and kd_root fields initialized.
	2. testImg	: test image/frame initialized
*/
/**********************************************************************/
void Find_Feature_Matches(struct img_struct *refImg, struct img_struct &testImg)
{
	struct feature** nbrs, *f;
	double d0, d1;

	if(bVerbose)	fprintf(stderr, "\nMatching Features..");
	for(int i = 0; i < REF_IMG_COUNT; i++)
	{
		if(bVerbose)	fprintf(stderr, "\nFinding matches with Reference #%i", i+1);
		fflush(stderr);
		for(int k = 0; k < testImg.n; k++)
		{
			f = testImg.feat + k;

			/* Use Lowe's 2NN heuristic to find distinctive feature matches */
			int m = kdtree_bbf_knn(refImg[i].kd_root, f, 2, &nbrs, refImg[i].n/4);
			if( m == 2 )
			{
				d0 = descr_dist_sq(f, nbrs[0]);
				d1 = descr_dist_sq(f, nbrs[1]);
				if( d0 < d1 * NN_SQ_DIST_RATIO_THR )
				{
					f->match[i] = nbrs[0];
					testImg.m[i]++;

					if(bDrawMatches)
					{
						MatchPoint mp;
						mp.test = Point(f->x, f->y);
						mp.ref = Point(nbrs[0]->x, nbrs[0]->y);

						switch(i)
						{
						case 0:
							good_L_matches.push_back(mp);
							break;
						case 1:
							good_R_matches.push_back(mp);
							break;
						case 2:
							good_M_matches.push_back(mp);
							break;
						}
					}
				}
			}
			if(nbrs)
				free(nbrs);
		}
	}
	if(bVerbose)	fprintf( stderr, "... done\n" );
}

/****************************************************
/*
Summary: Computation time and Performance monitor
*/
/***************************************************/
void StartTimer()
{
	if(bShowTimer)
		Start = omp_get_wtime();
}

void StopTimerDisplayElapsedTime(string stimer)
{
	if(bShowTimer)
	{
		double End = omp_get_wtime();
		cout<<"\n"<<stimer<<" :"<<End - Start;
	}
}

/****************************************************
/*
Summary: Stitch Reference Image and Test Image
*/
/***************************************************/
void StitchReferenceAndTest(Mat refImg, Mat testImg, int width, int height, vector<MatchPoint> matches, int sides)
{	
	Mat imgRefT(height, 2 * width, CV_8UC3);
	Mat left(imgRefT, Rect(0, 0, width, height));
	refImg.copyTo(left);
	Mat right(imgRefT, Rect(width, 0, width, height));
	testImg.copyTo(right);

	MatchPoint mp;
	Point p;
	int matchSize = (int)matches.size();
	for(int i = 0; i < matchSize; i++)
	{
		mp = matches[i];
		circle(imgRefT, mp.ref, 2, Scalar(0,255,0), 2, 8, 0);
		p.x = mp.test.x + width;
		p.y = mp.test.y;
		line(imgRefT, mp.ref, p, Scalar(0,0,255), 2, 4);
		circle(imgRefT, p, 2, Scalar(0,255,0), 2, 8, 0);
	}

	string strName;
	switch(sides)
	{
	case 0:
		strName = "Left-Reference_vs_Test Image";
		break;
	case 1:
		strName = "Right-Reference_vs_Test Image";
		break;
	case 2:
		strName = "Mid-Reference_vs_Test Image";
		break;
	}
	cvNamedWindow(strName.c_str(), CV_WINDOW_NORMAL);
	cvResizeWindow(strName.c_str(), 700, 400);
	imshow(strName.c_str(), imgRefT);
	
	if(bWrite2File)
	{
		Mat imgMat(imgRefT);
		imwrite(strName + ".jpg", imgRefT);
	}
}

void StitchReferenceAndTestEx(Mat refImg, Mat testImg, int width, int height, vector<MatchPoint> matches, int sides)
{	
	vector<Point2f> obj;
	vector<Point2f> scene;

	Mat imgRefT(refImg.size().height, testImg.size().width + refImg.size().width, CV_8UC3);
	Mat left(imgRefT, Rect(0, 0, refImg.size().width, refImg.size().height));
	refImg.copyTo(left);
	Mat right(imgRefT, Rect(refImg.size().width, 0, testImg.size().width, testImg.size().height));
	testImg.copyTo(right);

	MatchPoint mp;
	Point p;
	int matchSize = (int)matches.size();
	for(int i = 0; i < matchSize; i++)
	{
		mp = matches[i];
		circle(imgRefT, mp.ref, 2, Scalar(0,255,0), 2, 8, 0);
		p.x = mp.test.x + refImg.size().width;
		p.y = mp.test.y;
		line(imgRefT, mp.ref, p, Scalar(0,0,255), 2, 4);
		circle(imgRefT, p, 2, Scalar(0,255,0), 2, 8, 0);

		obj.push_back(mp.test);
		scene.push_back(mp.ref);
	}

	CvSize refSize;
	refSize.height = refImg.size().height;
	refSize.width = refImg.size().width;

	Mat output(refImg.size().height, refImg.size().width, CV_8UC3);
	Mat H = findHomography( obj, scene, RANSAC );
	warpPerspective( testImg, output, H, refSize, INTER_LINEAR, BORDER_CONSTANT, cvScalarAll(0));
	imwrite("Transformed.jpg", output);

	// *************
	Mat mask(testImg.size().height, testImg.size().width, CV_8UC1, Scalar(1));
	Mat frame_mask(refSize.height, refSize.width, CV_8UC1, Scalar(0));
	warpPerspective( mask, frame_mask, H, refSize, INTER_LINEAR, BORDER_CONSTANT, cvScalarAll(0));

	int scale, s;
	Vec3b val1, val2;
	for(int i = 0; i < refSize.height; i++)
	{
		for(int j = 0; j < refSize.width; j++)
		{
			s = (int)frame_mask.at<uchar>(i,j);//cvGet2D(frame_mask, i, j).val[0];
			if(s)
			{
				val1 = refImg.at<Vec3b>(i,j);
				val2 = output.at<Vec3b>(i,j);

				val1.val[0] = val1.val[0] * (1-s) + val2.val[0] * s;
				val1.val[1] = val1.val[1] * (1-s) + val2.val[1] * s;
				val1.val[2] = val1.val[2] * (1-s) + val2.val[2] * s;
				//cvSet2D( pano, i, j, val1 );
				//mat.at<uchar>(row, column, channel) = val;
				refImg.at<Vec3b>(i,j) = val1;
			}
		}
	}

	imwrite("Final_Pano.jpg", refImg);

	//******************

	string strName;
	switch(sides)
	{
	case 0:
		strName = "Left-Reference_vs_Test Image";
		break;
	case 1:
		strName = "Right-Reference_vs_Test Image";
		break;
	case 2:
		strName = "Mid-Reference_vs_Test Image";
		break;
	}
	cvNamedWindow(strName.c_str(), CV_WINDOW_NORMAL);
	cvResizeWindow(strName.c_str(), 700, 400);
	imshow(strName.c_str(), imgRefT);
	
	if(bWrite2File)
	{
		Mat imgMat(imgRefT);
		imwrite(strName + ".jpg", imgRefT);
	}
}

/*****************************************************
* Summary: Rectify frame on Panorama 
/*****************************************************/
void RectifyFrameOnPano(IplImage *pano, IplImage *frame, vector<MatchPoint> matches)
{
	vector<Point2f> obj;
	vector<Point2f> scene;

	MatchPoint mp;
	int matchSize = (int)matches.size();
	for(int i = 0; i < matchSize; i++)
	{
		obj.push_back(mp.test);
		scene.push_back(mp.ref);
	}

	CvSize refSize;
	refSize.height = pano->height;
	refSize.width = pano->width;
	cvSaveImage("Frame.jpg", frame, NULL );
	Mat H = findHomography(obj, scene, RANSAC);
	CvMat HH = H;
	IplImage *output = cvCreateImage(refSize, 8, 3);
	cvWarpPerspective(frame, output, &HH, CV_INTER_LINEAR, cvScalarAll(0));
	cvSaveImage("Frame_Output.jpg", output, NULL );

	IplImage *mask = cvCreateImage( cvGetSize(frame), 8, 1 );
	cvSet( mask, cvScalarAll(1), NULL );
	IplImage *frame_mask = cvCreateImage(refSize, 8, 1);
	cvWarpPerspective( mask, frame_mask, &HH, CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
	
	double scale, s;
	CvScalar val1, val2;
	for(int i = 0; i < pano->height; i++)
	{
		for(int j = 0; j < pano->width; j++)
		{
			s = cvGet2D(frame_mask, i, j).val[0];
			if(s)
			{
				val1 = cvGet2D( pano, i, j );
				val2 = cvGet2D( output, i, j );

				val1.val[0] = val1.val[0] * (1-s) + val2.val[0] * s;
				val1.val[1] = val1.val[1] * (1-s) + val2.val[1] * s;
				val1.val[2] = val1.val[2] * (1-s) + val2.val[2] * s;
				cvSet2D( pano, i, j, val1 );
			}
		}
	}

	cvSaveImage("OverlayTest.jpg", pano, NULL );
}

void RectifyFrameOnPanoEx(Mat pano, Mat testImg, vector<MatchPoint> matches)
{
	vector<Point2f> obj;
	vector<Point2f> scene;

	MatchPoint mp;
	int matchSize = (int)matches.size();
	for(int i = 0; i < matchSize; i++)
	{
		obj.push_back(mp.test);
		scene.push_back(mp.ref);
	}

	CvSize refSize;
	refSize.height = pano.size().height;
	refSize.width = pano.size().width;
	
	Mat output(refSize.height, refSize.width, CV_8UC3);;
	Mat H = findHomography(obj, scene, RANSAC);
	warpPerspective( testImg, output, H, refSize, INTER_LINEAR, BORDER_CONSTANT, cvScalarAll(0));
	imwrite("Transformed.jpg", output);

	Mat mask(testImg.size().height, testImg.size().width, CV_8UC1, Scalar(1));
	Mat frame_mask(refSize.height, refSize.width, CV_8UC1, Scalar(0));
	warpPerspective( mask, frame_mask, H, refSize, INTER_LINEAR, BORDER_CONSTANT, cvScalarAll(0));

	int scale, s;
	Vec3b val1, val2;
	for(int i = 0; i < refSize.height; i++)
	{
		for(int j = 0; j < refSize.width; j++)
		{
			s = (int)frame_mask.at<uchar>(i,j);//cvGet2D(frame_mask, i, j).val[0];
			if(s)
			{
				val1 = pano.at<Vec3b>(i,j);
				val2 = output.at<Vec3b>(i,j);

				val1.val[0] = val1.val[0] * (1-s) + val2.val[0] * s;
				val1.val[1] = val1.val[1] * (1-s) + val2.val[1] * s;
				val1.val[2] = val1.val[2] * (1-s) + val2.val[2] * s;
				//cvSet2D( pano, i, j, val1 );
				//mat.at<uchar>(row, column, channel) = val;
				pano.at<Vec3b>(i,j) = val1;
			}
		}
	}

	imwrite("Final_Pano.jpg", pano);
}

/***********************************************************
/*
Summary: Draw points that match in reference and test images
Parameters:
	1. lRefImg	: Left reference image
	2. rRefImg	: Right reference image
	3. iTestImg	: Test image
*/
/***********************************************************/
void DrawMatchingPoints(img_struct refImgs[], IplImage *iTestImg)
{
	Mat lImg(refImgs[0].img);
	Mat rImg(refImgs[1].img);

	Mat testImg(iTestImg);
	int width = lImg.size().width;
	int height = lImg.size().height;

	StitchReferenceAndTest(lImg, testImg, width, height, good_L_matches, 0);
	//StitchReferenceAndTestEx(lImg, testImg, width, height, good_L_matches, 0); - Works
	//RectifyFrameOnPano(refImgs[0].img, iTestImg, good_L_matches);
	//RectifyFrameOnPanoEx(lImg, testImg, good_L_matches);
	StitchReferenceAndTest(rImg, testImg, width, height, good_R_matches, 1);

	if(REF_IMG_COUNT > 2)
	{
		Mat mImg(refImgs[2].img);
		//StitchReferenceAndTest(mImg, testImg, width, height, good_M_matches, 2);
	}
	
	cvWaitKey(0);
}

/***********************************************************
/*
Summary: Draw the detected SIFT features on the image
Parameters:
	1. img	: image
	2. feat	: SIFT points
	3. n	: number of features
	4. idx	: index
	5. bTest: Is the image the test image
*/
/***********************************************************/
void DrawFeatures(IplImage* img, feature *feat, int n, int idx, bool bTest)
{
	draw_features(img, feat, n);
	if(!bTest)
		idx==0? cvShowImage("Left-Reference", img): cvShowImage("Right-Reference", img);
	else
		cvShowImage("Test-Image", img);
}

/***********************************************************
/*
Summary: Extract SIFT features and build kd tree (PageRank)
*/
/***********************************************************/
void ExtractFeaturesBuildTree(img_struct &testImg, IplImage *tmp)
{
	CvSize size;
	IplImage *sTmp = NULL;
	if(bRescale)
	{
		sTmp = rescale_image(tmp, rescaleMinSize);
		size.height = sTmp->height;
		size.width = sTmp->width;
		testImg.img = cvCreateImage(size, 8, 3);
		cvCopy(sTmp, testImg.img, NULL);
	}
	else
	{
		size.height = tmp->height;
		size.width = tmp->width;
		testImg.img = cvCreateImage(size, 8, 3);
		cvCopy(tmp, testImg.img, NULL);
	}
	testImg.n = sift_features(testImg.img, &(testImg.feat));
	testImg.kd_root = kdtree_build(testImg.feat, testImg.n);
	cvReleaseImage(&sTmp);
}

/***********************************************************
/*
Summary: Insert key value pair into the map (PageRank)
*/
/***********************************************************/
void Insert_Mapping(long frame, int IndexKey) 
{
	size_t index = 0;
    map<long, int>::const_iterator i = frame2Index.find(frame);
    if (i == frame2Index.end()) 
	{
        frame2Index.insert(pair<long, int>(frame, IndexKey));
    }
}

/****************************************************************************************
/*
Summary: If matched features exceeds threshold then consider the image matches (PageRank)
*/
/***************************************************************************************/
bool PairMatch(img_struct t1, img_struct t2, int &match)
{
	pairMatchCounter++;
	int matchThreshold = 30;
	// Ref: t1; Query: t2
	t2.m[0] = 0;
	struct feature** nbrs, * f;
	double d0, d1;

	if(bVerbose)	fprintf(stderr, "\nMatching Features..");
	for(int k = 0; k < t2.n; k++)
	{
		f = t2.feat + k;

		/* Use Lowe's 2NN heuristic to find distinctive feature matches */
		int m = kdtree_bbf_knn(t1.kd_root, f, 2, &nbrs, t1.n/4);
		if( m == 2 )
		{
			d0 = descr_dist_sq(f, nbrs[0]);
			d1 = descr_dist_sq(f, nbrs[1]);
			if( d0 < d1 * NN_SQ_DIST_RATIO_THR )
			{
				//f->match[0] = nbrs[0];
				t2.m[0]++;
			}
		}
		if(nbrs)
			free(nbrs);
	}

	match = t2.m[0];
	cout<<t2.m[0]<<" ";
	if(bVerbose)	fprintf( stderr, "... done\n" );
	if(t2.m[0] < matchThreshold)
		return false;
	return true;
}

/***********************************************************
/*
Summary: Automatic selection of reference frames (PageRank)
*/
/***********************************************************/
void SelectReferenceFrames(int nFrames, Video v)
{
	cout<<"\nStarting.."<<strCurGame;
	ofstream prFile;
	prFile.open(strPRInputFile);
	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	for(int i = 0; i < nFrames; i++)
	{
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + intervalSize * i;
		Insert_Mapping(frame1, i);
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);

		for(int j = i+1; j < nFrames; j++)
		{
			cout<<"\n#"<<i<<" vs "<<"#"<<j<<": ";
			// Set frame #, extract frame, extract features, build k-d tree
			frame2 = start_frame + intervalSize * j;
			Insert_Mapping(frame2, j);
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
			tmp2 = cvQueryFrame(v.vc);
			ExtractFeaturesBuildTree(testImg2, tmp2);

			if(PairMatch(testImg1, testImg2, matches))
			{
				prFile<<i<<" "<<j<<endl; //" "<<matches<<endl;
				prFile<<j<<" "<<i<<endl; //" "<<matches<<endl;
			}
			
			// Free memory
			cvReleaseImage(&testImg2.img);
			//cvReleaseImage(&tmp2);
			free(testImg2.feat);
			kdtree_release(testImg2.kd_root);
		}

		// Free memory
		cvReleaseImage(&testImg1.img);
		//cvReleaseImage(&tmp1);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
	}
}

/**********************************************************************************
/*
Summary: Automatic selection of reference frames from sequence of images (PageRank)
*/
/**********************************************************************************/
void SelectReferenceFramesFromImageSequence(int nFrames)
{
	cout<<"\nStarting.."<<strCurGame;
	ofstream prFile;
	prFile.open(strPRInputFile);
	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	string sFullPath = "E:\\Research\\Projects\\CourtSideIdentifier\\Release\\Sequence\\";
	char* buff = new char[5];
	char* buff1 = new char[5];
	for(int i = 1; i <= nFrames; i++)
	{
		sprintf(buff, "%d.jpg", i);
		string fName = buff;
		//free(buff);
		tmp1 = cvLoadImage((sFullPath + fName).c_str(), 1);
		ExtractFeaturesBuildTree(testImg1, tmp1);

		for(int j = i+1; j <= nFrames; j++)
		{
			cout<<"\n#"<<i<<" vs "<<"#"<<j<<": ";
			sprintf(buff1, "%d.jpg", j);
			string fName1 = buff1;
			//free(buff1);
			tmp2 = cvLoadImage((sFullPath + fName1).c_str(), 1);
			ExtractFeaturesBuildTree(testImg2, tmp2);

			if(PairMatch(testImg1, testImg2, matches))
			//PairMatch(testImg1, testImg2, matches);
			{
				prFile<<i<<" "<<j<<endl; //" : "<<matches<<endl; //" "<<matches<<endl;
				prFile<<j<<" "<<i<<endl; //" : "<<matches<<endl; //" "<<matches<<endl;
			}
			
			// Free memory
			cvReleaseImage(&testImg2.img);
			//cvReleaseImage(&tmp2);
			free(testImg2.feat);
			kdtree_release(testImg2.kd_root);
		}

		// Free memory
		cvReleaseImage(&testImg1.img);
		//cvReleaseImage(&tmp1);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
	}
}

/*********************************************
/*
Summary: Print Frame and Index pair (PageRank)
*/
/********************************************/
void PrintFrame2Index()
{
	ofstream f2iFile;
	f2iFile.open("10_Frame2Index_Game#3_i60_75.txt");
	typedef std::map<long, int>::iterator it_type;
	for(it_type iterator = frame2Index.begin(); iterator != frame2Index.end(); iterator++)
	{
		f2iFile<<iterator->first<<" "<<iterator->second<<endl;
	}
}

/*******************************************************
/*
Summary: Compute the page rank for each frame (PageRank)
*/
/******************************************************/
void PageRank()
{
	Table t;
	t.set_delim(" ");
	t.print_params(cerr);
	t.read_file(strPRInputFile);
	t.pagerank();
	int max = t.print_pagerank_v();
}

/*******************************************************
/*
Summary: Extract the best frame from all the clusters.
*/
/********************************************************/
void ExtractBestFrameFromClusters(FrameClusters fc, int frameStride, Video v)
{
	bool bDebug = true;

	cout<<"\nExtracting the best frame from the clusters...."<<endl;
	int clusterId = -1;
	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;

	ofstream resFile; // Debug file handle
	resFile.open("Result.txt");

	// The two largest clusters will be with end-of-court frames
	for(int i = 0; i < 2; i++)
	{
		// Array Index to Cluster Index map
		std::map<int, int> idx2CIdx;
		//int **f2FMatches;
		int *avgMatches;
		int clusterSize = fc.sortedCluster[i].second;
		//f2FMatches = new int*[clusterSize];
		avgMatches = new int[clusterSize];
		clusterId = fc.sortedCluster[i].first;

		// Initialize f2FMatches to 0
		for(int j = 0; j < clusterSize; j++)
		{
			//f2FMatches[j]= new int[clusterSize];
			//f2FMatches[j][j] = 0;
			avgMatches[j] = 0;
		}

		cout<<"\Selecting best frame for Cluster "<<fc.sortedCluster[i].first<<"..."<<endl;
		int oIdx = 0;	// Outer Index
		for(int j = 0; j < fc.frame2Cluster.size(); j++)
		{			
			if(fc.frame2Cluster[j] == clusterId)
			{
				idx2CIdx.insert(pair<int, int>(oIdx, j));
				// Set frame #, extract frame, extract features, build k-d tree
				frame1 = start_frame + frameStride * j;
				cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
				tmp1 = cvQueryFrame(v.vc);
				ExtractFeaturesBuildTree(testImg1, tmp1);

				int iIdx = oIdx + 1;	// Inner Index
				for(int k = j+1; k < fc.frame2Cluster.size(); k++)
				{
					if(fc.frame2Cluster[k] == clusterId && k != j)
					{
						if(bDebug)	cout<<"\n#"<<j<<" vs "<<"#"<<k<<": ";

						// Set frame #, extract frame, extract features, build k-d tree
						frame2 = start_frame + frameStride * k;
						cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
						tmp2 = cvQueryFrame(v.vc);
						ExtractFeaturesBuildTree(testImg2, tmp2);

						// Find matching features
						PairMatch(testImg1, testImg2, matches);

						//f2FMatches[oIdx][iIdx] = f2FMatches[iIdx][oIdx] = matches;
						
						avgMatches[oIdx] += matches;
						avgMatches[iIdx] += matches;

						// Free memory
						cvReleaseImage(&testImg2.img);
						free(testImg2.feat);
						kdtree_release(testImg2.kd_root);
						iIdx++;
					}
				}

				// Free memory
				cvReleaseImage(&testImg1.img);
				free(testImg1.feat);
				kdtree_release(testImg1.kd_root);
				oIdx++;
			}
		}

		if(bDebug)	cout<<"\nFind max index: "<<endl;
		int maxIndex = 0;
		for(int j = 0; j < clusterSize; j++)
		{
			if(bDebug)	cout<<"\nTotal #"<<j<<": "<<avgMatches[j];
			if(avgMatches[j] > avgMatches[maxIndex])
				maxIndex = j;
		}
		if(bDebug)	cout<<"\nMax Index (Array): "<<maxIndex<<endl;

		//cout<<"\nTotal matches: "<<endl;
		//for(int j = 0; j < clusterSize; j++)
		//{
		//	int sum = 0;
		//	for(int k = 0; k < clusterSize; k++)
		//	{
		//		sum += f2FMatches[j][k];
		//	}
		//	cout<<"\nSum #"<<j<<": "<<sum;
		//}

		if(bDebug)	cout<<"\nBest Frame for Cluster "<<fc.sortedCluster[i].first<<": "<<idx2CIdx[maxIndex]<<endl;
		resFile<<"\nBest Frame for Cluster "<<fc.sortedCluster[i].first<<": "<<idx2CIdx[maxIndex];

		ofstream debugFile; // Debug file handle
		if(bDebug)
		{
			debugFile.open("Index2CIndex.txt");
			typedef std::map<int, int>::iterator it_type;
			for(it_type iterator = idx2CIdx.begin(); iterator != idx2CIdx.end(); iterator++)
			{
				debugFile<<iterator->first<<" "<<iterator->second<<endl;
			}
			debugFile.close();
		}
	}
	resFile.close();
	cout<<"\nDone"<<endl;
}

void ExtractBestFrameFromClusters(vector<FrameOpticalFlow> fc, int frameStride, Video v)
{
	bool bDebug = true;

	cout<<"\nExtracting the best frame from the cluster...."<<endl;
	int clusterId = -1;
	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;

	ofstream resFile; // Debug file handle
	resFile.open("Result.txt", std::ios_base::app);

	// The two largest clusters will be with end-of-court frames
	// Array Index to Cluster Index map
	std::map<int, int> idx2CIdx;
	int *avgMatches;
	int clusterSize = (int)fc.size();
	avgMatches = new int[clusterSize];

	// Initialize avgMatches to 0
	for(int j = 0; j < clusterSize; j++)
		avgMatches[j] = 0;

	int oIdx = 0;	// Outer Index
	for(int j = 0; j < clusterSize; j++)
	{			
		idx2CIdx.insert(pair<int, int>(oIdx, fc[j].fIndex));
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + frameStride * fc[j].fIndex;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);

		int iIdx = oIdx + 1;	// Inner Index
		for(int k = j+1; k < clusterSize; k++)
		{
			if(bDebug)	cout<<"\n#"<<fc[j].fIndex<<" vs "<<"#"<<fc[k].fIndex<<": ";

			// Set frame #, extract frame, extract features, build k-d tree
			frame2 = start_frame + frameStride * fc[k].fIndex;
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
			tmp2 = cvQueryFrame(v.vc);
			ExtractFeaturesBuildTree(testImg2, tmp2);

			// Find matching features
			PairMatch(testImg1, testImg2, matches);
						
			avgMatches[oIdx] += matches;
			avgMatches[iIdx] += matches;

			// Free memory
			cvReleaseImage(&testImg2.img);
			free(testImg2.feat);
			kdtree_release(testImg2.kd_root);
			iIdx++;
		}
		
		// Free memory
		cvReleaseImage(&testImg1.img);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
		oIdx++;
	}

	if(bDebug)	cout<<"\nFind max index: "<<endl;
	int maxIndex = 0;
	for(int j = 0; j < clusterSize; j++)
	{
		if(bDebug)	cout<<"\nTotal #"<<j<<": "<<avgMatches[j];
		if(avgMatches[j] > avgMatches[maxIndex])
			maxIndex = j;
	}

	if(bDebug)	cout<<"\nMax Index (Array): "<<maxIndex<<endl;
	if(bDebug)	cout<<"\nBest Frame for Cluster: "<<idx2CIdx[maxIndex]<<endl;
	resFile<<"\nBest Frame for Cluster: "<<idx2CIdx[maxIndex];

	resFile.close();
	cout<<"\nDone"<<endl;
}

/*************************************************************************
/*
Summary: Compares the value of the pair/map data structure called by sort
Parameters:
	1. c1: First cluster
	2. c2: Secound cluster
*/
/************************************************************************/
bool clusterSizeCompare(std::pair<int,int> c1, std::pair<int,int> c2)
{
	if(c1.second > c2.second)
		return true;
	return false;
}

/*************************************************************************
/*
Summary: Cluster frames into as many possible clusters.
Two frames belong to the same cluster if the number of matched features
is greater or equal to the "matchThreshold".
- The two largest clusters are the desired clusters with side court frames
Parameters:
	1. nFrames:	Number of frames
	2. v: video object.
*/
/*************************************************************************/
FrameClusters ClusterFrames(int nFrames, Video v)
{
	StartTimer();
	cout<<"\nStart clustering..."<<endl;
	bool bDebug = false;

	ofstream prFile;
	if(bDebug)	prFile.open("Matches_Signal.txt", ios::app);

	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	int itrvlSize = 30;
	FrameClusters FC;

	std::map<int, int> frame2Cluster;
	std::map<int, int> clusterSize;

	for(int i=0; i < nFrames; i++)
	{
		frame2Cluster.insert(pair<int, int>(i, -1));
	}

	char buff[50];
	int matchThreshold = 80;

	int clusterId = 0;
	for(int i = 0; i < nFrames; i++)
	{
		if(frame2Cluster[i] == -1)
		{
			frame2Cluster[i] = ++clusterId;
			clusterSize.insert(pair<int, int>(clusterId, 1));
		}
		else continue;

		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + itrvlSize * i;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);
		
		if(bDebug)	
		{
			prFile<<i<<":\n";
			sprintf(buff, "F%02d.jpg", i);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		for(int j = 0; j < nFrames; j++)
		{
			if(i != j)
			{
				if(bDebug)	cout<<"\n#"<<i<<" vs "<<"#"<<j<<": ";
				// Set frame #, extract frame, extract features, build k-d tree
				frame2 = start_frame + itrvlSize * j;
				cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
				tmp2 = cvQueryFrame(v.vc);
				ExtractFeaturesBuildTree(testImg2, tmp2);

				PairMatch(testImg1, testImg2, matches);

				if(matches >= matchThreshold)
				{
					if(frame2Cluster[j] == -1)
					{
						frame2Cluster[j] = frame2Cluster[i];
						clusterSize[clusterId] = clusterSize[clusterId]+1;
						
						if(bDebug)
						{
							sprintf(buff, "F%02d.jpg", j);
							string fName = buff;
							cvSaveImage(fName.c_str(), tmp2);
						}
					}
				}

				if(bDebug)	prFile<<j<<" "<<matches<<"\n";

				// Free memory
				cvReleaseImage(&testImg2.img);
				//cvReleaseImage(&tmp2);
				free(testImg2.feat);
				kdtree_release(testImg2.kd_root);
			}
		}

		// Free memory
		cvReleaseImage(&testImg1.img);
		//cvReleaseImage(&tmp1);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
		
		if(bDebug)	prFile<<"\n\n";
	}
	prFile.close();

	ofstream debugFile; // Debug file handle
	if(bDebug)
	{
		debugFile.open("ClustersFrame.txt");
		typedef std::map<int, int>::iterator it_type;
		for(it_type iterator = frame2Cluster.begin(); iterator != frame2Cluster.end(); iterator++)
		{
			debugFile<<iterator->first<<" "<<iterator->second<<endl;
		}
		debugFile.close();
	}

	if(bDebug)
	{
		debugFile.open("ClustersSize.txt");
		typedef std::map<int, int>::iterator it_type;
		for(it_type iterator = clusterSize.begin(); iterator != clusterSize.end(); iterator++)
		{
			debugFile<<iterator->first<<" "<<iterator->second<<endl;
		}
		debugFile.close();
	}

	// Find the two clusters with largest size
	std::vector<std::pair<int,int> > sortedCluster(clusterSize.begin(), clusterSize.end());
	std::sort(sortedCluster.begin(), sortedCluster.end(), &clusterSizeCompare);

	if(bDebug)
	{	
		debugFile.open("SortedClustersSize.txt");
		typedef std::vector<std::pair<int,int>>::iterator ittype;
		for(ittype iterator = sortedCluster.begin(); iterator != sortedCluster.end(); iterator++)
		{
			debugFile<<iterator->first<<" "<<iterator->second<<endl;
		}
		debugFile.close();
	}

	if(bDebug)	cout<<"\nIndex of largest Cluster: "<<sortedCluster[0].first<<endl;
	cout<<"\nClustering Done!"<<endl;

	FC.frame2Cluster = frame2Cluster;
	FC.sortedCluster = sortedCluster;

	// Extract best frame from clusters
	ExtractBestFrameFromClusters(FC, itrvlSize, v); 
	StopTimerDisplayElapsedTime("Time to cluster and select best frame");
	cout<<"\nPairWise Calls: "<<pairMatchCounter;
	return FC;
}

void ClusterFramesEx(int nFrames, Video v)
{
	StartTimer();
	cout<<"\nStart clustering..."<<endl;
	bool bDebug = true;

	ofstream prFile;
	if(bDebug)	prFile.open("Matches_Signal.txt", ios::app);

	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	int itrvlSize = 60;
	FrameClusters FC;

	std::map<int, int> frame2Cluster;
	std::map<int, int> clusterSize;

	for(int i=0; i < nFrames; i++)
	{
		frame2Cluster.insert(pair<int, int>(i, -1));
	}

	char buff[50];
	int matchThreshold = 80;

	int clusterId = 0;
	for(int i = 0; i < nFrames - 1; i++)
	{
		if(frame2Cluster[i] == -1)
		{
			frame2Cluster[i] = ++clusterId;
			clusterSize.insert(pair<int, int>(clusterId, 1));
		}
		else continue;

		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + itrvlSize * i;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);
		
		if(bDebug)	
		{
			sprintf(buff, "A%02d.jpg", i);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		if(bDebug)	cout<<"\n#"<<i<<" vs "<<"#"<<i+1<<": ";
		// Set frame #, extract frame, extract features, build k-d tree
		frame2 = start_frame + itrvlSize * (i+1);
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
		tmp2 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg2, tmp2);

		PairMatch(testImg1, testImg2, matches);

		sprintf(buff, "A%02d.jpg", i+1);
		string fName = buff;
		cvSaveImage(fName.c_str(), tmp2);

		if(bDebug)	prFile<<i<<" "<<matches<<"\n";

		// Free memory
		cvReleaseImage(&testImg2.img);
		free(testImg2.feat);
		kdtree_release(testImg2.kd_root);
		cvReleaseImage(&testImg1.img);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
		
		if(bDebug)	prFile<<"\n\n";
	}
	prFile.close();
	StopTimerDisplayElapsedTime("Time to cluster and select best frame");
}

/************************************
/*
Summary: Compute the Median Magnitude
*/
/***********************************/
float ComputeMedian(vector<float> vectorArray)
{
	int arraySize = (int)vectorArray.size();
	float median = 0.;

	if(arraySize > 0)
	{
		// Sort
		std::sort(vectorArray.begin(), vectorArray.end());

		if(arraySize%2 == 0)
		{
			median = (vectorArray[arraySize/2] + vectorArray[arraySize/2 - 1])/2;
		}
		else
		{
			median = vectorArray[arraySize/2];
		}
	}
	return median;
}

float ComputeCircularMean(vector<float> vectorArray)
{
	return 0;
}

float ComputeCircularVariance(vector<float> vectorArray)
{
	//ofstream dirFile;
	//dirFile.open("DirectionData.txt");

	float meanX = 0;
	float meanY = 0;
	int length = (int)vectorArray.size();
	vector<float> X;
	vector<float> Y;
	for(int i = 0; i < length; i++)
	{
		//dirFile<<vectorArray[i]<<"\n";
		X.push_back(cos(vectorArray[i]));
		meanX += X[i];
		Y.push_back(sin(vectorArray[i]));
		meanY += Y[i];
	}

	meanX = meanX/length;
	meanY = meanY/length;
	//cout<<"\nMean: X="<<meanX<<" "<<" Y="<<meanY<<endl;
	//cout<<"Mean angle: "<<atan2(meanY, meanX)<<endl;
	//dirFile<<"\nMean: "<<mean;

	float varX = 0;
	float varY = 0;
	for(int i = 0; i < length; i++)
	{
		varX += pow((X[i]-meanX), 2);
		varY += pow((Y[i]-meanY), 2);
	}
	varX = varX/length;
	varY = varY/length;

	//cout<<"\nVariance: "<<varX + varY;
	//dirFile<<"\nVariance: "<<var;
	//dirFile.close();
	
	//return varX + varY;

	return atan2(meanY, meanX);

}

/*************************************************************************
/*
Summary: Gets the camera state on the frame relative to the next frame.
Parameters:
	1. dirMed: median of Direction of Optical Flow vectors.
	2. magMed: median of Magnitude of Optical Flow vectors.
	3. lastDir: last observed Camera state.
*/
/*************************************************************************/
int GetCameraState(float dirMed, float magMed, int lastState)
{
	if(magMed < 100)
	{
		return CameraMotion::STILL;
	}
	else 
	{
		if(dirMed > -CV_PI/2 && dirMed < CV_PI/2)
		{
			if(lastState != CameraMotion::STILL)
				return CameraMotion::MOVINGRIGHT;
			else
			{
				if(magMed < 100)
					return lastState;
				return CameraMotion::MOVINGRIGHT;
			}

		}
		else
		{
			if(lastState != CameraMotion::STILL)
				return CameraMotion::MOVINGLEFT;
			else
			{
				if(magMed < 100)
					return lastState;
				return CameraMotion::MOVINGLEFT;
			}
		}
	}
}

int GetCameraStateEx(float dirMed, float magMed, float dirVar)
{
	if(magMed < 100)
	{
		return CameraMotion::STILL;
	}
	else 
	{
		if(dirMed > -CV_PI/2 && dirMed < CV_PI/2)
		{
			return CameraMotion::MOVINGRIGHT;
		}
		else
		{
			return CameraMotion::MOVINGLEFT;
		}
	}
}

/***********************************************************************
/* 
Summary: Find distinctive feature matches between frame1 and frame2
Parameters:
	1. f1	: frame1 image structures with img, feat, n
	and kd_root fields initialized.
	2. f2	: frame 2
*/
/**********************************************************************/
int Find_Feature_Matches(img_struct f1, img_struct f2, float &magMed, float &dirMed, int i, int lastDirection)
{
	bool bTest = false;
	pairMatchCounter++;

	Scalar line_color = CV_RGB(0, 255, 0);
	Scalar cirle_color = CV_RGB(255, 0, 0);
	int line_thickness = 2;

	char buff[50];
	int	max_nn_chk = 4;
	struct feature** nbrs, *f;
	double d0, d1;
	vector<Point2f> obj;
	vector<Point2f> scene;
	vector<Point2f> vectors;
	vector<float> magnitude;
	vector<float> direction;
	float mag, dir;
	float x, y;

	if(bVerbose)	fprintf(stderr, "\nMatching Features: Start");
	fflush(stderr);

	for(int k = 0; k < f2.n; k++)
	{
		f = f2.feat + k;

		/* Use Lowe's 2NN heuristic to find distinctive feature matches */
		int m = kdtree_bbf_knn(f1.kd_root, f, 2, &nbrs, f1.n/max_nn_chk);
		if( m == 2 )
		{
			d0 = descr_dist_sq(f, nbrs[0]);
			d1 = descr_dist_sq(f, nbrs[1]);
			if( d0 < d1 * NN_SQ_DIST_RATIO_THR )
			{
				f->match[0] = nbrs[0];
				f2.m[0]++;

				obj.push_back(Point2f(f->x, f->y));
				scene.push_back(Point2f(nbrs[0]->x, nbrs[0]->y));
				x = nbrs[0]->x - f->x;
				y = nbrs[0]->y - f->y;
				dir = atan2(y, x);
				mag = sqrt(pow(y,2) + pow(x,2));
				vectors.push_back(Point2f(x, y));
				magnitude.push_back(mag);
				direction.push_back(dir);

				cvLine(f1.img, Point2f(nbrs[0]->x, nbrs[0]->y), Point2f(f->x, f->y), line_color, line_thickness, CV_AA, 0 );
				cvCircle(f1.img, Point2f(nbrs[0]->x, nbrs[0]->y), 2, cirle_color, 1, CV_AA, 0);
			}
		}
		if(nbrs)
			free(nbrs);
	}

	if(bVerbose)	fprintf( stderr, "\nMatching Features: Done\n" );

	// Compute Median
	magMed = ComputeMedian(magnitude);
	dirMed = ComputeMedian(direction);

	int hwidth = f1.img->width/2;
	int hheight = f1.img->height/2;
	
	if(bTest)
	{
		float xx = 100 * cos(dirMed);
		float yy = 100 * sin(dirMed);
		Point2f tip = Point2f(hwidth + xx, hheight + yy);
		Point2f end =  Point2f(hwidth, hheight);
		cvLine(f1.img, tip, end, CV_RGB(0, 0, 255), line_thickness, CV_AA, 0 );
		cvCircle(f1.img, end, 2, cirle_color, 1, CV_AA, 0);
		/* Draw arrow tips */
		Point2f p;
		p.x = (int) (tip.x - 12 * cos(dirMed + CV_PI / 4));
		p.y = (int) (tip.y - 12 * sin(dirMed + CV_PI / 4));
		cvLine(f1.img, p, tip, cirle_color, line_thickness, CV_AA, 0);
		p.x = (int) (tip.x - 12 * cos(dirMed - CV_PI / 4));
		p.y = (int) (tip.y - 12 * sin(dirMed - CV_PI / 4));
		cvLine(f1.img, p, tip, cirle_color, line_thickness, CV_AA, 0);
		sprintf(buff, "M%02d.jpg", i);
		string fName = buff;
		cvSaveImage(fName.c_str(), f1.img);
	}

	return GetCameraState(dirMed, magMed, lastDirection);
}

float Find_Feature_MatchesEx(img_struct f1, img_struct f2, float &magMed, float &dirMed, int i, int lastDirection)
{
	bool bTest = false;
	bool bIntersect = false;
	pairMatchCounter++;

	Scalar line_color = CV_RGB(0, 255, 0);
	Scalar cirle_color = CV_RGB(255, 0, 0);
	int line_thickness = 2;

	char buff[50];
	int	max_nn_chk = 4;
	struct feature** nbrs, *f;
	double d0, d1;
	vector<Point2f> obj;
	vector<Point2f> scene;
	vector<Point2f> vectors;
	vector<float> magnitude;
	vector<float> direction;
	float mag, dir, var = 0;
	float x, y;

	if(bVerbose)	fprintf(stderr, "\nMatching Features: Start");
	fflush(stderr);

	for(int k = 0; k < f2.n; k++)
	{
		f = f2.feat + k;

		/* Use Lowe's 2NN heuristic to find distinctive feature matches */
		int m = kdtree_bbf_knn(f1.kd_root, f, 2, &nbrs, f1.n/max_nn_chk);
		if( m == 2 )
		{
			d0 = descr_dist_sq(f, nbrs[0]);
			d1 = descr_dist_sq(f, nbrs[1]);
			if( d0 < d1 * NN_SQ_DIST_RATIO_THR )
			{
				f->match[0] = nbrs[0];
				f2.m[0]++;

				obj.push_back(Point2f(f->x, f->y));
				scene.push_back(Point2f(nbrs[0]->x, nbrs[0]->y));
				x = nbrs[0]->x - f->x;
				y = nbrs[0]->y - f->y;
				dir = atan2(y, x);
				mag = sqrt(pow(y,2) + pow(x,2));
				vectors.push_back(Point2f(x, y));
				magnitude.push_back(mag);
				direction.push_back(dir);

				if(bTest)
				{
					cvLine(f1.img, Point2f(nbrs[0]->x, nbrs[0]->y), Point2f(f->x, f->y), line_color, line_thickness, CV_AA, 0 );
					cvCircle(f1.img, Point2f(nbrs[0]->x, nbrs[0]->y), 2, cirle_color, 1, CV_AA, 0);
					
					if(bIntersect)
					{
						float xx = 1000 * cos(dir);
						float yy = 1000 * sin(dir);
						Point2f tip = Point2f(nbrs[0]->x + xx, nbrs[0]->y + yy);
						Point2f end =  Point2f(nbrs[0]->x, nbrs[0]->y);
						cvLine(f1.img, tip, end, CV_RGB(0, 0, 255), line_thickness, CV_AA, 0 );
					}
				}
			}
		}
		if(nbrs)
			free(nbrs);
	}

	if(bVerbose)	fprintf( stderr, "\nMatching Features: Done\n" );

	// Compute Median
	magMed = ComputeMedian(magnitude);
	dirMed = ComputeMedian(direction);
	//cout<<"\nDirection (Median): "<<dirMed;
	//var = ComputeCircularVariance(direction);

	// For Debug purpose	
	if(bTest)
	{
		int hwidth = f1.img->width/2;
		int hheight = f1.img->height/2;
		float xx = 100 * cos(dirMed);
		float yy = 100 * sin(dirMed);
		Point2f tip = Point2f(hwidth + xx, hheight + yy);
		Point2f end =  Point2f(hwidth, hheight);
		cvLine(f1.img, tip, end, CV_RGB(0, 0, 255), line_thickness, CV_AA, 0 );
		cvCircle(f1.img, end, 2, cirle_color, 1, CV_AA, 0);
		
		/* Median: Draw arrow tips */
		Point2f p;
		p.x = (int) (tip.x - 12 * cos(dirMed + CV_PI / 4));
		p.y = (int) (tip.y - 12 * sin(dirMed + CV_PI / 4));
		cvLine(f1.img, p, tip, cirle_color, line_thickness, CV_AA, 0);
		p.x = (int) (tip.x - 12 * cos(dirMed - CV_PI / 4));
		p.y = (int) (tip.y - 12 * sin(dirMed - CV_PI / 4));
		cvLine(f1.img, p, tip, cirle_color, line_thickness, CV_AA, 0);

		if(bIntersect)
		{
			/* Mean: Draw arrow tips */
			xx = 100 * cos(var);
			yy = 100 * sin(var);
			tip = Point2f(hwidth + 10 + xx, hheight +10 + yy);
			end =  Point2f(hwidth + 10, hheight + 10);
			cvLine(f1.img, tip, end, CV_RGB(255, 0, 0), line_thickness, CV_AA, 0 );
			cvCircle(f1.img, end, 2, cirle_color, 1, CV_AA, 0);

			p.x = (int) (tip.x - 12 * cos(var + CV_PI / 4));
			p.y = (int) (tip.y - 12 * sin(var + CV_PI / 4));
			cvLine(f1.img, p, tip,  CV_RGB(255, 0, 255), line_thickness, CV_AA, 0);
			p.x = (int) (tip.x - 12 * cos(var - CV_PI / 4));
			p.y = (int) (tip.y - 12 * sin(var - CV_PI / 4));
			cvLine(f1.img, p, tip,  CV_RGB(255, 0, 255), line_thickness, CV_AA, 0);
		}

		sprintf(buff, "M%02d.jpg", i);
		string fName = buff;
		cvSaveImage(fName.c_str(), f1.img);
	}

	return var;
}

/* Optical flow magnitudes */
bool compareOFFrame(FrameOpticalFlow f1, FrameOpticalFlow f2)
{
	if(f1.ofMagnitude < f2.ofMagnitude)
		return true;
	return false;
}

/* Extract the frame with the minimum magnitude in the cluster */
int ExtractMinMagnitudeFrameFromCluster(vector<FrameOpticalFlow> cluster)
{
	std::sort(cluster.begin(), cluster.end(), &compareOFFrame);
	cout<<"\nFrame Index: "<<cluster[0].fIndex<<endl;
	return cluster[0].fIndex;
}

/***********************************************************************
/*
Summary: Extract Representative Frame from each Candidate
	- Representative frame: Frame with minimum optical flow magnitude
/***********************************************************************/
void ExtractRepresentativeFrameFromCandidates(vector<Candidate> candidates, vector<FrameOpticalFlow> fOFArray, int itrvlSize)
{
	bool bSelectMin = false;
	vector<FrameOpticalFlow> leftSides;
	vector<FrameOpticalFlow> rightSides;
	for(int i = 0; i < candidates.size(); i++)
	{	
		cout<<"\nCandidate: #"<<i+1;
		if(bSelectMin)
		{
			int minIndex = 0;
			float minMag = 1000;
			for(int j = candidates[i].startIndex; j <= candidates[i].endIndex; j++)
			{
				cout<<"\nFrame Index: #"<<j<<" Mag: "<<fOFArray[j].ofMagnitude;
				if(fOFArray[j].ofMagnitude < minMag)
				{
					minMag = fOFArray[j].ofMagnitude;
					minIndex = j;
				}
			}

			cout<<"\nMinimum Index: "<<minIndex<<endl;
			if(candidates[i].possibleSide == FrameSide::RIGHT)
			{
				cout<<"\nRight Rep";
				rightSides.push_back(fOFArray[minIndex]);
			}
			else
			{
				cout<<"\nLeft Rep";
				leftSides.push_back(fOFArray[minIndex]);
			}
		}
		else
		{
			// Select first and last frame of the candidate
			if(candidates[i].possibleSide == FrameSide::RIGHT)
			{
				cout<<"\nRight Rep";
				rightSides.push_back(fOFArray[candidates[i].startIndex]);
				rightSides.push_back(fOFArray[candidates[i].endIndex]);
			}
			else
			{
				cout<<"\nLeft Rep";
				leftSides.push_back(fOFArray[candidates[i].startIndex]);
				leftSides.push_back(fOFArray[candidates[i].endIndex]);
			}

		}
	}

	cout<<"\nSelect best Right Reference frame";
	autoRefFrames[0] = ExtractFrameSaveAsFile(itrvlSize, SelectBestRepresentativeFrame(rightSides, itrvlSize), true);
	cout<<"\nSelect best Left Reference Frame";
	autoRefFrames[1] = ExtractFrameSaveAsFile(itrvlSize, SelectBestRepresentativeFrame(leftSides, itrvlSize), false);
}

/**********************************************************************/
/*
Summary: Compute Pairwise Optical Flow vectors 
*/
/**********************************************************************/
void PairWiseOpticalFlow(int nFrames, Video v)
{
	StartTimer();
	cout<<"\nStart computing Pairwise Optical Flow ..."<<endl;
	bool bDebug = false;

	// Candidate Counter:
	// Valid Candidates
	// ML -> S -> MR (2 -> 0 -> 1)
	// MR -> S -> ML (1 -> 0 -> 2)
	int candCounter = 0;
	int lastCameraDirection = -1;
	bool bStillFound = false;

	ofstream magFile, dirFile, sideFile;
	//if(bDebug)	
	{
		dirFile.open("Direction.txt");
		magFile.open("Magnitude.txt");
		sideFile.open("Sides.txt");
	}

	//FrameOpticalFlow *fOFArray = new FrameOpticalFlow[nFrames];
	vector<FrameOpticalFlow> fOFArray;
	FrameOpticalFlow temp;
	int last = -1;

	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	int itrvlSize = 60;
	FrameClusters FC;
	char buff[50];

	float direction = 0;
	float magnitude = 0;

	int clusterId = 0;
	for(int i = 0; i < nFrames; i++)
	{
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + itrvlSize * i;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);
		
		if(bDebug)	
		{
			//prFile<<i<<":\n";
			sprintf(buff, "A%02d.jpg", i);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		//if(bDebug)	cout<<"\n#"<<i<<" vs "<<"#"<<i+1<<": ";
		cout<<"\n#"<<i<<" vs "<<"#"<<i+1<<": ";
		
		// Set frame #, extract frame, extract features, build k-d tree
		frame2 = start_frame + itrvlSize * (i+1);
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
		tmp2 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg2, tmp2);
		
		if(bDebug)	
		{
			sprintf(buff, "A%02d.jpg", i+1);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		direction = 0;
		magnitude = 0;
				
		last = temp.cMotion = (CameraMotion)Find_Feature_Matches(testImg1, testImg2, magnitude, direction, i, last);
		temp.fIndex = i;
		temp.ofMagnitude = magnitude;
		fOFArray.push_back(temp);
		cout<<last;

		// Check if a valid candidate
		if(lastCameraDirection == -1 && fOFArray[i].cMotion != 0)
		{
			lastCameraDirection = fOFArray[i].cMotion;
		}
		else
		{
			if(fOFArray[i].cMotion == 0)
				bStillFound = true;
			else
			{
				if(fOFArray[i].cMotion != lastCameraDirection)
				{
					if(bStillFound)
					{
						candCounter++;
						lastCameraDirection = fOFArray[i].cMotion;
						bStillFound = false;
						cout<<"\nCandidate found at: "<<i<<endl;
					}
				}
				else
					bStillFound = false;
			}
		}
		
		//if(bDebug)	
		{
			dirFile<<i<<" "<<direction<<"\n";
			magFile<<i<<" "<<magnitude<<"\n";
			sideFile<<i<<" "<<last<<"\n";
		}

		// Free memory
		cvReleaseImage(&testImg2.img);
		cvReleaseImage(&testImg1.img);
		free(testImg2.feat);
		free(testImg1.feat);
		kdtree_release(testImg2.kd_root);
		kdtree_release(testImg1.kd_root);
	}

	dirFile.close();
	magFile.close();
	sideFile.close();

	// Cluster the frames
	cout<<"\nClustering..."<<endl;
	vector<FrameOpticalFlow> rightCluster;
	vector<FrameOpticalFlow> leftCluster;
	for(int i = 0; i < nFrames; i++)
	{
		bool bFound = false;
		if(fOFArray[i].cMotion == CameraMotion::STILL)
		{
			// Search Backward
			for(int j = i-1; j >= 0; j--)
			{
				if(fOFArray[j].cMotion != CameraMotion::STILL)
				{
					fOFArray[i].side = fOFArray[j].cMotion == CameraMotion::MOVINGRIGHT? FrameSide::RIGHT: FrameSide::LEFT;
					if(fOFArray[j].cMotion == CameraMotion::MOVINGRIGHT)
					{
						rightCluster.push_back(fOFArray[i]);
					}
					else
					{
						leftCluster.push_back(fOFArray[i]);
					}
					bFound = true;
					break;
				}
			}

			if(!bFound)
			{
				// Search Forward
				for(int j = i+1; j < nFrames; j++)
				{
					if(fOFArray[j].cMotion != CameraMotion::STILL)
					{
						fOFArray[i].side = fOFArray[j].cMotion == CameraMotion::MOVINGLEFT? FrameSide::RIGHT: FrameSide::LEFT;
						if(fOFArray[j].cMotion == CameraMotion::MOVINGLEFT)
						{
							rightCluster.push_back(fOFArray[i]);
						}
						else
						{
							leftCluster.push_back(fOFArray[i]);
						}
						bFound = true;
						break;
					}
				}
			}
		}
	}

	cout<<"\nCandidate Counter: "<<candCounter<<endl;
	cout<<"\nLeft Cluster Size: "<<leftCluster.size();
	cout<<"\nRight Cluster Size: "<<rightCluster.size();

	//cout<<"\nRight Cluster:"<<endl;
	//ExtractBestFrameFromClusters(rightCluster, itrvlSize, v);
	//cout<<"\nLeft Cluster:"<<endl;
	//ExtractBestFrameFromClusters(leftCluster, itrvlSize, v);
	cout<<"\nPairWise Counter: "<<pairMatchCounter<<endl;
	StopTimerDisplayElapsedTime("Time to cluster and select best frame");
}

void PairWiseOpticalFlowEx(int numberOfCandidates, Video v)
{
	StartTimer();
	cout<<"\nStart computing Pairwise Optical Flow ..."<<endl;
	bool bDebug = false;

	// Averaging Window
	int windowSize = 1;

	// Candidate Counter:
	// Valid Candidates
	// ML -> S -> MR (2 -> 0 -> 1)
	// MR -> S -> ML (1 -> 0 -> 2)
	int candCounter = 0;
	int lastCameraDirection = -1;
	bool bStillFound = false;

	ofstream magFile, dirFile, sideFile, resFile;
	//if(bDebug)	
	{
		dirFile.open("DirectionAvg_w1_cc4.txt");
		magFile.open("MagnitudeAvg_w1_cc4.txt");
		sideFile.open("SidesAvg_w1_cc4.txt");
		resFile.open("Result_w1_cc4.txt");
	}

	vector<FrameOpticalFlow> fOFArray;
	FrameOpticalFlow temp;
	int last = -1;

	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	int itrvlSize = 60;
	FrameClusters FC;
	char buff[50];

	float direction = 0;
	float magnitude = 0;

	int index = 0;
	while(candCounter < numberOfCandidates)
	{
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + itrvlSize * index;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);
		
		if(bDebug)	
		{
			//prFile<<i<<":\n";
			sprintf(buff, "A%02d.jpg", index);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		cout<<"\n#"<<index<<" vs "<<"#"<<index+1<<": ";
			
		// Set frame #, extract frame, extract features, build k-d tree
		frame2 = start_frame + itrvlSize * (index+1);
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
		tmp2 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg2, tmp2);
		
		if(bDebug)	
		{
			sprintf(buff, "A%02d.jpg", index+1);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		direction = 0;
		magnitude = 0;
				
		Find_Feature_MatchesEx(testImg1, testImg2, magnitude, direction, index, last);
		temp.fIndex = index;
		temp.ofMagnitude = magnitude;
		temp.direction = direction;
		temp.side = FrameSide::UNINIT;
		fOFArray.push_back(temp);

		if(index > windowSize-1)
		{
			//cout<<"\nAverage: ";
			float avgMag = 0;
			float avgDir = 0;
			int idx = index;
			int wCount = 0;
			while(wCount < (2*windowSize + 1))
			{
				//cout<<idx<<" ";
				if(idx<0)
				{
					//avgMag += fOFArray[0].ofMagnitude;
					avgDir += fOFArray[0].direction;
				}
				else
				{
					//avgMag += fOFArray[idx].ofMagnitude;
					avgDir += fOFArray[idx].direction;
				}
				wCount++;
				idx--;
			}
			//fOFArray[i-windowSize].ofMagnitude =  avgMag/(2*windowSize+1);
			fOFArray[index-windowSize].direction =  avgDir/(2*windowSize+1);
			last = fOFArray[index-windowSize].cMotion =  (CameraMotion)GetCameraState(fOFArray[index-windowSize].direction, fOFArray[index-windowSize].ofMagnitude, last);
			//cout<<"\n#"<<index-windowSize<<": "<<fOFArray[index-windowSize].cMotion<<endl; 

			// Check for valid candidate
			if(lastCameraDirection == -1 && fOFArray[index-windowSize].cMotion != 0)
			{
				lastCameraDirection = fOFArray[index-windowSize].cMotion;
			}
			else
			{
				if(lastCameraDirection != -1)
				{
					if(fOFArray[index-windowSize].cMotion == 0)
						bStillFound = true;
					else
					{
						if(fOFArray[index-windowSize].cMotion != lastCameraDirection)
						{
							if(bStillFound)
							{
								candCounter++;
								bStillFound = false;
								lastCameraDirection = fOFArray[index-windowSize].cMotion;
								cout<<"\nCandidate found at: "<<index-windowSize<<endl;
							}
							else
							{
							}
						}
						else
						{
							//lastCameraDirection = fOFArray[index-windowSize].cMotion;
							bStillFound = false;
						}
					}
				}
			}
		}
		
		// Free memory
		cvReleaseImage(&testImg2.img);
		cvReleaseImage(&testImg1.img);
		free(testImg2.feat);
		free(testImg1.feat);
		kdtree_release(testImg2.kd_root);
		kdtree_release(testImg1.kd_root);
		index++;
	}

	for(int i = 0; i < fOFArray.size(); i++)
	{
		//if(bDebug)	
		{
			dirFile<<i<<" "<<fOFArray[i].direction<<"\n";
			magFile<<i<<" "<<fOFArray[i].ofMagnitude<<"\n";
			sideFile<<i<<" "<<fOFArray[i].cMotion<<"\n";
		}
	}
	dirFile.close();
	magFile.close();
	sideFile.close();

	cout<<"\nCandidate Counter: "<<candCounter<<endl;

	// Cluster the frames
	cout<<"\nClustering..."<<endl;
	vector<FrameOpticalFlow> rightCluster;
	vector<FrameOpticalFlow> leftCluster;
	for(int i = 0; i < fOFArray.size(); i++)
	{
		CameraMotion before;
		CameraMotion after;

		if(fOFArray[i].cMotion == CameraMotion::STILL)
		{
			if(i > 0 && fOFArray[i-1].side != FrameSide::UNINIT)
			{
				fOFArray[i].side = fOFArray[i-1].side;
				if(fOFArray[i].side == FrameSide::RIGHT)
				{
					rightCluster.push_back(fOFArray[i]);
				}
				else
				{
					leftCluster.push_back(fOFArray[i]);
				}
				cout<<"\n"<<i<<": "<<fOFArray[i].side;
			}
			else
			{
				// Search Forward
				for(int j = i+1; j < fOFArray.size(); j++)
				{
					if(fOFArray[j].cMotion != CameraMotion::STILL)
					{
						after = fOFArray[j].cMotion;			
						break;
					}
				}

				// Search Backward
				bool bContinueBack = false;
				for(int j = i-1; j >= 0; j--)
				{
					if(fOFArray[j].cMotion != CameraMotion::STILL)
					{
						before = fOFArray[j].cMotion;
						if(after == before)
							bContinueBack = true;
						else	break;
					}
					else if(bContinueBack)	break;
				}

				if(before != after)
				{
					fOFArray[i].side = before == CameraMotion::MOVINGRIGHT? FrameSide::RIGHT: FrameSide::LEFT;
					if(fOFArray[i].side == FrameSide::RIGHT)
					{
						rightCluster.push_back(fOFArray[i]);
					}
					else
					{
						leftCluster.push_back(fOFArray[i]);
					}
					cout<<"\n"<<i<<": "<<fOFArray[i].side;
				}
			}
		}
	}
	cout<<"\nClustering Done!"<<endl;

	cout<<"\nNumber of processed frames: "<<index<<endl;
	cout<<"\nLeft Cluster Size: "<<leftCluster.size();
	cout<<"\nRight Cluster Size: "<<rightCluster.size()<<endl;
	
	resFile<<"\nNumber of processed frames: "<<index<<"\n";
	resFile<<"\nLeft Cluster Size: "<<leftCluster.size();
	resFile<<"\nRight Cluster Size: "<<rightCluster.size()<<"\n";

	cout<<"\nRight Cluster:";
	int fIndex = ExtractMinMagnitudeFrameFromCluster(rightCluster);
	resFile<<"\nRight Cluster Frame Index: "<<fIndex<<"\n";
	frame1 = start_frame + itrvlSize * fIndex;
	cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
	tmp1 = cvQueryFrame(v.vc);
	cvSaveImage("Right.jpg", tmp1);

	cout<<"\nLeft Cluster:";
	fIndex = ExtractMinMagnitudeFrameFromCluster(leftCluster);
	resFile<<"\nRight Cluster Frame Index: "<<fIndex<<"\n";
	frame1 = start_frame + itrvlSize * fIndex;
	cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
	tmp1 = cvQueryFrame(v.vc);
	cvSaveImage("Left.jpg", tmp1);

	cout<<"\nPairWise Counter: "<<pairMatchCounter<<endl;
	resFile<<"\nPairWise Counter: "<<pairMatchCounter<<"\n";
	resFile.close();
	StopTimerDisplayElapsedTime("Time to cluster and select best frame");
}

void PairWiseOpticalFlowIterative(int numberOfCandidates, Video v)
{
	StartTimer();
	cout<<"\nStart computing Pairwise Optical Flow ..."<<endl;
	bool bDebug = false;
	bool bWriteFile = true;
	bool bCountLimit = true;
	int stillLimit = 30;

	// Candidate Counter:
	// Valid Candidates
	// ML -> S -> MR (2 -> 0 -> 1)
	// MR -> S -> ML (1 -> 0 -> 2)
	int candCounter = 0;
	int lastCameraDirection = -1;
	bool bStillFound = false;

	ofstream magFile, dirFile, sideFile, resFile, varFile;
	if(bWriteFile)	
	{
		dirFile.open("Direction_Test.txt");
		magFile.open("Magnitude_Test.txt");
		sideFile.open("SidesAvg_Test.txt");
		resFile.open("Result_Test.txt");
		//varFile.open("Variance_Test.txt");
	}

	vector<FrameOpticalFlow> fOFArray;
	FrameOpticalFlow temp;
	vector<Candidate> candidates;
	Candidate tempCand;
	int last = -1;

	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;
	int itrvlSize = 30;
	FrameClusters FC;
	char buff[50];

	float direction = 0;
	float magnitude = 0;

	int segStartIndex = 0;

	int index = 0;
	int stillCounter = 0;
	int oppMotionCount = 0;

	cout<<"\nSearching for "<<numberOfCandidates<<" Candidates...";
	while(candCounter < numberOfCandidates)
	{
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + itrvlSize * index;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
	
		ExtractFeaturesBuildTree(testImg1, tmp1);
		
		if(bDebug)	
		{
			//prFile<<i<<":\n";
			sprintf(buff, "A%02d.jpg", index);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		cout<<"\n#"<<index<<" vs "<<"#"<<index+1<<": ";
			
		// Set frame #, extract frame, extract features, build k-d tree
		frame2 = start_frame + itrvlSize * (index+1);
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
		tmp2 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg2, tmp2);
		
		if(bDebug)	
		{
			sprintf(buff, "A%02d.jpg", index+1);
			string fName = buff;
			cvSaveImage(fName.c_str(), tmp1);
		}

		direction = 0;
		magnitude = 0;
				
		temp.dirVariance = Find_Feature_MatchesEx(testImg1, testImg2, magnitude, direction, index, last);
		temp.fIndex = index;
		temp.ofMagnitude = magnitude;
		temp.direction = direction;
		temp.side = FrameSide::UNINIT;
		fOFArray.push_back(temp);

		last = fOFArray[index].cMotion =  (CameraMotion)GetCameraStateEx(fOFArray[index].direction, fOFArray[index].ofMagnitude, fOFArray[index].dirVariance);

		// Check for valid candidate
		if(lastCameraDirection == -1 && fOFArray[index].cMotion != 0)
		{
			lastCameraDirection = fOFArray[index].cMotion;
		}
		else
		{
			if(lastCameraDirection != -1)
			{
				if(fOFArray[index].cMotion == 0)
				{
					if(!bStillFound)
					{
						//cout<<"Still found";
						bStillFound = true;
						segStartIndex = index;						
					}
					else
					{
						if(bCountLimit)	stillCounter++;
					}
				}
				else
				{
					if(fOFArray[index].cMotion != lastCameraDirection)
					{
						//cout<<"Opposite direction encountered";
						if(bStillFound)
						{
							//cout<<"\nStill already found";
							oppMotionCount++;
							//cout<<"\nOpp Motion count: "<<oppMotionCount;
							// Find at least 2 frames with opposite Camera Direction
							if(oppMotionCount > 1 && stillCounter > 1)
							{
								lastCameraDirection = fOFArray[index].cMotion;
								if(bCountLimit)
								{
									if(stillCounter > stillLimit)
									{
										// Reset
										cout<<"\n**Beyond Still frames limit**";
										stillCounter = 0;
										oppMotionCount = 0;
										bStillFound = false;
										//break;
										continue;
									}
								}
							
								candCounter++;
								cout<<"\nCandidate #"<<candCounter<<" found at: "<<index - oppMotionCount;
								resFile<<"\nCandidate #"<<candCounter<<": "<<segStartIndex<<" - "<<index - oppMotionCount<<"\n";
								cout<<"\nStart @:"<<segStartIndex<<" "<<"End @:"<<index - oppMotionCount<<endl;
								tempCand.startIndex = segStartIndex;
								tempCand.endIndex = index - oppMotionCount; // include the frame after the still frame
								tempCand.interval = itrvlSize;
								tempCand.possibleSide = lastCameraDirection == CameraMotion::MOVINGLEFT ? FrameSide::RIGHT: FrameSide::LEFT;
								tempCand.bHInit = false;
								candidates.push_back(tempCand);
								
								// Reset
								bStillFound = false;
								oppMotionCount = 0;
								stillCounter = 0;
							}
						}
					}
					else
					{
						//cout<<"Same direction encountered";
						bStillFound = false;
						oppMotionCount = 0;
					}

				}
			}
		}
		
		// Free memory
		cvReleaseImage(&testImg2.img);
		cvReleaseImage(&testImg1.img);
		free(testImg2.feat);
		free(testImg1.feat);
		kdtree_release(testImg2.kd_root);
		kdtree_release(testImg1.kd_root);
		index++;
	}
	cout<"\nCandidates Found!!";

	for(int i = 0; i < fOFArray.size(); i++)
	{
		if(bWriteFile)	
		{
			dirFile<<i<<" "<<fOFArray[i].direction<<"\n";
			magFile<<i<<" "<<fOFArray[i].ofMagnitude<<"\n";
			sideFile<<i<<" "<<fOFArray[i].cMotion<<"\n";
			//varFile<<i<<" "<<fOFArray[i].dirVariance<<"\n";
		}
	}
	dirFile.close();
	magFile.close();
	sideFile.close();
	varFile.close();

	cout<<"\nExtracting Representative Frames from Candidates: "<<candCounter<<endl;
	ExtractRepresentativeFrameFromCandidates(candidates, fOFArray, itrvlSize);
	cout<<"\nDone!!";
	
	resFile.close();
	StopTimerDisplayElapsedTime("Time to cluster and select best frame");
}

/* Indicator function */
int Indicator(float val, int index, int binSize, int minVal)
{
	//if(abs(val - ((index * binSize) + minVal)) < binSize)
	if(val - ((index * binSize) + minVal) < binSize)
		return 1;
	return 0;
}

/* Constructing edge count histogram */
Mat ExtractEdgeCountHistogram(Mat frame)
{
	Mat gray;
	cvtColor(frame, gray, CV_BGR2GRAY);
	int cellSizeX = 128;
	int cellSizeY = 72;
	int cellCols = cvFloor(frame.cols/cellSizeX);
	int cellRows = cvFloor(frame.rows/cellSizeY);
	//cout<<"\nNumber of cells: "<<cellRows<<"X"<<cellCols;

	vector<int> histVector;

	for(int i = 0; i < cellCols; i++)
	{
		int startX = i * cellSizeX;
		for(int j = 0; j < cellRows; j++)
		{
			int startY = j * cellSizeY;
			//cout<<"\nCell: ["<<i<<"],["<<j<<"]";
			// Setup region of interest
			cv::Rect rROI(startX, startY, cellSizeX, cellSizeY);
			cv::Mat roi = gray(rROI);
			Mat result;
			Canny(roi, result, 300, 300, 5, true);
			histVector.push_back(countNonZero(result));
		}
	}
	Mat hist(histVector);
	return hist;
}

/* Compute histogram */
Mat ExtractHistogram(Mat frame)
{
	HistDiff hd = HistDiff(0, 0, 0, false);
	return hd.CalculateHistogram(frame);
}

/* Construct YCbCR color histogram */
void ExtractYCbCrColorHistogram(Mat YCbCrFrame)
{
	bool bOCVHist = true;
	if(bOCVHist)
	{
		HistDiff hd = HistDiff(12, 2, 1, false);
		hd.CalculateHistogram(YCbCrFrame);
		//hd.CalculateDifference(YCbCrFrame,YCbCrFrame);
		return;
	}

	//imwrite("abc.jpg", YCbCrFrame);
	int rangeY = Y_MAX - Y_MIN;
	int rangeCb = Cb_MAX - Cb_MIN;
	int rangeCr = Cr_MAX - Cr_MIN;

	int binsY	= 8;
	int binsCb	= 12;
	int binsCr	= 12;
	int indexY	= 0;
	int indexCb = 0;
	int indexCr = 0;
	Mat hist(binsY * binsCb * binsCr, 1, CV_32SC1, Scalar(0));

	int *h = new int[binsY * binsCb * binsCr];
	for(int i = 0; i < binsY * binsCb * binsCr; i++)
	{
		h[i] = 0;
	}

	//for(int index = 0; index < binsY * binsCb * binsCr; index++)
	//{
	//	cout<<index<<" "<<hist.at<ushort>(index)<<"\n";
	//}
	int wCount = 0;
	int pCount = 0;
	int aCount = 0;
	for(int y = 0; y < YCbCrFrame.rows; y++)
	{
		for(int x = 0; x < YCbCrFrame.cols; x++)
		{
			Vec3b val = YCbCrFrame.at<Vec3b>(Point(x,y));
			//cout<<"\n0: "<<(int)val[0]<<" 1: "<<(int)val[1]<<" 2: "<<(int)val[2]<<endl;
			// Luminance			val[0]

			// Chrominance Blue		val[1]

			// Chrominance Red		val[2]
			int count = 0;
			for(int index = 0; index < binsY * binsCb * binsCr; index++)
			{
				indexY	= (int)index /(binsCb * binsCr);
				indexCb = ((int)index / binsCr) % binsCb;
				indexCr = index % binsCr;
				//cout<<index<<": "<<indexY<<" "<<indexCb<<" "<<indexCr<<endl;
				int a = Indicator((int)val[0], indexY, rangeY/binsY, Y_MIN) * Indicator((int)val[1], indexCb, rangeCb/binsCb, Cb_MIN) * Indicator((int)val[2], indexCr, rangeCr/binsCr, Cr_MIN);
				if(a == 1)
				{		
					aCount++;
					//hist.at<ushort>(index) = (int)hist.at<ushort>(index) + 1; 
					h[index] += 1;
					break;
				}
				count++;
			}
			if(count == binsY * binsCb * binsCr)
			{
				//cout<<"\nWarning: "<<"["<<x<<","<<y<<"]";
				wCount++;
			}

			pCount++;
		}
	}
	
	ofstream histFile;
	histFile.open("Histogram.txt");

	//cout<<"\New:"<<endl;
	int s = 0;
	for(int index = 0; index < binsY * binsCb * binsCr; index++)
	{
		//histFile<<index<<" "<<hist.at<ushort>(index)<<"\n";
		s += h[index];
		histFile<<index<<" "<<h[index]<<"\n";
	}
	cout<<"\nSum Histogram: "<<s<<" Product: "<<YCbCrFrame.rows * YCbCrFrame.cols<<" Sum: "<<pCount<<" A count: "<<aCount;
	cout<<"\nWarning Count: "<<wCount<<endl;
	histFile.close();
}

/* Extract histogram from candidates */
void ExtractHistogramFromCandidates(vector<Candidate> &candidates)
{
	bool bColorHist = true;
	char buff[50];
	bool bWriteHist = true;
	IplImage *temp;
	Mat YCbCrFrame;
	int interval = candidates[0].interval;
	for(int i = 0; i < candidates.size(); i++)
	{	
		cout<<"\nCandidate: #"<<i+1;
		int fCount = 0;
		for(int j = candidates[i].startIndex; j <= candidates[i].endIndex; j++)
		{
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, (start_frame + interval * j));
			temp = cvQueryFrame(v.vc);
			Mat rgbFrame(temp);
			Mat h;
			if(bColorHist)	// Color Histogram
			{   				
				cvtColor(rgbFrame, YCbCrFrame, CV_BGR2YCrCb);
				h = ExtractHistogram(YCbCrFrame);
			}
			else		// Edge Count Histogram
			{
				h = ExtractEdgeCountHistogram(rgbFrame);
			}

			if(!candidates[i].bHInit)
			{
				candidates[i].hist = h;
				candidates[i].bHInit = true;
			}
			else
			{
				candidates[i].hist += h;
			}
			fCount++;
		}
		candidates[i].hist /= fCount;

		if(bWriteHist)
		{
			ofstream histFile;
			sprintf(buff, "HistogramE%02d.txt", i);
			string fName = buff;
			histFile.open(fName);

			for(int index = 0; index < candidates[i].hist.rows; index++)
			{
				histFile<<index<<" "<< candidates[i].hist.at<float>(index)<<"\n";
			}
			histFile.close();
		}
	}
}

/* Extract frame from video and save it as a file */
long ExtractFrameSaveAsFile(int itrvlSize, int fIndex, bool bRight)
{
	IplImage *tmp1 = NULL;
	long frame = start_frame + itrvlSize * fIndex;
	cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame);
	tmp1 = cvQueryFrame(v.vc);
	
	if(bRight)
	{
		cvSaveImage("RightRef.jpg", tmp1);
	}
	else
	{
		cvSaveImage("LeftRef.jpg", tmp1);
	}
	cvReleaseImage(&tmp1);
	return frame;
}

/***********************************************************************
/* 
Summary: Select Best Representative Frame
	- Best Representative frame: Frame with maximum average # matches
/***********************************************************************/
int SelectBestRepresentativeFrame(vector<FrameOpticalFlow> sides, int frameStride)
{
	bool bDebug = true;
	IplImage *tmp1 = NULL;
	IplImage *tmp2 = NULL;
	int matches = 0;
	img_struct testImg1, testImg2;
	long frame1, frame2;

	// The two largest clusters will be with end-of-court frames
	// Array Index to Cluster Index map
	std::map<int, int> idx2CIdx;
	int *avgMatches;
	int length = sides.size();
	avgMatches = new int[length];

	// Initialize avgMatches to 0
	for(int j = 0; j < length; j++)
		avgMatches[j] = 0;

	int oIdx = 0;	// Outer Index
	for(int j = 0; j < length; j++)
	{			
		idx2CIdx.insert(pair<int, int>(oIdx, sides[j].fIndex));
		// Set frame #, extract frame, extract features, build k-d tree
		frame1 = start_frame + frameStride * sides[j].fIndex;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame1);
		tmp1 = cvQueryFrame(v.vc);
		ExtractFeaturesBuildTree(testImg1, tmp1);

		int iIdx = oIdx + 1;	// Inner Index
		for(int k = j+1; k < length; k++)
		{
			if(bDebug)	cout<<"\n#"<<sides[j].fIndex<<" vs "<<"#"<<sides[k].fIndex<<": ";

			// Set frame #, extract frame, extract features, build k-d tree
			frame2 = start_frame + frameStride * sides[k].fIndex;
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, frame2);
			tmp2 = cvQueryFrame(v.vc);
			ExtractFeaturesBuildTree(testImg2, tmp2);

			// Find matching features
			PairMatch(testImg1, testImg2, matches);
						
			avgMatches[oIdx] += matches;
			avgMatches[iIdx] += matches;

			// Free memory
			cvReleaseImage(&testImg2.img);
			free(testImg2.feat);
			kdtree_release(testImg2.kd_root);
			iIdx++;
		}
		
		// Free memory
		cvReleaseImage(&testImg1.img);
		free(testImg1.feat);
		kdtree_release(testImg1.kd_root);
		oIdx++;
	}

	if(bDebug)	cout<<"\nFind max index: "<<endl;
	int maxIndex = 0;
	for(int j = 0; j < length; j++)
	{
		if(bDebug)	cout<<"\nTotal #"<<j<<": "<<avgMatches[j];
		if(avgMatches[j] > avgMatches[maxIndex])
			maxIndex = j;
	}

	if(bDebug)	cout<<"\nMax Index (Array): "<<maxIndex<<endl;
	if(bDebug)	cout<<"\nBest Frame for Cluster: "<<idx2CIdx[maxIndex]<<endl;
	cout<<"\nDone"<<endl;
	return idx2CIdx[maxIndex];
}

/***********************************************************
/*
Summary: Hierarchical Agglomerative Clustering functions
*/
/***********************************************************
/* Merge Clusters */
void MergeClusters(HFileReader &hf, int i, int j)
{
	cout<<"\nMerging Cluster "<<hf.clusters[i].id <<" & "<<hf.clusters[j].id <<endl;
	Cluster mergeCluster;
	mergeCluster.count = hf.clusters[i].count + hf.clusters[j].count;
	mergeCluster.id = hf.clusters[i].id + "_" + hf.clusters[j].id;
	mergeCluster.h = (hf.clusters[i].h + hf.clusters[j].h)/2;
	mergeCluster.candidates.insert(mergeCluster.candidates.begin(), hf.clusters[i].candidates.begin(), hf.clusters[i].candidates.end());
	mergeCluster.candidates.insert(mergeCluster.candidates.end(), hf.clusters[j].candidates.begin(), hf.clusters[j].candidates.end());

	hf.clusters.erase(hf.clusters.begin() + i);
	hf.clusters.erase(hf.clusters.begin() + j-1);
	hf.clusters.push_back(mergeCluster);
	hf.totalHist--;
}

/* Start HAC */
void HierarchicalAggloCluster(HFileReader hf)
{
	int clusters = 3;
	int count = 0;
	int totalClusters = hf.totalHist;
	while(count < totalClusters - clusters)
	{
		float minDist = 1000;
		int mini = 0;
		int minj = 0;
		for(int i = 0; i < hf.totalHist; i++)
		{
			for(int j = i+1; j < hf.totalHist; j++)
			{
				float ed = norm(hf.clusters[i].h, hf.clusters[j].h);
				//cout<<"\n"<<i<<" vs "<<j<<": "<<ed;

				if(minDist > ed)
				{
					minDist = ed;
					mini = i;
					minj = j;
				}
			}
		}
		MergeClusters(hf, mini, minj);
		count++;
	}
}

/* Function to start clustering */
void StartHAC()
{
	HFileReader hf("E:\\Research\\Projects\\CourtSideIdentifier\\G1");
	hf.ExtractHistogramAssignCluster();
	HierarchicalAggloCluster(hf);
}

/************************************************************
/* ======================== MAIN ========================= */
/* Arguments:
	1. -l Left Reference Image
	2. -r Right Reference Image
	3. -m Mid Reference Image
	4. -g Game Identifier
	5. -v Video File Index
	6. -i Interval Size
	7. -s Scale
	8. -sf Start Frame
	9. -ef End Frame

*/
/***********************************************************/
int main(int argc, char** argv)
{
	bool bHAC = false;
	if(bHAC)
	{
		cout<<"\nHierarchical Clustering..."<<endl;
		StartHAC();
		return 0;
	}

	ReadCommandLine(argc, argv);
	v = LoadVideo(gameIdx, videoIdx);
	if(v.vc == NULL)
	{
		fprintf(stderr, "Error: Can't open video.\n");
		return -1;
	}
	
	rescaleMinSize = v.frame_size.height/scaleFactor;

	bool bTakeSnapShot = false;
	if(bTakeSnapShot)
	{
		IplImage *t = NULL;
		cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, 6436);
		t = cvQueryFrame(v.vc);
		Mat f(t);
		cv::imwrite("9_PRFrame_Rank#3.jpg", f);
		return 0;
	}
	
	bool bAutoRef = false;
	if(bAutoRef)
	{
		bool bPageRank = false;
		bool bCluster = false;
		if(bPageRank)
		{
			SelectReferenceFrames(75, v);
			PrintFrame2Index();
			getchar();
		}
		else
		{
			if(bCluster)
				ClusterFrames(30, v);
			else
			{
				PairWiseOpticalFlowIterative(6, v);
			}
		}
		cout<<"\nDone!!"<<endl;
		return 0;
	}
	
	ofstream sideFile, diffFile;
	sideFile.open(strOutputFile);
	diffFile.open(strDiffFile);

	// Load the Reference Images
	struct img_struct refImgs[REF_IMG_COUNT];

	CvSize size;
	IplImage *tmp = NULL;
	IplImage *itmp = NULL;
	for(int i=0; i < REF_IMG_COUNT; i++)
	{
		// Clear Inliers from Reference Images
		refImgs[i].inlier[0] = NULL;

		if(bAutoRef)
		{
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, autoRefFrames[i]);
			itmp = cvQueryFrame(v.vc);
			if (itmp == NULL)
			{
				fprintf(stderr, "Error: Could not retrieve the frame: %d.\n", autoRefFrames[i]);
				return -1;
			}
		}
		else
		{
			itmp = cvLoadImage(img_file[i],1);
			if(!itmp)	fatal_error("unable to load image from %s", img_file[i]);
		}
		
		// Extract features from Reference Images
		if(bRescale)
		{
			tmp = rescale_image(itmp, rescaleMinSize);
			size.height = tmp->height;
			size.width = tmp->width;
			refImgs[i].img = cvCreateImage(size, 8, 3);
			cvCopy(tmp, refImgs[i].img, NULL);
			refImgs[i].n = sift_features(tmp, &(refImgs[i].feat));
		}
		else
		{
			CvSize refSize;
			refSize.height = itmp->height;
			refSize.width = itmp->width;
			refImgs[i].img = cvCreateImage(refSize, 8, 3);
			cvCopy(itmp, refImgs[i].img, NULL);
			refImgs[i].n = sift_features(refImgs[i].img, &(refImgs[i].feat));
		}

		refImgs[i].kd_root = kdtree_build(refImgs[i].feat, refImgs[i].n);
		erase_from_stream(stderr, 1 + digits_in_int(i+1));
		if(bDrawFeatures)
			DrawFeatures(refImgs[i].img, refImgs[i].feat, refImgs[i].n, i, false);

		cvReleaseImage(&itmp);
	}
	
	cout<<"\nProcessing frame from "<<start_frame<<" - "<<end_frame<<" ["<<"Interval: "<<intervalSize<<" Scale: "<<scaleFactor<<"]"<<endl;
	int prcsCount = 0;
	int totalFrames = (end_frame - start_frame)/intervalSize;
	int dispPercent = (totalFrames * 5)/100; // 5%
	dispPercent = dispPercent < 1 ? 1 : dispPercent;
	double timeStart = omp_get_wtime();
	long current_frame = start_frame;
	while(current_frame < end_frame)
	{
		prcsCount++;
		static IplImage *frame = NULL;
		struct img_struct testImg;

		if(bIsFile)
		{
			frame = cvLoadImage("E:\\Research\\Projects\\CourtSideIdentifier\\TestImages\\14.jpg",1);
		}
		else
		{
			// Start from current frame
			cvSetCaptureProperty(v.vc, CV_CAP_PROP_POS_FRAMES, current_frame);

			// Extract the frame
			frame = cvQueryFrame(v.vc);
			if (frame == NULL)
			{
				fprintf(stderr, "Error: Could not retrieve the frame: %d.\n", current_frame);
				return -1;
			}
		}

		// Clear Inliers and Match count
		for(int i = 0; i < REF_IMG_COUNT; i++)
		{
			testImg.inlier[i] = NULL;
			testImg.m[i] = 0;
		}
				
		// Extract SIFT features
		StartTimer();
		if(bRescale)
		{
			tmp = rescale_image(frame, rescaleMinSize);
			size.height = tmp->height;
			size.width = tmp->width;
			testImg.img = cvCreateImage(size, 8, 3);
			cvCopy(tmp, testImg.img, NULL);
			testImg.n = sift_features(tmp, &(testImg.feat));
		}
		else
		{
			testImg.img = cvCreateImage(v.frame_size, 8, 3);
			cvCopy(frame, testImg.img, NULL);
			testImg.n = sift_features(testImg.img, &(testImg.feat));
		}
		StopTimerDisplayElapsedTime("Extract features time");

		if(bDrawFeatures)
			DrawFeatures(testImg.img, testImg.feat, testImg.n, 0, true);

		StartTimer();
		testImg.kd_root = kdtree_build(testImg.feat, testImg.n);
		StopTimerDisplayElapsedTime("Build Tree time");

		// Match with Reference Images
		StartTimer();
		Find_Feature_Matches(refImgs, testImg);
		StopTimerDisplayElapsedTime("Feature Match finding time");
		
		if(bIsFile)		
		{
			cout<<current_frame<<" ";
			if(REF_IMG_COUNT == 2)
				cout<<testImg.m[0] - testImg.m[1]<<endl;
			else
				cout<<testImg.m[0]<<" "<<testImg.m[1]<<" "<<testImg.m[2]<<endl;
		}
		else
		{
			//cout<<current_frame<<" ";
			sideFile<<current_frame<<" ";
			diffFile<<current_frame<<" ";
			if(REF_IMG_COUNT == 2)
			{
				sideFile<<testImg.m[0]<<" "<<testImg.m[1]<<"\n";
				diffFile<<testImg.m[0] - testImg.m[1]<<"\n";
			}
			else // Left Right Mid
			{
				sideFile<<testImg.m[0]<<" "<<testImg.m[1]<<" "<<testImg.m[2]<<"\n";
			}
		}

		if(bDrawMatches)	
			DrawMatchingPoints(refImgs, testImg.img);
		
		// Free memory
		cvReleaseImage(&testImg.img);
		cvReleaseImage(&tmp);
		free(testImg.feat);
		kdtree_release(testImg.kd_root);

		current_frame += intervalSize;
		if(prcsCount%dispPercent == 0)
		{
			if(prcsCount < dispPercent)
				fprintf(stderr, " 1 ");
			else
				fprintf(stderr, " %i ", (prcsCount/dispPercent) *  5);
			fflush(stderr);
		}
	}

	// Free memory used by reference images
	for(int i = 0; i < REF_IMG_COUNT; i++)
	{
		cvReleaseImage(&refImgs[i].img);
		free(refImgs[i].feat);
		kdtree_release(refImgs[i].kd_root);
	}
	
	double timeEnd = omp_get_wtime();
	double elapsedTime = (timeEnd-timeStart);

	ofstream perFile;
	perFile.open("Computation_Peformance(LRM).txt", ios_base::app);
	perFile<<"Game #"<<gameIdx<<" (LRM)\n";
	perFile<<strCurGame;
	perFile<<"\nFrames \t\t\t\t\t: "<<start_frame<<" - "<<end_frame;
	perFile<<"\nNumber of frames \t\t: "<<end_frame - start_frame;
	perFile<<"\nInterval Size \t\t\t: "<<intervalSize;
	perFile<<"\nScale Factor \t\t\t: "<<scaleFactor;
	perFile<<"\nComputation Time \t\t: "<<elapsedTime;
	perFile<<"\n# Processed Frames \t\t: "<<(end_frame - start_frame)/intervalSize;
	perFile<<"\nAverage Time/frame \t\t: "<<elapsedTime/((end_frame - start_frame)/intervalSize)<<"\n\n";
	cout<<"\nComputation Time: "<<elapsedTime;
	fprintf(stderr, "\nDone!!\n");
	return 1;
}