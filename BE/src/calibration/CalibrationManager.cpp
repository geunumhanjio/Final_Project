#include "calibration/CalibrationManager.h"
#include <random>

CalibrationManager::CalibrationManager() : isCalibrated(false) {
}

CalibrationManager::~CalibrationManager() {
}

void CalibrationManager::performCalibration() {
    // [Scenario] Random coordinate pairs for testing as requested
    std::vector<cv::Point2f> srcPoints;
    std::vector<cv::Point2f> dstPoints;

    // Generating some random mock points for source (Pixel coordinates)
    // Assuming a 1920x1080 resolution for example
    srcPoints.push_back(cv::Point2f(100, 100));
    srcPoints.push_back(cv::Point2f(1800, 100));
    srcPoints.push_back(cv::Point2f(1800, 900));
    srcPoints.push_back(cv::Point2f(100, 900));

    // Generating some random mock points for destination (World coordinates/Transformed view)
    // Just applying some offset/scale for the "Random" test aspect
    dstPoints.push_back(cv::Point2f(0, 0));
    dstPoints.push_back(cv::Point2f(500, 0));
    dstPoints.push_back(cv::Point2f(500, 500));
    dstPoints.push_back(cv::Point2f(0, 500));

    // Calculate Homography
    // In a real scenario, these points would come from user input or detection
    homographyMatrix = cv::findHomography(srcPoints, dstPoints);
    isCalibrated = true;

    std::cout << "[Calibration] Homography Matrix Calculated:\n" << homographyMatrix << std::endl;
}

cv::Point2f CalibrationManager::apply(const cv::Point2f& inputPoint) {
    if (!isCalibrated) {
        std::cerr << "[Calibration] Error: Not calibrated yet!" << std::endl;
        return inputPoint;
    }

    std::vector<cv::Point2f> inPts, outPts;
    inPts.push_back(inputPoint);

    // Apply the perspective transformation
    cv::perspectiveTransform(inPts, outPts, homographyMatrix);

    return outPts[0];
}
