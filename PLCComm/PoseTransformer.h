
#ifndef POSE_TRANSFORMER_HPP
#define POSE_TRANSFORMER_HPP

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <cmath>
class PoseTransformer
{
private:
	cv::Mat T_base_cam;  // 4x4 transformation matrix

public:
	PoseTransformer() {}
	bool loadFromXML(const std::string& filename);
	std::vector<double> convert(double x_cam, double y_cam, double z_cam, double theta_rad);
	cv::Point3d convertPosition(double x_cam, double y_cam, double z_cam);
};

#endif // POSE_TRANSFORMER_HPP

