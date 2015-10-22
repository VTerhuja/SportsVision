/* 
* Author	: Vikedo Terhuja
* Date		: September 2014
* Summary	: Utility for reading all types of files.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
using namespace std;

/* Constants */
const char SPACE_SYMBOL = ' ';
const char HASH_SYMBOL = '#';

/* Enumerations and Data Structures */
enum FrameStatus
{
	START,
	STILL,
	LEFT2RIGHT,
	RIGHT2LEFT,
	TIMEOUT,
	TRANSITION,
	LEFT_START,
	LEFT_END,
	RIGHT_START,
	RIGHT_END,
};

enum SegmentType
{
	LEFT_SEG,
	RIGHT_SEG,
	TRANS_SEG,
};

struct PossessionMarker
{
	long frameNumber;
	FrameStatus status;
};

/* Side Detector Difference */
struct SDDiff
{
	long frameNumber;
	float value;
};

struct ThreeSignal
{
	long frameNumber;
	int lMatches;
	int rMatches;
	int mMatches;
};

struct VideoSegment
{
	long		startFrame;
	long		endFrame;
	SegmentType type;
	int			length;
};

typedef map<int,std::string> filesMap;

/************************************************************************
/* Class name	: FileReader
/* Description	: Loads and reads Ground Truth & SideDetector Data files.
/***********************************************************************/
class FileReader
{
private: 
	const char* fileName;
	bool bGroundTruth;
	filesMap videoFiles;

private:
	int GetLinesCount(string fileName);
	VideoSegment ExtractSegmentData(string rowString);

public:		
	FileReader(const char* name, bool bGT);
	FileReader(const char* name);
		
	long ExtractFrameNumber(string rowString, int &nextIndex);

	/* Ground Truth */
	PossessionMarker* ExtractPossessionMarker(int &numberOfMarkers);
	void ExtractFrameStatus(string rowString, PossessionMarker &pM, int nextIndex);

	/* Side detector difference */
	SDDiff* ExtractSideDetectorData(int &numberOfMarkers, int iNormalize, int &min, int &max);
	int ExtractSDDValue(string rowString, SDDiff &oF, int nextIndex);

	/* 3 signals */
	ThreeSignal* Extract3Signals(int &count);
	int ExtractSignal(string rowString, int &nextIndex);

	/* Extract Left/Right segments */
	vector<VideoSegment> ExtractSegments();

	/* Read file names of all videos */
	void ReadVideoFileNames();

	/* Read all the full path of the video files and assign id */
	string GetVideoFileName(int gId);
};