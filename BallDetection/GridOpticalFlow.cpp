#include "GridOpticalFlow.h"
#include "CellFeatures.h"

/************************** Constructors ****************************/
GridOpticalFlow::GridOpticalFlow(string path, string seg, int gameID)
{
	/* This constructor is called mainly for test purposes */
	imagesPath = path;
	outputDirPath = path;

	segName = seg;
	gId = gameID;

	DirectoryReader outDir = DirectoryReader(outputDirPath);
	stabImages = outDir.Read();

	Mat temp = imread(stabImages[0], 0);
	int width = temp.cols;
	int height = temp.rows;
	GetGridArray(width, height);

	bFiner = false;
	bUseClassifier = false;
	
	if(bFiner)			GetFinerGrid(width, height);
	if(bUseClassifier)	GetGridArrayFeature(width, height);
}

GridOpticalFlow::GridOpticalFlow(string path, string stabOutputDir, int width, int height)
{
	imagesPath = path;
	outputDirPath = path;
	bSpatialGradient = true;

	bFiner = true;
	int type = 1;
	if(bFiner)	
	{
		if(type == 0)	GetFinerGrid(width, height);
		else			GetGridArrayFeature(width, height); 
	}
	FindEachCellNeighborCells();
}

GridOpticalFlow::~GridOpticalFlow()
{
}

/* Check if two cells are overlapping */
bool GridOpticalFlow::OverlapCells(Rect a, Rect b)
{
	int l1 = a.x;
	int t1 = a.y;
	int r1 = a.x + a.width;
	int b1 = a.y + a.height;

	int l2 = b.x;
	int t2 = b.y;
	int r2 = b.x + b.width;
	int b2 = b.y + b.height;

	if(l1 > r2 || r1 < l2 || b1 < t2 || t1 > b2 ) return false;

	return true;
}

/* Calculate overlap percentage */
float OverlapPercentage(Rect a, Rect b)
{
	int l1 = a.x;
	int t1 = a.y;
	int r1 = a.x + a.width;
	int b1 = a.y + a.height;

	int l2 = b.x;
	int t2 = b.y;
	int r2 = b.x + b.width;
	int b2 = b.y + b.height;

	float xOvlp = max(0, min(r2,r1) - max(l2,l1));
	float yOvlp = max(0, min(b2,b1) - max(t2,t1));

	float iArea = xOvlp * yOvlp;
	float uArea = (max(r2,r1) - min(l2,l1)) * (max(b2,b1) - min(t2,t1));
	return iArea/uArea;
}

/* Find the neighbor cell index of each and every cell */
void GridOpticalFlow::FindEachCellNeighborCells()
{
	for(int i=0; i<cellArray.size(); i++)
	{
		vector<int> neighbors;
		for(int j=0; j<cellArray.size(); j++)
		{
			if(i!=j)
			{
				if(OverlapCells(cellArray[i], cellArray[j]))
				{
					neighbors.push_back(j);
				}
			}
		}
		neighborCells.push_back(neighbors);
		neighbors.clear();
	}
}

/* Find the neighbor cell index of each and every cell */
void GridOpticalFlow::FindEachCellNeighborCellsEx()
{
	for(int i=0; i<cellArray.size(); i++)
	{
		vector<int> neighbors;
		for(int j=0; j<cellArray.size(); j++)
		{
			if(i!=j)
			{
				if(OverlapPercentage(cellArray[i], cellArray[j]) > 0.07)
				{
					neighbors.push_back(j);
				}
			}
		}
		neighborCells.push_back(neighbors);
		neighbors.clear();
	}
}

/* Get the grid by dividing the image into equal sized cells */
vector<Rect> GridOpticalFlow::GetGrid(int width, int height)
{
	vector<Rect> rects;
	int w = width / CELLCOLS;
	int h = height / CELLROWS;

	Rect temp(0, 0, w, h);
	for(int i=0; i<CELLROWS; i++)
	{
		temp.y = (h*i);
		for(int j=0; j<CELLCOLS; j++)
		{
			temp.x = (w*j);
			rects.push_back(temp);
		}
	}

	return rects;
}

/* Compute finer grid with overlapping cells */
void GridOpticalFlow::GetFinerGrid(int width, int height)
{
	int w = width * .25;
	int h = height * .25;

	Rect temp(0, 0, w, h);
	for(int x=0; x+w<=width; x+=7)
	{
		temp.x = x;
		for(int y=0; y+h<=height; y+=7)
		{
			temp.y = y;
			cellArray.push_back(temp);
		}
	}
	cout<<"\nNumber of cells: "<<cellArray.size()<<endl;
}

/* Compute grid and store it in 2D array */
void GridOpticalFlow::GetGridArray(int width, int height)
{
	int w = width / CELLCOLS;
	int h = height / CELLROWS;

	Rect temp(0, 0, w, h);
	for(int i=0; i<CELLCOLS; i++)
	{
		temp.y = (h*i);
		for(int j=0; j<CELLROWS; j++)
		{
			temp.x = (w*j);
			cells[i][j] = temp;
		}
	}
}

/* Compute grid and store it in a vector */
void GridOpticalFlow::GetGridArrayFeature(int width, int height)
{
	int w = width * 0.20;
	int h = height * 0.25;
	float wStride = (float)(width-w)/(CCOLS-1);
	float hStride = (float)(height-h)/(CROWS-1);
	Rect temp(0, 0, w, h);
	
	for(int x=0; x < CCOLS; x++)
	{
		temp.x = x * wStride;
		for(int y=0; y < CROWS; y++)
		{
			temp.y = y * hStride;
			cellArray.push_back(temp);
		}
	}
}

/* Convert from cartesian to polar coordinates */
Point2f GridOpticalFlow::toPolar(cv::Point2f point)
{
	Point2f pc;
	pc.y = atan2(point.y, point.x);
	pc.x = sqrt(pow(point.x,2) + pow(point.y,2));
	return pc;
}

/* Compute Circular Variance of Optical Flow Variance */
float GridOpticalFlow::ComputeCircularVariance(vector<float> vectorArray)
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

/* Compute Optical Flow vector magnitude */
float GridOpticalFlow::GetMagnitude(cv::Point2f point)
{
	float magnitude = sqrt(point.x * point.x + point.y * point.y);
	return magnitude;
}

/* Compute Optical Flow vectors sum */
float GridOpticalFlow::Sum(const Rect& rRect, const Mat &integralFlow)
{
	assert(rRect.x >= 0 && rRect.y >= 0 && (rRect.x + rRect.width)  < integralFlow.cols && (rRect.y + rRect.height) < integralFlow.rows);
	return (integralFlow.at<float>(rRect.y, rRect.x) +	
		   integralFlow.at<float>(rRect.y + rRect.height, rRect.x + rRect.width) - 
		   integralFlow.at<float>(rRect.y + rRect.height, rRect.x) - 
		   integralFlow.at<float>(rRect.y, rRect.x + rRect.width)) / (rRect.area());
}

/* Compute Optical Flow direction variance */
float GridOpticalFlow::DirectionVariance(Rect box, Mat &dirFlow)
{
	vector<float> directionArray;
	for(int x = box.x; x < (box.x + box.width); x++)
	{
		for(int y = box.y; y < (box.y + box.height); y++)
		{
			directionArray.push_back(dirFlow.at<float>(x,y));
		}
	}
	return ComputeCircularVariance(directionArray);
}

/* Compute median of Optical Flow vectors magnitude */
float GridOpticalFlow::ComputeMedianMagnitude(vector<float> magnitudeVector)
{
	int arraySize = (int)magnitudeVector.size();
	float median = 0.;

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

	return median;
}

/* Generate Radial Samples cells around the given center */
vector<Rect> GridOpticalFlow::RadialSamples(Rect center, int radius, int nr, int nt)
{
	vector<Rect> samples;
	float xc = center.x + center.width/2;
	float yc = center.y + center.height/2;

	Rect s(center);
	float rstep = (float)radius/nr;
	float tstep = 2*(float)M_PI/nt;
	samples.push_back(center);
	
	for (int ir = 1; ir <= nr; ++ir)
	{
		float phase = (ir % 2)*tstep/2;
		for (int it = 0; it < nt; ++it)
		{
			float dx = ir*rstep*cosf(it*tstep+phase);
			float dy = ir*rstep*sinf(it*tstep+phase);
			s.x = center.x+dx;
			s.y = center.x+dy;
			samples.push_back(s);
		}
	}	
	return samples;
}

/* Check if the sample is valid */
bool IsSampleValid(Rect sample, int width, int height)
{
	return (sample.x >= 0 && sample.y >= 0 && sample.x + sample.width < width && sample.y + sample.height < height);
}

/* Extension: Perform a smoothing of the maxcell amplitude across a window */
void GridOpticalFlow::AverageOverWindowEx(vector<MaxCell> &maxCells)
{
	int	avgWindowSize = 2;
	bool bWrite = false;
	ofstream ampFile;
	int count = maxCells.size();
	if(bWrite)	ampFile.open("SmoothedMaxCellsAmplitude.txt");

	for(int i = 0; i < count; i++)
	{
		float val = 0;
		int wCount = 0;
		int idx = i + avgWindowSize;
		while(wCount < (2*avgWindowSize + 1))
		{
			if(idx < 0)
				val += maxCells[0].amplitude;
			else if(idx >= count)
				val += maxCells[count-1].amplitude;
			else
				val += maxCells[idx].amplitude;
			wCount++;
			idx--;
		}
		maxCells[i].amplitude = val/(2*avgWindowSize + 1);

		if(bWrite)
		{
			ampFile<<val/(2*avgWindowSize + 1)<<"\n";
		}
	}

	if(bWrite)	ampFile.close();
}

/* Perform a smoothing of the maxcell amplitude across a window */
void AverageOverWindow(vector<MaxCell> &maxCells)
{
	/* Preceeding 2 values */
	int	avgWindowSize = 3;
	ofstream ampFile;
	int count = maxCells.size();

	for(int i = 3; i < count; i++)
	{
		float val = 0;
		int wCount = 0;
		int idx = i;
		while(wCount < avgWindowSize + 1)
		{
			if(idx < 0)
				val += maxCells[0].amplitude;
			else if(idx >= count)
				val += maxCells[count-1].amplitude;
			else
				val += maxCells[idx].amplitude;
			wCount++;
			idx--;
		}
		maxCells[i].amplitude = val/(avgWindowSize + 1);
	}
}

/* Refine the radial search for active cell in the grid */
void GridOpticalFlow::RadialRefineSearch(Rect cell, const Mat &magIntFlow, const Mat &frame)
{
	bool bShowImage = false;
	if(bShowImage)
	{
		Mat temp;
		frame.copyTo(temp);
		rectangle(temp, cell, Scalar(0, 0 , 255), 1);
		imshow("Max Cell", temp);
		waitKey(0);
		cvSaveImage("MaxCell.jpg", &(IplImage(temp)));
	}

	float max = 0.0;
	int maxIndex = 0;
	vector<Rect> samples = RadialSamples(cell, 20, 3, 8);

	Mat temp2;
	frame.copyTo(temp2);

	/* Trim out invalid samples */
	for(int i=0; i<samples.size(); i++)
	{
		if(IsSampleValid(samples[i], magIntFlow.cols, magIntFlow.rows))
		{
			if(bShowImage) rectangle(temp2, samples[i], Scalar(0,0, 255), 1);
			float cellSum = Sum(samples[i], magIntFlow);
			if(max < cellSum)
			{
				max = cellSum;
				maxIndex = i;
			}
		}
	}	
	
	//	rectangle(temp2, samples[maxIndex], Scalar(0,0, 255), 1);
	if(bShowImage)
	{
		imshow("Samples", temp2);
		waitKey(0);
		cvSaveImage("Samples.jpg", &(IplImage(temp2)));

		Mat temp3;
		frame.copyTo(temp3);
		rectangle(temp3, samples[maxIndex], Scalar(0,0, 255), 1);
		imshow("Real Max", temp3);
		cvSaveImage("RealMax.jpg", &(IplImage(temp3)));
		waitKey(0);
	}
}

/* Compute the optical flow for each cell in the grid */
void GridOpticalFlow::ComputeGridOpticalFlow(const Mat &flow, vector<Rect> &rects, int index)
{
	cv::Mat magFlow(flow.size(), CV_32FC1);
	//cv::Mat dirFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			//Point2f t = toPolar(flow.at<cv::Point2f>(r,c));
			//magFlow.at<float>(r,c) = t.x;
			//dirFlow.at<float>(r,c) = t.y;

			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float minCellVal = 1000, maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);
	int maxIndex = 0;

	vector<float> meanDirArray;
	for(int i = 0; i < rects.size(); i++)
	{
		cellVal = Sum(rects[i], integralFlow);
		if(cellVal > maxCellVal)	
		{
			maxIndex = i;
			maxCellVal = cellVal;
		}
		if(cellVal < minCellVal)	minCellVal = cellVal;
		avgCellVal += cellVal;
		//meanDirArray.push_back(DirectionVariance(rects[maxIndex], dirFlow));
	}

	//cirVar = DirectionVariance(rects[maxIndex], dirFlow);
	//cirVar *= rad2deg;
	//medDir = ComputeMedianMagnitude(meanDirArray);

	avgCellVal /= CELLROWS * CELLROWS;
	//ofMagnitude<<index<<" "<<maxCellVal<<" "<<avgCellVal<<" "<<maxCellVal - avgCellVal<<" "<<maxIndex<<"\n";
}

/* Compute the optical flow for each cell in the grid */
MaxCell GridOpticalFlow::ComputeGridOpticalFlow(const Mat &flow, int index, const Mat &frame, long segFrame)
{
	MaxCell maxCell;
	maxCell.frame = segFrame;
	cv::Mat magFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);

	for(int i = 0; i < CELLROWS; i++)
	{
		for(int j = 0; j < CELLCOLS; j++)
		{		
			cellVal = Sum(cells[i][j], integralFlow);
			if(cellVal > maxCellVal)	
			{
				maxCell.cell.y = i;
				maxCell.cell.x = j;
				maxCellVal = cellVal;
			}

			avgCellVal += cellVal;
		}
	}

	avgCellVal /= CELLROWS * CELLROWS;

	//if(maxAvgMag < avgCellVal)
	//{
	//	maxAvgMag = avgCellVal;
	//	maxAvgMaxIdx = index;
	//}
	maxCell.amplitude = maxCellVal - avgCellVal;
	maxCellValVector.push_back(maxCell.amplitude);
	return maxCell;
}

/* Get Median of amplitude of active cells */
float GetMedian(vector<float> &cellMagArray)
{
	sort(cellMagArray.begin(), cellMagArray.end());
	int n = cellMagArray.size();
	if(n % 2 == 0)
	{
		return (cellMagArray[n/2] + cellMagArray[n/2 - 1])/2;
	}
	else
	{
		return cellMagArray[n/2];
	}
}

/* Compute the optical flow for each cell in the finer grid */
MaxCell GridOpticalFlow::ComputeFinerGridOpticalFlow(const Mat &flow, int index, const Mat &frame, long segFrame)
{
	MaxCell maxCell;
	maxCell.frame = segFrame;
	cv::Mat magFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);

	for(int i=0; i < cellArray.size(); i++)
	{
		cellVal = Sum(cellArray[i], integralFlow);
		if(cellVal > maxCellVal)	
		{
			maxCell.box = cellArray[i];
			maxCellVal = cellVal;
			maxCell.idx = i;
		}

		avgCellVal += cellVal;
	}

	avgCellVal /= cellArray.size();
	maxCell.amplitude = maxCellVal - avgCellVal;

	avgMaxMag += maxCell.amplitude;
	maxCellValVector.push_back(maxCell.amplitude);
	return maxCell;
}

/* Compute the spatial gradient: Difference of active cell and average of neighboring cells */
float GridOpticalFlow::SpatialGradient(float activeCellAmplitute, float avgCellMag, int idx, const Mat &iImg)
{
	float avgGrad;
	int n = neighborCells[idx].size();
	for(int i=0; i<n; i++)
	{
		avgGrad += Sum(cellArray[neighborCells[idx][i]], iImg) - avgCellMag;
	}
	return abs(activeCellAmplitute - avgGrad/n);
}

/* Compute Grid optical flow gradient of active cells */
MaxCell GridOpticalFlow::ComputeGridOpticalFlowGradient(const Mat &flow, int index, const Mat &frame, long segFrame)
{
	MaxCell maxCell;
	maxCell.frame = segFrame;
	cv::Mat magFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);

	for(int i=0; i < cellArray.size(); i++)
	{
		cellVal = Sum(cellArray[i], integralFlow);
		if(cellVal > maxCellVal)	
		{
			maxCell.box = cellArray[i];
			maxCellVal = cellVal;
			maxCell.idx = i;
		}

		avgCellVal += cellVal;
	}

	avgCellVal /= cellArray.size();
	maxCell.amplitude = maxCellVal - avgCellVal;
	maxCell.sGrad = SpatialGradient(maxCell.amplitude, avgCellVal, maxCell.idx, integralFlow);

	avgMaxMag += maxCell.amplitude;
	maxCellValVector.push_back(maxCell.amplitude);
	return maxCell;
}

/* Create integral image of optical flow magnitude */
Mat GridOpticalFlow::CreateOpticalFlowIntegralImage(const Mat &flow)
{
	cv::Mat magFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);
	return integralFlow;
}

/* Normalize and vectorize optical flow */
void GridOpticalFlow::NormalizeVectorizeOF(vector<float> vArray, float maxDiff, float avgMag, ofstream &featFile)
{
	for(int i=0; i<vArray.size(); i++)
	{
		vArray[i] = (vArray[i] - avgMag)/maxDiff;
	}

	std::sort(vArray.begin(), vArray.end());
	//featFile<<idx<<" ";
	for(int i=0; i<vArray.size(); i++)
	{
		featFile<<vArray[i]<<" ";
	}
	featFile<<"\n";
}

MaxCell GridOpticalFlow::VectorizeGridOpticalFlow(const Mat &flow, int index, const Mat &frame, ofstream &featFile)
{
	vector<float> vArray;
	MaxCell maxCell;
	float maxCellVal = 0, avgCellVal = 0, cellVal = 0;
	Mat iFlow = CreateOpticalFlowIntegralImage(flow);
	for(int i=0; i < cellArray.size(); i++)
	{
		cellVal = Sum(cellArray[i], iFlow);
		vArray.push_back(cellVal);
		if(cellVal > maxCellVal)	
		{
			maxCell.box = cellArray[i];
			maxCellVal = cellVal;
			maxCell.idx = i;
		}

		avgCellVal += cellVal;
	}

	avgCellVal /= cellArray.size();

	//if(maxAvgMag < avgCellVal)
	//{
	//	maxAvgMag = avgCellVal;
	//	maxAvgMaxIdx = index;
	//}
	maxCell.amplitude = maxCellVal - avgCellVal;
	NormalizeVectorizeOF(vArray, maxCell.amplitude, avgCellVal, featFile);

	return maxCell;
}

/* Compute dense optical flow and refine search for maximum */
bool GridOpticalFlow::ComputeGridOpticalFlowEx(const Mat &flow, int index, const Mat &frame, MaxCell &maxCell)
{
	cv::Mat magFlow(flow.size(), CV_32FC1);
	for(int r = 0; r < flow.rows; r++)
	{
		for(int c = 0; c < flow.cols; c++)
		{
			magFlow.at<float>(r,c) = GetMagnitude(flow.at<cv::Point2f>(r,c));
		}
	}

	float maxCellVal = 0, avgCellVal = 0, cellVal = 0, cirVar = 0.0, medDir = 0.0;
	Mat integralFlow ;//(flow.rows+1, flow.cols+1, CV_32FC1);
	cv::integral(magFlow, integralFlow, CV_32F);

	for(int i = 0; i < CELLROWS; i++)
	{
		for(int j = 0; j < CELLCOLS; j++)
		{		
			cellVal = Sum(cells[i][j], integralFlow);
			if(cellVal > maxCellVal)	
			{
				maxCell.cell.y = i;
				maxCell.cell.x = j;
				maxCellVal = cellVal;
			}

			avgCellVal += cellVal;
		}
	}

	avgCellVal /= CELLROWS * CELLROWS;
	maxCell.amplitude = maxCellVal - avgCellVal;

	/* if amplitude is above a threshold then refine search for max cell */
	if(maxCell.amplitude > 1.5)
	{
		RadialRefineSearch(cells[maxCell.cell.y][maxCell.cell.x], integralFlow, frame);
		return true;
	}

	//ofMagnitude<<index<<" "<<maxCellVal<<" "<<avgCellVal<<" "<<maxCellVal - avgCellVal<<" "<<maxIndex<<"\n";
	return false;
}

/* Find whether Max cells of consecutive frames are adjacent */
bool IsAdjacent(MaxCell a, MaxCell b)
{
	if(abs(a.cell.x - b.cell.x) > 1 || abs(a.cell.y - b.cell.y) > 1)	
		return false;

	return true;
}

/* Find whether Max cells of consecutive frames are at least 2 cells apart */
bool IsAdjacentEx(MaxCell a, MaxCell b)
{
	if(abs(a.cell.x - b.cell.x) + abs(a.cell.y - b.cell.y) > 3)	
		return false;

	return true;
}

/* Check if the boxes are overlapping */
bool IsOverlap(MaxCell a, MaxCell b)
{
	int l1 = a.box.x;
	int t1 = a.box.y;
	int r1 = a.box.x + a.box.width;
	int b1 = a.box.y + a.box.height;

	int l2 = b.box.x;
	int t2 = b.box.y;
	int r2 = b.box.x + b.box.width;
	int b2 = b.box.y + b.box.height;

	if(l1 > r2 || r1 < l2 || b1 < t2 || t1 > b2 ) return false;

	return true;
}

/* Validate the ball entry candidate */
bool GridOpticalFlow::ValidateBallEntryCandidate(const vector<MaxCell> &maxCells, int index, int &stride)
{
	int iAdjCellThreshold = 5;
	MaxCell ref = maxCells[index];
	int count = 0;

	for(int i=index+1; i < maxCells.size(); i++)
	{
		if(maxCells[i].amplitude > adaptThresh)
		{
			if(bFiner)
			{
				if(IsOverlap(ref, maxCells[i]))
					count++;
				else break;
			}
			else
			{
				if(IsAdjacentEx(ref, maxCells[i]))
					count++;
				else break;
			}
		}
		else break;
		ref = maxCells[i];
	}
	
	if(count >= iAdjCellThreshold)
	{
		stride = count;
		return true;
	}		
	return false;
}

/* Find frames where ball entered the backboard */
vector<MaxCell> GridOpticalFlow::GetBallEntryFrames(const vector<MaxCell> &maxCells)
{
	/* This threshold is a prior: A ball cannot be enter the backboard twice within 3 secs: 90 frames  or even if so it can be considered as one shot */
	int priorSpan = 3 * 30; 
	vector<MaxCell> entryIndeces;
	int idx = 0;

	while(idx < maxCells.size())
	{
		if(maxCells[idx].amplitude > adaptThresh)
		{
			int stride = 0;
			if(ValidateBallEntryCandidate(maxCells, idx, stride))
			{
				if(entryIndeces.empty())
				{
					entryIndeces.push_back(maxCells[idx]);
				}
				else if((maxCells[idx].frame - entryIndeces.back().frame) > priorSpan) 
				{
					entryIndeces.push_back(maxCells[idx]);
				}
				idx += stride;
				//cout<<"\nStride: "<<stride<<endl;
			}
		}
		idx++;
	}
	return entryIndeces;
}

/* Extract features for training Linear Classifier */
void GridOpticalFlow::GetTrainingVectors()
{
	vector<long> segmentFrames;
	bool bCreateLabel = false;
	DirectoryReader outDir = DirectoryReader(outputDirPath);
	vector<string> imageFiles = outDir.GetFiles();
	if(bCreateLabel)
	{
		ofstream labelFile;
		labelFile.open("Label.txt");
		for(int i=0; i<imageFiles.size(); i++)
		{
			labelFile<<imageFiles[i]<<" "<<0<<"\n";
		}
		labelFile.close();
		exit(0);
	}
	else
	{
		for(int i=0; i<imageFiles.size(); i++)
		{
			segmentFrames.push_back(i);
		}
	}
	
	vector<MaxCell> maxCells;
	Mat frame1, frame2, flow;
	frame1 = imread(stabImages[0], 0);
	int width = frame1.cols;
	int height = frame1.rows;
	maxAvgMag = 0.0;

	char buf[20];
	sprintf(buf, "Feature_G%02d_", gId);
	segName = buf + segName + ".txt";
	ofstream featFile;
	featFile.open(segName);

	for(int i = 0; i < stabImages.size(); i++)
	{	
		//cout<<i<<endl;
		frame2 = imread(stabImages[i], 0);
		if(frame2.cols != width || frame2.rows != height)
		{
			resize(frame2, frame2, Size(width, height));
		}
		
		// frame1: Single channel (grayscale) previous frame
		// frame2: Single channel next frame
		// flow		: Optical flow image
		// 0.5		: Pyramid scale (each later is twice smaller that the previous)
		// 3		: Levels of pyramid
		// 3		: Optical Flow window size
		// 5		: # Iterations
		// 5		: Size of pixel neighborhood
		// 1.1		: Standard deviation of Gaussian
		// Flag		: OPTFLOW_USE_INITIAL_FLOW /OPTFLOW_FARNEBACK_GAUSSIAN 
		calcOpticalFlowFarneback(frame1, frame2, flow, 0.5, 3, 15, 3, 10, 5, 1);
		
		if(bUseClassifier)
			VectorizeGridOpticalFlow(flow, i, frame2, featFile);

		frame2.copyTo(frame1);
		flow.release();
	}
	featFile.close();

	bool bSaveMaxImage = true;
	if(bSaveMaxImage)
	{
		Mat img;
		char buf[200];
		for(int i=0; i<stabImages.size(); i++)
		{
			img = cv::imread(stabImages[i]);
			cv::rectangle(img, maxCells[i].box, cv::Scalar(0, 255, 0), 1);				
			sprintf(buf, "I%02d.jpg", i);
			string fileName = buf;
			cvSaveImage(fileName.c_str(), &(IplImage(img)));
		}
	}
}

/* Trace: Write Max Cell Magnitude Difference */
void WriteDiffMagnitude(vector<MaxCell> &maxCells)
{
	bool bDiff = false;
	if(bDiff)
	{
		ofstream dFile;
		dFile.open("DiffMagnitude.txt");
		/* Debug Max cell magnitude */
		for(int m = 0; m < maxCells.size()-5; m++)
		{
			dFile<<m+5<<" "<<maxCells[m+5].amplitude - maxCells[m].amplitude<<"\n";
		}
		dFile.close();
	}
}

/* Trace: Write Max Cell Magnitude */
void WriteMaxCellMagnitude(vector<MaxCell> &maxCells, bool bAvg, bool bC)
{
	vector<float> aggGrad;
	vector<float> aggGradEx;
	vector<float> aggMax;
		
	bool bWriteMag = true;
	if(bWriteMag)
	{
		ofstream cellMagFile;
		ofstream gradMagFile;
		!bAvg ? cellMagFile.open("MaxCellMagnitude.txt") : cellMagFile.open("MaxCellMagnitudeAvg.txt");
		!bAvg ? gradMagFile.open("MaxCellGradient.txt") : gradMagFile.open("MaxCellAvgGradient.txt");
		/* Debug Max cell magnitude */
		float lastMag = 0;
		for(int m = 0; m < maxCells.size(); m++)
		{
			cellMagFile<<m<<" "<<maxCells[m].amplitude<<"\n";
			if(m == 0)
			{
				aggMax.push_back(maxCells[m].amplitude);
				aggGrad.push_back(0);
				gradMagFile<<m<<" "<<0<<"\n";
			}
			else
			{
				aggMax.push_back(aggMax.back() + maxCells[m].amplitude);
				aggGrad.push_back(aggGrad.back() + abs(lastMag - maxCells[m].amplitude));
				gradMagFile<<m<<" "<<abs(lastMag - maxCells[m].amplitude)<<"\n";
			}
			lastMag = maxCells[m].amplitude;
		}
		cellMagFile.close();
		gradMagFile.close();
	}

	ofstream grad2File;
	grad2File.open("GradientEx.txt");
	for(int i=0; i<maxCells.size(); i++)
	{
		if(i==0)
		{
			if(i+1 < maxCells.size())
				aggGradEx.push_back(abs(maxCells[i].amplitude - maxCells[i+1].amplitude));
			else
				aggGradEx.push_back(maxCells[i].amplitude);
		}
		else
		{
			if(i != maxCells.size() - 1)
				aggGradEx.push_back(abs(maxCells[i-1].amplitude - maxCells[i+1].amplitude));
			else
				aggGradEx.push_back(abs(maxCells[i-1].amplitude - maxCells[i].amplitude));
		}
		grad2File<<i<<" "<<aggGradEx[i]<<"\n";
	}
	grad2File.close();

	if(bC)
	{
		ofstream clusterFile;
		clusterFile.open("Cluster.txt");
		int sWSize = 20;
		int index = 0;
		int i = 0;
		vector<pair<int, pair<int,int>>> windows;
		while(i + sWSize < maxCells.size())
		{
			windows.push_back(make_pair(index, make_pair(aggMax[i+sWSize] - aggMax[i], aggGrad[i+sWSize] - aggGrad[i])));
			clusterFile<<index<<" "<<aggMax[i+sWSize] - aggMax[i]<<" "<<aggGrad[i+sWSize] - aggGrad[i]<<"\n";
			i += sWSize/2;
			index++;
		}
		clusterFile.close();
	}
}

/* Calculate L2 Norm */
float L2Norm(float mag, float grad)
{
	return sqrt(mag * mag + grad * grad);
}

/* Compute adaptive threshold by averaging the top k max cell values */
void GridOpticalFlow::ComputeAdaptiveThreshold()
{
	sort(maxCellValVector.rbegin(), maxCellValVector.rend());
	float k = .15;
	int topK = maxCellValVector.size() * k;
	adaptThresh = 0;
	for(int i=0; i<topK; i++)
	{
		adaptThresh += maxCellValVector[i];
	}
	adaptThresh = adaptThresh/topK;
}

/* Build samples of window size across the samples and cluster the samples into two classes using k-means */
void GridOpticalFlow::BuildWindowSamples(const vector<MaxCell> &maxCells, vector<long> &fIds, int sId)
{
	ofstream resFile;
	resFile.open("G#10_Cluster_Difference.txt", ios::app);
	vector<float> aggMax;
	vector<float> aggGrad;
	
	/* High pass filter: Filter out all signals below average of Max Cell amplitude */
	/*
	vector<float> filtered(maxCells.size(), 0);
	for(int i = 0; i < maxCells.size(); i++)
	{
		filtered[i] = maxCells[i].amplitude - avgMaxMag < 0 ? 0 : maxCells[i].amplitude - avgMaxMag;	
	}

	float lastMag = 0;
	for(int m = 0; m < maxCells.size(); m++)
	{
		if(m == 0)
		{
			aggMax.push_back(filtered[m]);
			aggGrad.push_back(0);
		}
		else
		{
			aggMax.push_back(aggMax.back() + filtered[m]);
			aggGrad.push_back(aggGrad.back() + abs(lastMag - maxCells[m].amplitude));
		}
		lastMag = maxCells[m].amplitude;
	}
	*/

	vector<float> gradEx;
	for(int i=0; i<maxCells.size(); i++)
	{
		if(i==0)
		{
			if(i+1 < maxCells.size())
				gradEx.push_back(abs(maxCells[i].amplitude - maxCells[i+1].amplitude));
			else
				gradEx.push_back(maxCells[i].amplitude);
		}
		else
		{
			if(i != maxCells.size() - 1)
				gradEx.push_back(abs(maxCells[i-1].amplitude - maxCells[i+1].amplitude));
			else
				gradEx.push_back(abs(maxCells[i-1].amplitude - maxCells[i].amplitude));
		}
	}


	/* Calculate cumulative maximum cell amplitude and absolute gradient */
	float lastMag = 0;
	for(int m = 0; m < maxCells.size(); m++)
	{
		if(m == 0)
		{
			aggMax.push_back(maxCells[m].amplitude);
			//aggGrad.push_back(0);
			aggGrad.push_back(gradEx[m]);
		}
		else
		{
			aggMax.push_back(aggMax.back() + maxCells[m].amplitude);
			//aggGrad.push_back(aggGrad.back() + abs(lastMag - maxCells[m].amplitude));
			aggGrad.push_back(aggGrad.back() + abs(lastMag - gradEx[m]));
		}
		lastMag = gradEx[m];
	}

	/* Scanning window size */
	int sWSize = maxCells.size() < 150 ? 20 : 30;
	float sFactor = maxCells.size() < 150 ? 1.5 : 2;
	int index = 0;
	int i = 0;
	Window w;
	vector<pair<int, pair<int,int>>> windows;
	map<int, Window> scanWindows;

	float windowAmpMax = 0;
	float windowGradMax = 0;
	float maxL2Norm = 0;
	//vector<float> l2Norms;
	vector<float> windowsAggMax, windowsAggGrad;
	while(i + sWSize < maxCells.size())
	{
		w.startFrame = maxCells[i].frame;
		w.endFrame = maxCells[i+sWSize].frame;
		scanWindows.insert(make_pair(index, w));
		windowsAggMax.push_back(aggMax[i+sWSize] - aggMax[i]);
		windowsAggGrad.push_back(aggGrad[i+sWSize] - aggGrad[i]);

		/*
		if(windowAmpMax < aggMax[i+sWSize] - aggMax[i])
			windowAmpMax = aggMax[i+sWSize] - aggMax[i];

		if(windowGradMax < aggGrad[i+sWSize] - aggGrad[i])
			windowGradMax = aggGrad[i+sWSize] - aggGrad[i];
		*/

		float l2 = L2Norm(windowsAggMax.back(), windowsAggGrad.back());
		//l2Norms.push_back(l2);
		if(maxL2Norm < l2)
			maxL2Norm = l2;

		i += sWSize/2;
		index++;
	}

	/* For debugging
	ofstream winNormFile;
	winNormFile.open("WindowsNorms.txt");
	*/
	/* Just for testing */
	/*
	for(int i=0; i<l2Norms.size(); i++)
	{
		winNormFile<<i<<" "<<l2Norms[i]/maxL2Norm<<"\n";
	}
	winNormFile.close();
	*/

	/* #Scanning windows */
	int wCount = scanWindows.size();
	/* if the number of frames is smaller than sWSize there can be no ball entry in the segment */
	if(sWSize > maxCells.size() || wCount < 2)	{ return; }

	/* Horizontal contatenation of window's aggregate amplitude and aggregate gradient */
	/* Each row represents a sample */
	Mat samples;
	samples.push_back(Mat(windowsAggMax));
	hconcat(samples, Mat(windowsAggGrad), samples);

	/* Cluster the samples into two samples */
	int k = 2;
	Mat labels;
	Mat centers;
	kmeans(samples, k, labels, TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 100, 0.001), 3, KMEANS_PP_CENTERS, centers);
	vector<int> cZero, cOne;

	/* Count the number of samples in each cluster */
	for(int i=0; i<wCount; i++)
	{
		if(labels.at<int>(i,0) == 0)
			cZero.push_back(i);
		else
			cOne.push_back(i);
	}

	Mat centZero, centOne;
	centers.row(0).copyTo(centZero);
	centers.row(1).copyTo(centOne);

	//float similarity = abs(centZero.at<float>(0,0) - centOne.at<float>(0,0))/windowAmpMax;
	//float gadSimilarity = abs(centZero.at<float>(0,1) - centOne.at<float>(0,1))/windowGradMax;

	float na = L2Norm(centZero.at<float>(0,0), centZero.at<float>(0,1));
	float nb = L2Norm(centOne.at<float>(0,0), centOne.at<float>(0,1));
	float ndiff = (float)abs(na-nb)/maxL2Norm;

	resFile<<sId<<" "<<ndiff<<"\n";
	int nZero = cZero.size();
	int nOne = cOne.size();
	/* Criteria Satisfied: Ball entry detected */
	if(min(nZero, nOne) * sFactor < max(nZero, nOne) && ndiff > .40)
	{		
		/* Selected windows */
		vector<int> selWindows;
		if(nZero < nOne)
			selWindows.swap(cZero);
		else
			selWindows.swap(cOne);

		/* Potential windows */
		vector<Window> potentWindows;
		potentWindows.push_back(scanWindows[selWindows[0]]);
		for(int i=1; i<selWindows.size(); i++)
		{
			if(potentWindows.back().endFrame > scanWindows[selWindows[i]].startFrame)
				potentWindows.back().endFrame = scanWindows[selWindows[i]].endFrame;
			else
				potentWindows.push_back(scanWindows[selWindows[i]]);
		}

		/* Perform another scan on each potential window and find the window with the highest sum of maximum amplitude */
		for(int i=0; i<potentWindows.size(); i++)
		{
			int j=potentWindows[i].startFrame;
			float maxMag = INT_MIN;
			int maxFrameIndex = 0;
			int idx = 0;
			while(j + sWSize <= potentWindows[i].endFrame)
			{
				float val = aggMax[idx+sWSize] - aggMax[idx];
				if(maxMag < val)
				{
					maxMag = val;
					maxFrameIndex = j + sWSize/3;
				}
				j += 5;
				idx += 5;
			}
			fIds.push_back(maxFrameIndex);
		}
	}
	resFile.close();
}

/* Save amplitude of active cells */
void GridOpticalFlow::SaveActiveCellsValue(const vector<MaxCell> &maxCells, int sId)
{
	string dirName = "E:\\Research\\Projects\\StruckTracker\\Release\\tOutput\\Files\\G#3\\";
	char buf[100];
	string fName;
	sprintf(buf, "Segment_%03d.txt", sId);
	fName = dirName + buf;
	ofstream file;
	file.open(fName);

	for(int i=0; i<maxCells.size(); i++)
	{
		if(bSpatialGradient)
			file<<maxCells[i].frame<<" "<<maxCells[i].amplitude<<" "<<maxCells[i].sGrad<<"\n";
		else
			file<<maxCells[i].frame<<" "<<maxCells[i].amplitude<<"\n";
	}
	file.close();
}

/* Detect frame of ball entry to the backboard */
vector<long> GridOpticalFlow::DetectBallEntry(vector<long> &segmentFrames, int segId)
{
	vector<long> fIds;
	ofstream resFile;
	resFile.open("BallEntryResult.txt", ios::app);

	DirectoryReader outDir = DirectoryReader(imagesPath);
	stabImages = outDir.Read();
	
	if(segmentFrames.size() != stabImages.size()) { cout<<"\nError: Number of frames not equal to number of images!!"<<endl;  return fIds; }
	vector<MaxCell> maxCells;
	Mat frame1, frame2, flow;
	frame1 = imread(stabImages[0], 0);
	int width = frame1.cols;
	int height = frame1.rows;
	maxAvgMag = 0.0;
	avgMaxMag = 0.0;

	/* TBD: Do a resize for now. Later each section can be processed separately */

	for(int i = 1; i < stabImages.size(); i++)
	{	
		//cout<<i<<endl;
		frame2 = imread(stabImages[i], 0);
		if(frame2.cols != width || frame2.rows != height)
		{
			resize(frame2, frame2, Size(width, height));
		}
		
		// frame1: Single channel (grayscale) previous frame
		// frame2: Single channel next frame
		// flow		: Optical flow image
		// 0.5		: Pyramid scale (each later is twice smaller that the previous)
		// 3		: Levels of pyramid
		// 3		: Optical Flow window size
		// 5		: # Iterations
		// 5		: Size of pixel neighborhood
		// 1.1		: Standard deviation of Gaussian
		// Flag		: OPTFLOW_USE_INITIAL_FLOW /OPTFLOW_FARNEBACK_GAUSSIAN 
		calcOpticalFlowFarneback(frame1, frame2, flow, 0.5, 3, 15, 3, 10, 5, 1);
		
		if(!bFiner)
			maxCells.push_back(ComputeGridOpticalFlow(flow, i, frame2, segmentFrames[i]));
		else if(bFiner)
		{
			if(bSpatialGradient)
				maxCells.push_back(ComputeGridOpticalFlowGradient(flow, i, frame2, segmentFrames[i]));
			else
				maxCells.push_back(ComputeFinerGridOpticalFlow(flow, i, frame2, segmentFrames[i]));
		}

		frame2.copyTo(frame1);
		flow.release();
	}

	avgMaxMag = avgMaxMag/maxCells.size();

	//WriteMaxCellMagnitude(maxCells, false, false);
	/* Smooth amplitude */
	AverageOverWindowEx(maxCells);
	SaveActiveCellsValue(maxCells, segId);
	return fIds;
	//WriteMaxCellMagnitude(maxCells, true, true);

	BuildWindowSamples(maxCells, fIds, segId);
	
	if(!fIds.empty())
	{
		for(int i=0; i<fIds.size(); i++)
			resFile<<segId<<" "<<fIds[i]<<"\n";
	}
	else
	{
		resFile<<segId<<" "<<-1<<"\n";
	}

	resFile.close();
	stabImages.clear();

	return fIds;
}

/* Detect frame of ball entry to the backboard */
void GridOpticalFlow::CollectHistogramStats(vector<long> &segmentFrames, int segId)
{
	outputDirPath = imagesPath;
	DirectoryReader outDir = DirectoryReader(outputDirPath);
	stabImages = outDir.Read();
	
	vector<MaxCell> maxCells;
	
	/* HSV */
	CellFeatures cf = CellFeatures(20, true);
	cf.ExtractCellFeatures(this->cellArray, this->stabImages, 31, 0);
	for(int i=32; i<88; i++)
	{
		cf.CompareCells(this->cellArray, this->stabImages, i);
	}
		
	getchar();
	

	/* 3 Channel Color */
	CellFeatures cf2 = CellFeatures(20, true);
	cf2.ExtractCellColorHistogram(this->cellArray, this->stabImages, 31, 0);
	for(int i=32; i<99; i++)
	{
		cf2.CompareCells2(this->cellArray, this->stabImages, i);
	}

	cf2.ClipCellImages(this->stabImages, this->cellArray, 65);
	getchar();

	/* Gray Scale */
	CellFeatures cf3 = CellFeatures(20, true);
	cf3.ExtractCellGrayHistogram(this->cellArray, this->stabImages, 31, 0);
	for(int i=32; i<99; i++)
	{
		cf3.CompareCells3(this->cellArray, this->stabImages, i);
	}	
}
