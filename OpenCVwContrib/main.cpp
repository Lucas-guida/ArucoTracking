// OpenCVwContrib.cpp : This file contains the 'main' function. Program execution begins and ends there.
// Tracking v3
#include "pch.h"

#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>

#include "opencv2/core.hpp"
#//include "opencv2/imgcodecs.hpp"
//#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/aruco.hpp"
#include "opencv2/calib3d.hpp"

#include <iostream>
#include <iostream>
#include <fstream>
#include <math.h>


double pi = 3.14159265358979323846;

using namespace std;
using namespace cv;

Vec3f rotationMatrixToEulerAngles(Mat &R)
{
	float sy = sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));

	bool singular = sy < 1e-6; // If

	float x, y, z;
	if (!singular)
	{
		x = atan2(R.at<double>(2, 1), R.at<double>(2, 2));
		y = atan2(-R.at<double>(2, 0), sy);
		z = atan2(R.at<double>(1, 0), R.at<double>(0, 0));
	}
	else
	{
		x = atan2(-R.at<double>(1, 2), R.at<double>(1, 1));
		y = atan2(-R.at<double>(2, 0), sy);
		z = 0;
	}
	return Vec3f(x, y, z);
}


static bool readCameraParameters(string filename, Mat &camMatrix, Mat &distCoeffs) {
	FileStorage fs(filename, FileStorage::READ);
	if (!fs.isOpened())
		return false;
	fs["camera_matrix"] >> camMatrix;
	cout << camMatrix << endl;
	fs["distortion_coefficients"] >> distCoeffs;
	cout << distCoeffs << endl;
	return true;
}

static bool readDetectorParameters(string filename, Ptr<aruco::DetectorParameters> &params) {
	FileStorage fs(filename, FileStorage::READ);
	if (!fs.isOpened())
		return false;
	fs["adaptiveThreshWinSizeMin"] >> params->adaptiveThreshWinSizeMin;
	fs["adaptiveThreshWinSizeMax"] >> params->adaptiveThreshWinSizeMax;
	fs["adaptiveThreshWinSizeStep"] >> params->adaptiveThreshWinSizeStep;
	fs["adaptiveThreshConstant"] >> params->adaptiveThreshConstant;
	fs["minMarkerPerimeterRate"] >> params->minMarkerPerimeterRate;
	fs["maxMarkerPerimeterRate"] >> params->maxMarkerPerimeterRate;
	fs["polygonalApproxAccuracyRate"] >> params->polygonalApproxAccuracyRate;
	fs["minCornerDistanceRate"] >> params->minCornerDistanceRate;
	fs["minDistanceToBorder"] >> params->minDistanceToBorder;
	fs["minMarkerDistanceRate"] >> params->minMarkerDistanceRate;
	fs["cornerRefinementMethod"] >> params->cornerRefinementMethod;
	fs["cornerRefinementWinSize"] >> params->cornerRefinementWinSize;
	fs["cornerRefinementMaxIterations"] >> params->cornerRefinementMaxIterations;
	fs["cornerRefinementMinAccuracy"] >> params->cornerRefinementMinAccuracy;
	fs["markerBorderBits"] >> params->markerBorderBits;
	fs["perspectiveRemovePixelPerCell"] >> params->perspectiveRemovePixelPerCell;
	fs["perspectiveRemoveIgnoredMarginPerCell"] >> params->perspectiveRemoveIgnoredMarginPerCell;
	fs["maxErroneousBitsInBorderRate"] >> params->maxErroneousBitsInBorderRate;
	fs["minOtsuStdDev"] >> params->minOtsuStdDev;
	fs["errorCorrectionRate"] >> params->errorCorrectionRate;
	return true;
}

int main()
{
	int markersX = 3; //Number of squares in X direction
	int markersY = 3;//Number of squares in Y direction
	float markerLength = 0.085; //Marker side length(in meters)
	float markerSeparation = 0.012; //Separation between two consecutive markers in the grid(in meters)

	Ptr<aruco::DetectorParameters> detectorParams = aruco::DetectorParameters::create();
	readDetectorParameters("detector_params.yml", detectorParams);
	detectorParams->cornerRefinementMethod = aruco::CornerRefineMethod::CORNER_REFINE_NONE;

	Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_100);

	Mat camMatrix, distCoeffs;
	readCameraParameters("calib1080p.yml", camMatrix, distCoeffs);

	VideoCapture inputVideo;
	inputVideo.open(1);

	inputVideo.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P','G'));
	inputVideo.set(CAP_PROP_FRAME_WIDTH, 960);
	inputVideo.set(CAP_PROP_FRAME_HEIGHT, 720);

	cout << inputVideo.get(CAP_PROP_FPS) << endl;

	cout << inputVideo.get(CAP_PROP_FRAME_WIDTH) << endl;
	cout << inputVideo.get(CAP_PROP_FRAME_HEIGHT) << endl;

	while (true) {
		Mat image, imageCopy;

		inputVideo.read(image);

		Mat rotation;
		Vec3f euler;

		vector<int> ids;
		vector<vector<Point2f>> corners, rejected;
		vector<Vec3d> rvecs, tvecs;

		aruco::detectMarkers(image, dictionary, corners, ids, detectorParams, rejected);

		if (ids.size() > 0) {
			aruco::estimatePoseSingleMarkers(corners, markerLength, camMatrix, distCoeffs, rvecs, tvecs);
			cout << tvecs.at(0) << endl;
			//cout << rvecs.at(0) << endl;
			Rodrigues(rvecs.at(0), rotation);
			euler = rotationMatrixToEulerAngles(rotation);
			cout << euler*180.0/pi << endl;
		}
		image.copyTo(imageCopy);
		if (ids.size() > 0) {
				for (unsigned int i = 0; i < ids.size(); i++)
					aruco::drawAxis(imageCopy, camMatrix, distCoeffs, rvecs[i], tvecs[i], markerLength * 0.5f);
		}
		namedWindow("out", WINDOW_NORMAL);
		resizeWindow("out", 600, 600);
		imshow("out", imageCopy);
		char key = (char)waitKey(30);
		if (key == 27) break;
	}

	return 0;
}

