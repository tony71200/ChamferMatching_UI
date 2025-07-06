#include "PoseTransformer.h"

bool PoseTransformer::loadFromXML(const std::string& filename)
{
    cv::FileStorage fs(filename, cv::FileStorage::READ);
    if (!fs.isOpened()) {
        std::cerr << "❌ Không thể mở file: " << filename << "\n";
        return false;
    }

    fs["T_base_cam"] >> T_base_cam;
    fs.release();

    if (T_base_cam.empty() || T_base_cam.rows != 4 || T_base_cam.cols != 4) {
        std::cerr << "❌ Ma trận T_base_cam không hợp lệ\n";
        return false;
    }

    std::cout << "✅ Đã load T_base_cam:\n" << T_base_cam << "\n";
    return true;
}

// Convert 3D point in camera + theta (rad) → 6DOF robot pose
std::vector<double> PoseTransformer::convert(double x_cam, double y_cam, double z_cam, double theta_rad)
{
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

    // Position trong camera frame
    cv::Mat P_cam = (cv::Mat_<double>(4, 1) << x_cam, y_cam, z_cam, 1.0);
    cv::Mat P_base = T_base_cam * P_cam;

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
cv::Point3d PoseTransformer::convertPosition(double x_cam, double y_cam, double z_cam)
{
    cv::Mat P_cam = (cv::Mat_<double>(4, 1) << x_cam, y_cam, z_cam, 1.0);
    cv::Mat P_base = T_base_cam * P_cam;

    return cv::Point3d(
        P_base.at<double>(0),
        P_base.at<double>(1),
        P_base.at<double>(2));
}