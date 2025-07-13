#ifndef ROBOTPOSETRANS_H
#define ROBOTPOSETRANS_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <QDebug>
#include "FileIO/json.hpp"

using json =nlohmann::json;
struct Pose{
    double x, y, z;
    double rx, ry, rz;

    Pose() : x(0), y(0), z(0), rx(0), ry(0), rz(0) {}
    Pose(double x, double y, double z, double rx, double ry, double rz)
        : x(x), y(y), z(z), rx(rx), ry(ry), rz(rz) {}
};

class RobotPoseTrans{
private:
    cv::Mat basecamM;

public:
    RobotPoseTrans(){}
    bool loadCameraParams(const std::string& filename, cv::Mat& K, cv::Mat& distCoeffs){
        std::ifstream file(filename);
        if(!file.is_open()) return false;

        json j;
        file >> j;

        auto mat = j["CameraMatrix"];
        K = (cv::Mat_<double>(3,3) <<
             mat[0][0], mat[0][1], mat[0][2],
            mat[1][0], mat[1][1], mat[1][2],
            mat[2][0], mat[2][1], mat[2][2]);

        auto d = j["DistCoeffs"];
        distCoeffs = (cv::Mat_<double>(1, 5) << d[0], d[1], d[2], d[3], d[4]);

        return true;
    }
    bool loadFromXML(const std::string& filename)
    {
        cv::FileStorage fs(filename, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cerr << "❌ Không thể mở file: " << filename << "\n";
            return false;
        }

        cv::FileNode node = fs["T_base_cam"];
        if(node.empty()){
            std::cerr << "Cannot find node 'T_base_cam'\n";
            return false;
        }

        int rows = (int)node["rows"];
        int cols = (int)node["cols"];
        std::string dt = (std::string)node["dt"];
        cv::FileNode data_node = node["data"];

        cv::Mat temp;
        if(!fs["T_base_cam"].empty()){
            fs["T_base_cam"] >> temp;
        }
        else{
            std::cerr << " Cannot find the key T_base_cam trong file XML. \n";
            return false;
        }

        try{
            this->basecamM = temp.clone();
            fs.release();
            std::cout << "✅ Đã load T_base_cam:\n" << this->basecamM << "\n";
        }
        catch(const cv::Exception& e){
            std::cerr << "Clone failed: " << e.what() << "\n";
            return false;
        }

        return true;
    }
    // Convert 3D point in camera + theta (rad) → 6DOF robot pose
    std::vector<double> convert(double x_cam, double y_cam, double z_cam, double theta_rad)
    {
        // Tạo ma trận quay quanh Z trong camera frame
        cv::Mat Rz_cam = (cv::Mat_<double>(3, 3) <<
            cos(theta_rad), -sin(theta_rad), 0,
            sin(theta_rad), cos(theta_rad), 0,
            0, 0, 1);

        // Lấy rotation từ camera → base
        cv::Mat R_cam2base = basecamM(cv::Rect(0, 0, 3, 3));
        cv::Mat R_obj_base = R_cam2base * Rz_cam;

        // Convert sang Rodrigues vector
        cv::Mat rvec_base;
        cv::Rodrigues(R_obj_base, rvec_base);

        // Position trong camera frame
        cv::Mat P_cam = (cv::Mat_<double>(4, 1) << x_cam, y_cam, z_cam, 1.0);
        cv::Mat P_base = basecamM * P_cam;

        // Trả về 6DOF pose [x, y, z, rx, ry, rz]
        std::vector<double> pose(6);
        pose[0] = P_base.at<double>(0);
        pose[1] = P_base.at<double>(1);
        pose[2] = P_base.at<double>(2);
        pose[3] = rvec_base.at<double>(0);
        pose[4] = rvec_base.at<double>(1);
        pose[5] = rvec_base.at<double>(2);
        return pose;
    }

    // Optional: Convert 3D point only (không cần theta)
    cv::Point3d convertPosition(double x_cam, double y_cam, double z_cam)
    {
        cv::Mat P_cam = (cv::Mat_<double>(4, 1) << x_cam, y_cam, z_cam, 1.0);
        cv::Mat P_base = basecamM * P_cam;

        return cv::Point3d(
            P_base.at<double>(0),
            P_base.at<double>(1),
                    P_base.at<double>(2));
    }
    Pose img_2_robot(double x, double y, double z_cam, double theta_deg){
        qDebug() << "Center x: " << x;
        qDebug() << "Center y: " << y;
        qDebug() << "Angle theta: " << theta_deg;

        Pose robot_pose;
        cv::Mat cameraMatrix, distCoeffs;
        std::string camera_matrix_path = "./CameraCalibration.json";
        std::ifstream file(camera_matrix_path);
        if(!file.is_open()) return robot_pose;

        json j;
        file >> j;

        auto mat = j["CameraMatrix"];
        cameraMatrix = (cv::Mat_<double>(3,3) <<
             mat[0][0], mat[0][1], mat[0][2],
            mat[1][0], mat[1][1], mat[1][2],
            mat[2][0], mat[2][1], mat[2][2]);

        auto d = j["DistCoeffs"];
        distCoeffs = (cv::Mat_<double>(1, 5) << d[0], d[1], d[2], d[3], d[4]);

        // Convert 2D (u,v,Z) to camera (x_cam, y_cam, z_cam)
        cv::Mat uv = (cv::Mat_<double>(3,1) << x, y, 1.0);
        cv::Mat K_inv = cameraMatrix.inv();

        cv::Mat xyz = K_inv * uv * z_cam;

        std::string bTc_path = "T_base_cam.xml";

        cv::FileStorage fs(bTc_path, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cerr << "❌ Không thể mở file: " << bTc_path << "\n";
            return robot_pose;
        }

        cv::Mat T_base_cam;
        if(!fs["T_base_cam"].empty()){
            fs["T_base_cam"] >> T_base_cam;
        }
        else{
            std::cerr << " Cannot find the key T_base_cam trong file XML. \n";
            return robot_pose;
        }

        // Convert camera to robot
        cv::Mat P_cam = (cv::Mat_<double>(4,1) <<
                         xyz.at<double>(0),
                         xyz.at<double>(1),
                         xyz.at<double>(2),
                         1.0);

        cv::Mat P_base = T_base_cam * P_cam;

//        double x_base = P_base.at<double>(0);
//        double y_base = P_base.at<double>(1);
//        double z_base = P_base.at<double>(2);

        double theta_rad = theta_deg * CV_PI / 180.0;

        //std::vector<double> pose = convert(x_cam, y_cam, z_cam, theta_rad);
        // Tạo ma trận quay quanh Z trong camera frame
        cv::Mat Rz_cam = (cv::Mat_<double>(3, 3) <<
            cos(theta_rad), -sin(theta_rad), 0,
            sin(theta_rad), cos(theta_rad), 0,
            0, 0, 1);

        // Lấy rotation từ camera → base
        cv::Mat R_cam2base = T_base_cam(cv::Rect(0, 0, 3, 3));
        cv::Mat R_obj_base = R_cam2base * Rz_cam;

        // Convert sang Rodrigues vector
        cv::Mat rvec_base;
        cv::Rodrigues(R_obj_base, rvec_base);

        // Trả về 6DOF pose [x, y, z, rx, ry, rz]
        robot_pose.x = P_base.at<double>(0);
        robot_pose.y = P_base.at<double>(1);
        robot_pose.z = P_base.at<double>(2);
        robot_pose.rx = rvec_base.at<double>(0);
        robot_pose.ry = rvec_base.at<double>(1);
        robot_pose.rz = rvec_base.at<double>(2);
        qDebug() << "Pose 1: " << robot_pose.x;
        qDebug() << "Pose 2: " << robot_pose.y;
        qDebug() << "Pose 3: " << robot_pose.z;
        qDebug() << "Pose 4: " << robot_pose.rx;
        qDebug() << "Pose 5: " << robot_pose.ry;
        qDebug() << "Pose 6: " << robot_pose.rz;

        return robot_pose;

    }
    Pose robotPoseCalculation(double x_cam, double y_cam, double theta_deg)
    {
        qDebug() << "x cam: " << x_cam;
        qDebug() << "y cam: " << y_cam;
        qDebug() << "theta: " << theta_deg;
        Pose robot_pose;
        std::string bTc_path = "T_base_cam.xml";

        cv::FileStorage fs(bTc_path, cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cerr << "❌ Không thể mở file: " << bTc_path << "\n";
            return robot_pose;
        }

        cv::Mat temp;
        if(!fs["T_base_cam"].empty()){
            fs["T_base_cam"] >> temp;
        }
        else{
            std::cerr << " Cannot find the key T_base_cam trong file XML. \n";
            return robot_pose;
        }

//        if(!loadFromXML(bTc_path)){
//            return robot_pose;
//        }

        double z_cam = 0.85;
        double theta_rad = theta_deg * CV_PI / 180.0;

        //std::vector<double> pose = convert(x_cam, y_cam, z_cam, theta_rad);
        // Tạo ma trận quay quanh Z trong camera frame
        cv::Mat Rz_cam = (cv::Mat_<double>(3, 3) <<
            cos(theta_rad), -sin(theta_rad), 0,
            sin(theta_rad), cos(theta_rad), 0,
            0, 0, 1);

        // Lấy rotation từ camera → base
        cv::Mat R_cam2base = temp(cv::Rect(0, 0, 3, 3));
        cv::Mat R_obj_base = R_cam2base * Rz_cam;

        // Convert sang Rodrigues vector
        cv::Mat rvec_base;
        cv::Rodrigues(R_obj_base, rvec_base);

        // Position trong camera frame
        cv::Mat P_cam = (cv::Mat_<double>(4, 1) << x_cam, y_cam, z_cam, 1.0);
        cv::Mat P_base = temp * P_cam;

        // Trả về 6DOF pose [x, y, z, rx, ry, rz]
        std::vector<double> pose(6);
        pose[0] = P_base.at<double>(0);
        pose[1] = P_base.at<double>(1);
        pose[2] = P_base.at<double>(2);
        pose[3] = rvec_base.at<double>(0);
        pose[4] = rvec_base.at<double>(1);
        pose[5] = rvec_base.at<double>(2);
        qDebug() << "Pose 1: " << pose[0];
        qDebug() << "Pose 2: " << pose[1];
        qDebug() << "Pose 3: " << pose[2];
        qDebug() << "Pose 4: " << pose[3];
        qDebug() << "Pose 5: " << pose[4];
        qDebug() << "Pose 6: " << pose[5];

        std::cout << "Robot Pose: [";
        for(double val : pose) std::cout << val << " ";
        std::cout << "]\n";

        return robot_pose;
    }
};

#endif // ROBOTPOSETRANS_H
