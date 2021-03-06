////////////////////////////////////////////////
// IOT Aruco Tracker Micro Server
//
// by: Lucas Santaguida
// Date: 04/09/2018
//
// This program uses OpenCV and the Aruco libray
// to track Aruco markers and determine the pose
// of the camera with respect to the markers. 
// The application is wrapped by the mongoose IOT
// webservice module to serve tracked Aruco marker
// data to external application using a REST API
///////////////////////////////////////////////


// dependent files and libraires
#include "pch.h"

#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/aruco.hpp"
#include "opencv2/calib3d.hpp"

#include <iostream>
#include <iostream>
#include <fstream>
#include <math.h>

//#include <ctime>
#include <chrono>

#include "mongoose.h"


// Const PI as math.h M_PI const was not working
double pi = 3.14159265358979323846;

// Port that the application will serve request on
static const char *s_http_port = "8000";

float markerLength = 0.050; //Marker side length(in meters)

using namespace std;
using namespace cv;

Mat TAC = Mat::zeros(4,4,CV_32F);
Mat rotation = Mat::zeros(3,3,CV_32F);
Vec3f tvec;


Ptr<aruco::DetectorParameters> detectorParams;
Ptr<aruco::Dictionary> dictionary;
Mat camMatrix, distCoeffs;

VideoCapture inputVideo;

Mat image, imageCopy;

std::chrono::time_point<std::chrono::system_clock> start, endTime;

// Message event handeler for IOT Webserver
void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
	if (ev == MG_EV_HTTP_REQUEST) {

		start = std::chrono::system_clock::now();
		inputVideo.read(image);
		image.copyTo(imageCopy);

		Vec3f euler;

		// store all markers recorded and output rotation nad translation vector
		vector<int> ids;
		vector<vector<Point2f>> corners, rejected;
		vector<Vec3d> rvecs, tvecs;

		// detection of aruco markers
		aruco::detectMarkers(image, dictionary, corners, ids, detectorParams, rejected);

		// writes a single marker information to the command prompet. 
		// If there are more than one marker is in the display it will only write one.
		if (ids.size() > 0) {
			aruco::estimatePoseSingleMarkers(corners, markerLength, camMatrix, distCoeffs, rvecs, tvecs);
			tvec = tvecs.at(0);
			cout << tvecs.at(0) << endl;
			//cout << rvecs.at(0) << endl;
			Rodrigues(rvecs.at(0), rotation);
			//cout << tvec[0] << endl;
			//euler = rotationMatrixToEulerAngles(rotation);
			//cout << euler*180.0/pi << endl;
		}

		endTime = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = endTime - start;

		cout << elapsed_seconds.count() << endl;

		struct http_message *hm = (struct http_message *) ev_data;
		char addr[32];

		mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
			MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

		printf("%p: %.*s %.*s\r\n", nc, (int)hm->method.len, hm->method.p,
			(int)hm->uri.len, hm->uri.p);


		//Header information to web resquest/response
		//In the below case an xml file will be served
		mg_send_response_line(nc, 200,
			"Content-Type: text/xml\r\n"
			"Connection: close");
		
		// data to be sent through the webservice in XML format
		std::string output = "\r\n<data><row><c1>" + std::to_string(rotation.at<double>(0,0)) + "</c1><c2>" + std::to_string(rotation.at<double>(0, 1)) + "</c2><c3>"+ std::to_string(rotation.at<double>(0, 2)) +"</c3><c4>" + std::to_string(tvec[0]) + "</c4></row>"+
			"<row><c1>" + std::to_string(rotation.at<double>(1, 0)) + "</c1><c2>" + std::to_string(rotation.at<double>(1, 1)) + "</c2><c3>"+ std::to_string(rotation.at<double>(1, 2)) +"</c3><c4>" + std::to_string(tvec[1]) + "</c4></row>"+
			"<row><c1>" + std::to_string(rotation.at<double>(2, 0)) + "</c1><c2>" + std::to_string(rotation.at<double>(2, 1)) + "</c2><c3>" + std::to_string(rotation.at<double>(2, 2)) + "</c3><c4>" + std::to_string(tvec[2]) + "</c4></row>" +
			"<row><c1>" + std::to_string(0) + "</c1><c2>" + std::to_string(0) + "</c2><c3>" + std::to_string(0) + "</c3><c4>" + std::to_string(1) + "</c4></row></data>" +
			"\r\n";
		int n = output.length();
		char char_array[1000];
		strcpy_s(char_array, output.c_str());

		mg_printf(nc,
			char_array,
			addr, (int)hm->uri.len, hm->uri.p);
		nc->flags |= MG_F_SEND_AND_CLOSE;
	}
}

// Converts the 3x3 rotation matrix into 3 euler angles
Vec3f rotationMatrixToEulerAngles(Mat &R)
{
	float sy = sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) + R.at<double>(1, 0) * R.at<double>(1, 0));

	bool singular = sy < 1e-6;

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

// Reads file for Camera Matrix and Distortion Parameters 
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

// Reads file for Aruco Detection Parameters
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
	//setup of IOT webservice
	struct mg_mgr mgr;
	struct mg_connection *c;
	mg_mgr_init(&mgr, NULL);
	c = mg_bind(&mgr, s_http_port, ev_handler);
	mg_set_protocol_http_websocket(c);

	

	// Cration and reading of detector parameters
	detectorParams = aruco::DetectorParameters::create();
	readDetectorParameters("detector_params.yml", detectorParams);
	detectorParams->cornerRefinementMethod = aruco::CornerRefineMethod::CORNER_REFINE_NONE;

	// seletion of marker search dictionary. DICT_4x4_100 is the smallest dictionary
	// with a 4x4 pixel grid requiring less processing. 
	dictionary = aruco::getPredefinedDictionary(aruco::DICT_4X4_100);

	// reading of camera matrix and distortion coefficients form calibration file. Callibration
	// was compleated using Matlab
	
	readCameraParameters("calib1080p.yml", camMatrix, distCoeffs);

	// Initating video capture
	inputVideo.open(0);

	// setting the video capture frame size and encoding type
	inputVideo.set(CAP_PROP_FOURCC, VideoWriter::fourcc('H', '2', '6','4'));
	inputVideo.set(CAP_PROP_FRAME_WIDTH, 640);
	//inputVideo.set(CAP_PROP_FRAME_HEIGHT, 1080);

	// returning video caputre fps and fame size
	cout << inputVideo.get(CAP_PROP_FPS) << endl;
	cout << inputVideo.get(CAP_PROP_FOURCC) << endl;
	cout << inputVideo.get(CAP_PROP_FRAME_WIDTH) << endl;
	cout << inputVideo.get(CAP_PROP_FRAME_HEIGHT) << endl;

	//clock_t start;
	//double duration;

	

	//main loop
	while (true) {
			

			// drawas axis on the marker in the window
			//if (ids.size() > 0) {
			//	for (unsigned int i = 0; i < ids.size(); i++)
			//		aruco::drawAxis(imageCopy, camMatrix, distCoeffs, rvecs[i], tvecs[i], markerLength * 0.5f);
			//}
		// wrties xml data to port 8000
		mg_mgr_poll(&mgr, 10);

		//duration = (clock() - start) / (double)CLOCKS_PER_SEC;
		//cout << duration << endl;
			
		//namedWindow("out", WINDOW_NORMAL);
		//resizeWindow("out", 960 / 2, 720 / 2);
		//imshow("out", image);
		
	}
	mg_mgr_free(&mgr);
	return 0;
}

