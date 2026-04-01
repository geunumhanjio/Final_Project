#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>

class CalibrationManager {
public:
    CalibrationManager();
    ~CalibrationManager();

    // Performs the initial calibration (calculates homography matrix)
    void performCalibration();

    // Applies the calibration matrix to a point
    cv::Point2f apply(const cv::Point2f& inputPoint);

private:
    cv::Mat homographyMatrix;
    bool isCalibrated;
};

#endif // CALIBRATION_MANAGER_H
