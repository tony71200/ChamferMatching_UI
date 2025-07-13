#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QXmlStreamReader>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/ccalib.hpp>
#include <vector>
#include <algorithm>

class Calibration {
public:
    Calibration();
    ~Calibration();
    bool loadFromJson(const QString& filename);
    bool loadFromXml(const QString& filename);
    void clear();

    cv::Mat getCameraMatrix() const;
    cv::Mat getDistCoeffs() const;
    double getFocus() const; // Return focus (fx hoặc fy)
    cv::Point2d getImageCenter() const; // (cx, cy)
    double getMmPerPixel() const; // mmPerPixel = distanceCamera*10 / sqrt(fx^2 + fy^2)
    float getDistanceCamera() const; // cm
    cv::Mat undistortImage(const cv::Mat& input) const;

private:
    cv::Mat cameraMatrix_; // 3x3
    cv::Mat distCoeffs_;  // 5x1
    double focus_ = 0.0;
    cv::Point2d center_;
    double mmPerPixel_ = 0.08;
    float distanceCamera_ = 0.0f; // cm
    std::vector<float> tvecs_item2_; // các item2 của TVecs
    bool parseJson(const QJsonObject& obj);
    bool parseXml(QXmlStreamReader& xml);
    void processTvecs();
};

#endif // CALIBRATION_H 
