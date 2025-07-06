#include "Calibration.h"
#include <QJsonArray>
#include <QTextStream>
#include <QDebug>

Calibration::Calibration() {}

bool Calibration::loadFromJson(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open calibration json file:" << filename;
        return false;
    }
    QByteArray data = file.readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return false;
    }
    if (!doc.isObject()) return false;
    return parseJson(doc.object());
}

bool Calibration::parseJson(const QJsonObject& obj) {
    if (!obj.contains("CameraMatrix")) return false;
    QJsonArray matArr = obj["CameraMatrix"].toArray();
    cameraMatrix_ = cv::Mat::zeros(3, 3, CV_64F);
    for (int i = 0; i < 3; ++i) {
        QJsonArray row = matArr[i].toArray();
        for (int j = 0; j < 3; ++j) {
            cameraMatrix_.at<double>(i, j) = row[j].toDouble();
        }
    }
    if (obj.contains("DistCoeffs")) {
        QJsonArray distArr = obj["DistCoeffs"].toArray();
        distCoeffs_ = cv::Mat::zeros(5, 1, CV_64F);
        for (int i = 0; i < 5; ++i) {
            distCoeffs_.at<double>(i, 0) = distArr[i].toDouble();
        }
    }
    // focus (fx, fy)
    focus_ = cameraMatrix_.at<double>(0, 0); // fx
    // center (cx, cy)
    center_.x = cameraMatrix_.at<double>(0, 2);
    center_.y = cameraMatrix_.at<double>(1, 2);
    // pixel/mm: giả sử tỉ lệ là fx (pixel) / 1mm nếu biết kích thước cảm biến thực tế, ở đây chỉ trả về fx
    pixelPerMM_ = focus_;
    return true;
}

bool Calibration::loadFromXml(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open calibration xml file:" << filename;
        return false;
    }
    QXmlStreamReader xml(&file);
    return parseXml(xml);
}

bool Calibration::parseXml(QXmlStreamReader& xml) {
    // Cần biết cấu trúc xml cụ thể, ở đây chỉ là khung mẫu
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "CameraMatrix") {
                cameraMatrix_ = cv::Mat::zeros(3, 3, CV_64F);
                for (int i = 0; i < 3; ++i) {
                    xml.readNextStartElement();
                    QJsonArray row;
                    QStringList vals = xml.readElementText().split(",");
                    for (int j = 0; j < 3; ++j) {
                        cameraMatrix_.at<double>(i, j) = vals[j].toDouble();
                    }
                }
            } else if (xml.name() == "DistCoeffs") {
                distCoeffs_ = cv::Mat::zeros(5, 1, CV_64F);
                QStringList vals = xml.readElementText().split(",");
                for (int i = 0; i < 5; ++i) {
                    distCoeffs_.at<double>(i, 0) = vals[i].toDouble();
                }
            }
        }
    }
    // focus (fx, fy)
    focus_ = cameraMatrix_.at<double>(0, 0); // fx
    // center (cx, cy)
    center_.x = cameraMatrix_.at<double>(0, 2);
    center_.y = cameraMatrix_.at<double>(1, 2);
    pixelPerMM_ = focus_;
    return true;
}

cv::Mat Calibration::getCameraMatrix() const {
    return cameraMatrix_;
}

cv::Mat Calibration::getDistCoeffs() const {
    return distCoeffs_;
}

double Calibration::getFocus() const {
    return focus_;
}

cv::Point2d Calibration::getImageCenter() const {
    return center_;
}

double Calibration::getPixelPerMM() const {
    return pixelPerMM_;
} 
