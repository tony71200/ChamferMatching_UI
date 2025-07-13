#ifndef ALIGNMENT_H
#define ALIGNMENT_H

#include <QPoint>
#include <cmath>

struct AlignmentResult {
    float dx_robot; // mm
    float dy_robot; // mm
    float angle_deg; // độ lệch panel
    float dx_image_mm; // mm
    float dy_image_mm; // mm
};

class Alignment {
public:
    // Hàm tính toán dịch chuyển robot dựa trên thông tin panel và camera
    static AlignmentResult compute(const QPoint& trained_center, const QPoint& found_center,
                                   float angle_deg, double mmPerPixels, float a) {
        AlignmentResult res;
        // 1. Tính dx, dy trên ảnh (pixel)
        float dx_image = found_center.x() - trained_center.x();
        float dy_image = found_center.y() - trained_center.y();
        // 2. Quy đổi sang mm
        res.dx_image_mm = dx_image * mmPerPixels;
        res.dy_image_mm = dy_image * mmPerPixels;
        // 3. Tính dx_robot, dy_robot theo công thức bạn cung cấp
        float theta = angle_deg * M_PI / 180.0f;
        res.dx_robot = a * (std::cos(theta) -1 ) - res.dy_image_mm;
        res.dy_robot = a * std::sin(theta) - res.dx_image_mm;
        res.angle_deg = angle_deg;
        return res;
    }
};

#endif // ALIGNMENT_H 
