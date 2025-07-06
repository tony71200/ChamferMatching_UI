#ifndef IMAGEPARAMS_H
#define IMAGEPARAMS_H


struct ImageParams
{
    int threshold_value = 128;
    int threshold_type = 0;
    int blur_type = 0;  // 0: gaussian, 1: median
    int blur_size = 3;
    int blur_amp = 0;
    int canny_th1 = 80; // Canny recommended a upper:lower ratio between 2:1 and 3:1.
    int canny_th2 = 160;
    int clahe_of_off = 0;

    int clahe_size = 0;
    int clahe_limit = 0;
    int percent_contours = 78;
    int image_width = 1280;
    int image_height = 1024;

    int max_threshold_value = 255;
    int max_binary_value = 255;
    int max_percent_contours = 100;

    // Jang 20220629
    int dilate_erode_size = 1;
    int downsample_step = 1;
    int reduce_param_scale = 3;
    // Jang 20220712
    int blur_canny = 4; // combination of blur and canny
};

#endif // IMAGEPARAMS_H
