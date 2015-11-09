/* 
* Author	: Vikedo Terhuja
* Date		: September 2014
* Summary	: Compute grid optical flow and detect active cells for ball entry detection.
*/

#pragma once
#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include "DirectoryReader.h"
#include <opencv/cv.h>
#include <opencv2/opencv.hpp>
#include <map>
using namespace std;
using namespace cv;

/* Cell/Grid variables */
#define CELLROWS	5
#define CELLCOLS	5
#define CROWS	8
#define CCOLS	8
#define M_PI 3.14159
const float rad2deg = (float)(180/M_PI);

/********************** Data Structures *****************************/
struct Window
{
	long startFrame;
	long endFrame;
};

struct MaxCell
{
	Point2i cell;

	/* Amplitude: The magnitude difference between cell with max OF magnitude and the average OF magnitude of the grid */
	float amplitude;
	Rect box;
	int idx;
	long frame;	

	/* Spatial Gradient */
	float sGrad;
};


/************************************************************************
/* Class name	: GridOpticalFlow
/* Description	: Computes active cell and detect ball on backboard
/***********************************************************************/
class GridOpticalFlow
{
private:
	string imagesPath;
	string outputDirPath;
	float maxAvgMag;
	float avgMaxMag;
	float adaptThresh;
	int maxAvgMaxIdx;
	Rect cells[CELLROWS][CELLCOLS];
	vector<Rect> cellArray;
	vector<string> stabImages;
	vector<float> maxCellValVector;
	vector<vector<int>> neighborCells;
	int gId;
	string segName;

	/* Spatial Gradient */
	bool bSpatialGradient;

	/* Just for test purposes */
	bool bFiner;
	bool bUseClassifier;

	vector<Rect> GetGrid(int width, int height);
	void GetFinerGrid(int width, int height);
	void GetGridArray(int width, int height);
	Point2f toPolar(cv::Point2f point);
	float ComputeCircularVariance(vector<float> vectorArray);
	float GetMagnitude(cv::Point2f point);
	float Sum(const Rect& rRect, const Mat &integralFlow);
	float DirectionVariance(Rect box, Mat &dirFlow);
	float ComputeMedianMagnitude(vector<float> magnitudeVector);	
	void RadialRefineSearch(Rect cell, const Mat &magIntFlow, const Mat &frame);
	void ComputeGridOpticalFlow(const Mat &flow, vector<Rect> &rects, int index);	

	MaxCell ComputeGridOpticalFlow(const Mat &flow, int index, const Mat &frame, long segFrame);
	MaxCell ComputeFinerGridOpticalFlow(const Mat &flow, int index, const Mat &frame, long segFrame);
	MaxCell ComputeGridOpticalFlowGradient(const Mat &flow, int index, const Mat &frame, long segFrame);	

	/* Methods just for test purposes */
	MaxCell VectorizeGridOpticalFlow(const Mat &flow, int index, const Mat &frame, ofstream &featFile);
	bool ComputeGridOpticalFlowEx(const Mat &flow, int index, const Mat &frame, MaxCell &maxCell);

	vector<MaxCell> GetBallEntryFrames(const vector<MaxCell> &maxCells);

	vector<Rect> RadialSamples(Rect centre, int radius, int nr, int nt);
	bool ValidateBallEntryCandidate(const vector<MaxCell> &maxCells, int index, int &stride);

	void GetGridArrayFeature(int width, int height);
	Mat CreateOpticalFlowIntegralImage(const Mat &flow);
	void NormalizeVectorizeOF(vector<float> vArray, float maxDiff, float avgMag, ofstream &featFile);
	void ComputeAdaptiveThreshold();

	void AverageOverWindowEx(vector<MaxCell> &maxCells);
	void BuildWindowSamples(const vector<MaxCell> &maxCells, vector<long> &fIds, int sId);

	/* Spatial Gradient */
	void FindEachCellNeighborCells();
	void FindEachCellNeighborCellsEx();
	bool OverlapCells(Rect a, Rect b);
	float SpatialGradient(float activeCellAmplitute, float avgCellMag, int activeCellIdx, const Mat &integralImg);

	/* Save data files */
	void SaveActiveCellsValue(const vector<MaxCell> &maxCells, int sId);
public:
	GridOpticalFlow(string path, string segName, int gameId);
	GridOpticalFlow(string path, string stabOutputDir, int width, int height);
	~GridOpticalFlow();
	vector<long> DetectBallEntry(vector<long> &segmentFrames, int segId);
	void CollectHistogramStats(vector<long> &segmentFrames, int segId);
	void GetTrainingVectors();
};