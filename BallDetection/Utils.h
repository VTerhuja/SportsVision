/* 
* Author	: Vikedo Terhuja
* Date		: April 2015
* Summary	: Utilities such as saving images and writing to files.
*/

#pragma once
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv2/highgui/highgui.hpp>

/************************* Data Structures **********************/
struct BoxesProperties
{
	float ovlpIOU;		// Intersection over Union overlap of Box1 and Box2
	float relDist;		// Distance between centers of Box1 and Box2 relative to Box1 size
	Rect bbBox;
};

BoxesProperties baseBP;
BoxesProperties prevBP;

struct FrameBox
{
	long frameNumber;
	Rect box;
};

/*****************************************************************/

class Utils
{
private:
	bool bDebug;
	bool bScaleImage;
	bool bCollectTrain;
	string trackerDirectory;
	string segsFolder;

public:
	Utils(bool debug, bool scale, bool collectTrain, string tDir, string sFolder): bDebug(debug), bScaleImage(scale), bCollectTrain(collectTrain), trackerDirectory(tDir), segsFolder(sFolder){}

	/* Square of value */
	inline float Square(float x)
	{
		return x*x;
	}

	/* Overlap: Intersection over Union of box1 and box2 */
	float overlapPercentage(Rect box1, Rect box2)
	{
		int l1 = box1.x;
		int t1 = box1.y;
		int r1 = box1.x + box1.width;
		int b1 = box1.y + box1.height;

		int l2 = box2.x;
		int t2 = box2.y;
		int r2 = box2.x + box1.width;
		int b2 = box2.y + box2.height;

		int xOvlp = max(0, min(r2,r1) - max(l2,l1));
		int yOvlp = max(0, min(b2,b1) - max(t2,t1));

		float iArea = xOvlp * yOvlp;
		float uArea = (max(r2,r1) - min(l2,l1)) * (max(b2,b1) - min(t2,t1));
		return iArea/uArea;
	}

	/* Compute Boxes Properties */
	BoxesProperties ComputeBoxesProperties(Rect box1, Rect box2)
	{
		BoxesProperties bp;
		bp.bbBox = box1;
		bp.ovlpIOU = overlapPercentage(box1, box2);
		float dx = (box1.x + box1.width/2) - (box2.x + box2.width/2);
		float dy = (box1.y + box1.height/2) - (box2.y + box2.height/2);

		bp.relDist = sqrt(Square(dx) + Square(dy));// * (box1.height/box2.height);
		return bp;
	}

	/* Check Criterian for Re-Initialization */
	bool CheckCriteria(BoxesProperties curBP, bool bDebug)
	{
		float deltaOvlp = abs(baseBP.ovlpIOU - curBP.ovlpIOU);
		float deltaDist = abs(baseBP.relDist - curBP.relDist);
	
		if(bDebug)
		{
			cout<<"\nBase Overlap: "<<baseBP.ovlpIOU<<" Relative Distance: "<<baseBP.relDist;
			cout<<"\nBase Overlap: "<<curBP.ovlpIOU<<" Relative Distance: "<<curBP.relDist; 
			cout<<"\nDelta Overlap: "<<deltaOvlp<<" Delta Relative Distance: "<<deltaDist<<endl;
		}
		return true;
	}

	/* Check criteria for fixing tracker */
	bool CheckFixTrackerCriteria(BoxesProperties curBP, int idx, bool bDebug)
	{
		float delta = 1.0;
		float x1 = (float)(curBP.bbBox.x + (float)curBP.bbBox.width / 2);
		float y1 = (float)(curBP.bbBox.y + (float)curBP.bbBox.height / 2);
		float x2 = (float)(prevBP.bbBox.x + (float)prevBP.bbBox.width / 2);
		float y2 = (float)(prevBP.bbBox.y + (float)prevBP.bbBox.height / 2);
		float dist = (Square(x1 - x2) + Square(y1 - y2));
		if(bDebug)	cout<<"\n"<<idx<<" Displacement: "<<dist<<" Delta: "<<delta;
		prevBP = curBP;
		if(dist < delta)
			return true;
		return false;
	}

	/* Get Rectangle */
	void rectangle(Mat& rMat, const FloatRect& rRect, const Scalar& rColour)
	{
		IntRect r(rRect);
		cv::rectangle(rMat, Point(r.XMin(), r.YMin()), Point(r.XMax(), r.YMax()), rColour);
	}

	/* Get Rect box */
	Rect GetRectBox(const FloatRect& rRect)
	{
		Rect box = Rect(rRect.XMin(), rRect.YMin(), rRect.Width(), rRect.Height());
		return box;
	}

	/* Write Backboard and Basket tracker boxes (FloatRect) to file (For evaluation) */
	void WriteToFile(ofstream &resultBBFile, ofstream &resultBSFile, double start_Frame, int fCount, const FloatRect &bb1, const FloatRect &bb2, float scaleW, float scaleH)
	{
		if(bDebug)
		{
			if(bScaleImage)
				resultBBFile << start_Frame + fCount <<" "<< bb1.XMin()/scaleW << " " << bb1.YMin()/scaleH << " " << bb1.Width()/scaleW << " " << bb1.Height()/scaleH << endl;
			else
				resultBBFile << start_Frame + fCount <<" "<< bb1.XMin() << " " << bb1.YMin() << " " << bb1.Width() << " " << bb1.Height() << endl;
				
			if(bScaleImage)
				resultBSFile << start_Frame + fCount <<" "<< bb2.XMin()/scaleW << " " << bb2.YMin()/scaleH << " " << bb2.Width()/scaleW << " " << bb2.Height()/scaleH << endl;
			else
				resultBSFile << start_Frame + fCount <<" "<< bb2.XMin() << " " << bb2.YMin() << " " << bb2.Width() << " " << bb2.Height() << endl;
		}
	}

	/* Write Backboard and Basket tracker boxes (Rect) to file (For evaluation) */
	void WriteToFile(ofstream &resultBBFile, ofstream &resultBSFile, double start_Frame, int fCount, Rect &bb1, Rect &bb2, float scaleW, float scaleH)
	{
		if(bDebug)
		{
			if(bScaleImage)
				resultBBFile << start_Frame + fCount <<" "<< bb1.x/scaleW << " " << bb1.y/scaleH << " " << bb1.width/scaleW << " " << bb1.height/scaleH << endl;
			else
				resultBBFile << start_Frame + fCount <<" "<< bb1.x << " " << bb1.y << " " << bb1.width << " " << bb1.height << endl;
				
			if(bScaleImage)
				resultBSFile << start_Frame + fCount <<" "<< bb2.x/scaleW << " " << bb2.y/scaleH << " " << bb2.width/scaleW << " " << bb2.height/scaleH << endl;
			else
				resultBSFile << start_Frame + fCount <<" "<< bb2.x << " " << bb2.y << " " << bb2.width << " " << bb2.height << endl;
		}
	}

	/* Save Image to File */
	void SaveImageToFile(const Mat &result, Rect bb, int segId, int &fIdx)
	{
		char buf[20];
		string fileName;
		Mat bbFrame = result(bb);
		sprintf(buf, "S%d_%04d.jpg", segId, fIdx);

		if(bCollectTrain)
		{
			char tBuf[20];
			sprintf(tBuf, "_S%02d", segId);
			fileName = trackerDirectory + segsFolder + tBuf + "\\" + buf;
		}
		else
			fileName = trackerDirectory + segsFolder + "\\" + buf;

		cvSaveImage(fileName.c_str(), &(IplImage(bbFrame)));
		fIdx++;
	}
};
