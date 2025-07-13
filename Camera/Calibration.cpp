#include "Calibration.h"
#include <QJsonArray>
#include <QDebug>
#include <cmath>

Calibration::Calibration() {}
Calibration::~Calibration() { clear(); }

void Calibration::clear() {
    cameraMatrix_.release();
    distCoeffs_.release();
    tvecs_item2_.clear();
    focus_ = 0.0;
    center_ = cv::Point2d();
    mmPerPixel_ = 0.08;
    distanceCamera_ = 0.0f;
}

bool Calibration::loadFromJson(const QString& filename) {
    clear();
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
    center_.x = cameraMatrix_.at<double>(0, 2);
    center_.y = cameraMatrix_.at<double>(1, 2);
    // TVecs
    tvecs_item2_.clear();
    if (obj.contains("TVecs")) {
        QJsonArray tvecsArr = obj["TVecs"].toArray();
        for (const auto& tvecVal : tvecsArr) {
            QJsonObject tvecObj = tvecVal.toObject();
            if (tvecObj.contains("Item2")) {
                tvecs_item2_.push_back(static_cast<float>(tvecObj["Item2"].toDouble()));
            }
        }
        processTvecs();
    }
    return true;
}

bool Calibration::loadFromXml(const QString& filename) {
    clear();
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open calibration xml file:" << filename;
        return false;
    }
    QXmlStreamReader xml(&file);
    return parseXml(xml);
}

bool Calibration::parseXml(QXmlStreamReader& xml) {
    cameraMatrix_ = cv::Mat::zeros(3, 3, CV_64F);
    distCoeffs_ = cv::Mat::zeros(5, 1, CV_64F);
    tvecs_item2_.clear();
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "CameraMatrix") {
                for (int i = 0; i < 3; ++i) {
                    xml.readNextStartElement();
                    QStringList vals = xml.readElementText().split(",");
                    for (int j = 0; j < 3; ++j) {
                        cameraMatrix_.at<double>(i, j) = vals[j].toDouble();
                    }
                }
            } else if (xml.name() == "DistCoeffs") {
                QStringList vals = xml.readElementText().split(",");
                for (int i = 0; i < 5; ++i) {
                    distCoeffs_.at<double>(i, 0) = vals[i].toDouble();
                }
            } else if (xml.name() == "TVec") {
                float item2 = 0.0f;
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "TVec")) {
                    xml.readNext();
                    if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "Item2") {
                        item2 = xml.readElementText().toFloat();
                        tvecs_item2_.push_back(item2);
                    }
                }
            }
        }
    }
    processTvecs();
    // focus (fx, fy)
    focus_ = cameraMatrix_.at<double>(0, 0); // fx
    center_.x = cameraMatrix_.at<double>(0, 2);
    center_.y = cameraMatrix_.at<double>(1, 2);
    return true;
}

void Calibration::processTvecs() {
    if (tvecs_item2_.empty()) {
        distanceCamera_ = 0.0f;
        return;
    }
    std::sort(tvecs_item2_.begin(), tvecs_item2_.end(), std::greater<float>());
    size_t trim = tvecs_item2_.size() / 10;
    size_t start = trim;
    size_t end = tvecs_item2_.size() - trim;
    if (end <= start) { start = 0; end = tvecs_item2_.size(); }
    float sum = 0.0f;
    for (size_t i = start; i < end; ++i) sum += tvecs_item2_[i];
    distanceCamera_ = sum / (end - start);
}

cv::Mat Calibration::getCameraMatrix() const { return cameraMatrix_; }
cv::Mat Calibration::getDistCoeffs() const { return distCoeffs_; }
double Calibration::getFocus() const { return focus_; }
cv::Point2d Calibration::getImageCenter() const { return center_; }
float Calibration::getDistanceCamera() const { return distanceCamera_; }

double Calibration::getMmPerPixel() const {
    double fx = cameraMatrix_.at<double>(0, 0);
    double fy = cameraMatrix_.at<double>(1, 1);
    if (fx == 0 && fy == 0) return 0.0;
    return distanceCamera_ * 10.0 / std::sqrt(fx * fx + fy * fy);
}

cv::Mat Calibration::undistortImage(const cv::Mat& input) const {
    cv::Mat output;
    if (!cameraMatrix_.empty() && !distCoeffs_.empty()) {
        cv::undistort(input, output, cameraMatrix_, distCoeffs_);
    } else {
        output = input.clone();
    }
    return output.clone();
} 
