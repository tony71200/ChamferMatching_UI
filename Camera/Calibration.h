#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QXmlStreamReader>
#include <opencv2/core.hpp>
#include <vector>

class Calibration {
public:
    Calibration();
    bool loadFromJson(const QString& filename);
    bool loadFromXml(const QString& filename);

    cv::Mat getCameraMatrix() const;
    cv::Mat getDistCoeffs() const;
    double getFocus() const; // Trả về tiêu cự (fx hoặc fy)
    cv::Point2d getImageCenter() const; // Trả về tâm ảnh (cx, cy)
    double getPixelPerMM() const; // Trả về tỉ lệ pixel/mm

private:
    cv::Mat cameraMatrix_; // 3x3
    cv::Mat distCoeffs_;  // 5x1
    double focus_ = 0.0;
    cv::Point2d center_;
    double pixelPerMM_ = 0.0;
    bool parseJson(const QJsonObject& obj);
    bool parseXml(QXmlStreamReader& xml);
};

#endif // CALIBRATION_H 