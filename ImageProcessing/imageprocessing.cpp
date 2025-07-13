#include "imageprocessing.h"

ImageProcessing::ImageProcessing()
{
    skip_blur_ = true;
    skip_threshold_contour_ = false;
    save_path = "./";
}

ImageProcessing::~ImageProcessing()
{
}

bool ImageProcessing::SetSaveDir(std::string path)
{
    save_path = path;
    if(!LoadParams(current_iparam))
        SaveParams(current_iparam);
    return true;
}

void ImageProcessing::InitLog(std::string dir_path, std::string name){
    lw.SetDirPath(dir_path);
    lw.SetLogName(name);
    lw.SetShowDate(true);
}

std::string ImageProcessing::GetSaveDir()
{
    return save_path;
}

void ImageProcessing::SkipBlurContour(bool skip_blur, bool skip_thres_contour){
    skip_blur_ = skip_blur;
    skip_threshold_contour_ = skip_thres_contour;
}

void ImageProcessing::UploadImage(cv::Mat img)
{
    orig_img = img.clone();
    processed_img = img.clone();

}

bool CompareSize(std::vector<cv::Point> cont_0, std::vector<cv::Point> cont_1)
{
    return (cv::arcLength(cont_0, true) > cv::arcLength(cont_1, true));
}

void ImageProcessing::SaveParams(ImageParams iparam) {
    cv::FileStorage fs;
    try {
        if(!stdfs::exists(save_path)) stdfs::create_directories(save_path);
        if (fs.open(save_path + "/imgproc_params.txt", cv::FileStorage::WRITE)) {
            fs << "threshold_value" << iparam.threshold_value;
            fs << "threshold_type" << iparam.threshold_type;

            fs << "blur_type" << iparam.blur_type;
            fs << "blur_size" << iparam.blur_size;
            fs << "blur_amp" << iparam.blur_amp;

            fs << "canny_th1" << iparam.canny_th1;
            fs << "canny_th2" << iparam.canny_th2;

            fs << "clahe_of_off" << iparam.clahe_of_off;
            fs << "clahe_size" << iparam.clahe_size;
            fs << "clahe_limit" << iparam.clahe_limit;

            fs << "image_width" << iparam.image_width;
            fs << "image_height" << iparam.image_height;


            fs << "percent_contours" << iparam.percent_contours;
            fs << "max_percent_contours" << iparam.max_percent_contours;

            // Jang 20220628
            fs << "dilate_erode_size" << iparam.dilate_erode_size;
            fs << "downsample_step" << iparam.downsample_step;
            fs << "reduce_param_scale" << iparam.reduce_param_scale;
            fs << "blur_canny" << iparam.blur_canny;    // Jang 20220712

//            cv::imwrite(save_path + "/0_orig_img.png", orig_img);
//            cv::imwrite(save_path + "/4_contour_img.png", processed_img);
        }

        else {
            std::cout << "The file couldn't be opened: " << save_path  << ", line " << __LINE__ << std::endl;
        }

    }
    catch (cv::Exception e) {
        std::cout << "FileStorage error: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "The file couldn't be opened: " << save_path  << ", line " << __LINE__ << std::endl;
    }
    fs.release();
}

bool ImageProcessing::LoadParams(ImageParams& iparam){

    cv::FileStorage fs;
    try {
        if (fs.open(save_path + "/imgproc_params.txt", cv::FileStorage::READ)) {
            fs["threshold_value"] >> iparam.threshold_value;
            fs["threshold_type"] >> iparam.threshold_type;

            fs["blur_type"] >> iparam.blur_type;
            fs["blur_size"] >> iparam.blur_size;
            fs["blur_amp"] >> iparam.blur_amp;

            fs["canny_th1"] >> iparam.canny_th1;
            fs["canny_th2"] >> iparam.canny_th2;

            fs["clahe_of_off"] >> iparam.clahe_of_off;
            fs["clahe_size"] >> iparam.clahe_size;
            fs["clahe_limit"] >> iparam.clahe_limit;

            fs["image_width"] >> iparam.image_width;
            fs["image_height"] >> iparam.image_height;

            fs["percent_contours"] >> iparam.percent_contours;
            fs["max_percent_contours"] >> iparam.max_percent_contours;

            // Jang 20220628
            fs["dilate_erode_size"] >> iparam.dilate_erode_size;
            fs["downsample_step"] >> iparam.downsample_step;
            fs["reduce_param_scale"] >> iparam.reduce_param_scale;
            fs["blur_canny"] >> iparam.blur_canny; // Jang 20220712
        }

        else {
            std::cout << "The file couldn't be opened "  << save_path << ", line " << __LINE__ << std::endl;
            std::cout << __func__ << ", line " << __LINE__ << std::endl;
            return false;
        }

    }
    catch (cv::Exception e) {
        std::cout << "FileStorage error: " << e.what() << std::endl;
        if(fs.isOpened()) fs.release();
        return false;
    }
    fs.release();
    return true;
}


bool ImageProcessing::LoadINIParams(std::ifstream &fs, ImageParams &iparam)
{
//    try{
//        fm::ReadINIText(fs, "ImgProcParams", "threshold_value", iparam.threshold_value);
//        fm::ReadINIText(fs, "ImgProcParams", "threshold_type", iparam.threshold_type);

//        fm::ReadINIText(fs, "ImgProcParams", "blur_type", iparam.blur_type);
//        fm::ReadINIText(fs, "ImgProcParams", "blur_size", iparam.blur_size);
//        fm::ReadINIText(fs, "ImgProcParams", "blur_amp", iparam.blur_amp);

//        fm::ReadINIText(fs, "ImgProcParams", "canny_th1", iparam.canny_th1);
//        fm::ReadINIText(fs, "ImgProcParams", "canny_th2", iparam.canny_th2);

//        fm::ReadINIText(fs, "ImgProcParams", "clahe_of_off", iparam.clahe_of_off);
//        fm::ReadINIText(fs, "ImgProcParams", "clahe_size", iparam.clahe_size);
//        fm::ReadINIText(fs, "ImgProcParams", "clahe_limit", iparam.clahe_limit);

//        fm::ReadINIText(fs, "ImgProcParams", "image_width", iparam.image_width);
//        fm::ReadINIText(fs, "ImgProcParams", "image_height", iparam.image_height);

//        fm::ReadINIText(fs, "ImgProcParams", "percent_contours", iparam.percent_contours);
//        fm::ReadINIText(fs, "ImgProcParams", "max_percent_contours", iparam.max_percent_contours);
//        // Jang 20220628
//        fm::ReadINIText(fs, "ImgProcParams", "dilate_erode_size", iparam.dilate_erode_size);
//        fm::ReadINIText(fs, "ImgProcParams", "downsample_step", iparam.downsample_step);
//        fm::ReadINIText(fs, "ImgProcParams", "reduce_param_scale", iparam.reduce_param_scale);
//        fm::ReadINIText(fs, "ImgProcParams", "blur_canny", iparam.blur_canny);   // Jang 20220712

//    } catch(std::exception e){
//        std::cout << "LoadINIParams error: " << e.what() << std::endl;
//        return false;
//    }
    return true;
}

void ImageProcessing::ProcessContour(cv::Mat& img, cv::Mat& out_img, ImageParams iparam){
    cv::Mat final_img;
    if(iparam.percent_contours <= 0){
//        img.copyTo(out_img);
        out_img = img.clone();
    }
    else{

        final_img = cv::Mat::zeros(img.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;

        // Jang 20220330
//        cv::findContours(img, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
        cv::findContours(img, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

        size_t num_contours = contours.size();
        if(num_contours < 1){
            lw << "[ERROR] No contours detected."
               << " Code file \"" << __FILE__ << "\", line " << __LINE__ << ", in " << __func__ << lw.endl;
//            img.copyTo(out_img);
            out_img = img.clone();
        }

        sort(contours.begin(), contours.end(), CompareSize);
        if(skip_threshold_contour_){
            // skip
        }
        else{
            float threshold_contour = (float)img.cols - ((float)img.cols * (float)iparam.percent_contours / (float)iparam.max_percent_contours);
            for(int i = 0; i < num_contours ; i++){
                if(threshold_contour > cv::arcLength(contours.at(i), true)){
    //                lw << "threshold_contour: " << threshold_contour << lw.endl;
    //                lw << "cv::arcLength(contours.at(i), true): " << cv::arcLength(contours.at(i), true) << lw.endl;
    //                lw << "i: " << i << lw.endl;
    //                lw << "iparam.percent_contours: " << iparam.percent_contours << lw.endl;
    //                lw << "iparam.max_percent_contours: " << iparam.max_percent_contours << lw.endl;
                    contours.erase(contours.begin() + i, contours.end());
                    break;
                }
            }
        }




        int contour_thickness;
    //    contour_thickness = final_img.cols / 400;
    //    if( contour_thickness < 1) contour_thickness = 1;
        contour_thickness = 1;

        // Draw all contours
        cv::drawContours(final_img, contours, -1, (255, 255, 255), contour_thickness, cv::LINE_8);

        // Jang 20220330
        current_contours_.clear();
        current_contours_ = contours;
        out_img = final_img.clone();
    }

    skip_blur_ = false;
    skip_threshold_contour_ = false;
}

void ImageProcessing::GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, bool use_cuda){
    ImageParams iparam;
    if(!LoadParams(iparam)){
        SaveParams(iparam);
        lw << "[WARNING] No parameter data detected for image processing. The data is saved with default parameters." << lw.endl;
    }

    if(use_cuda){
        GetImageBySavedInfo(img, out_img, iparam, use_cuda);
    }
    else{
        GetImageBySavedInfo(img, out_img, iparam);
    }
}

void ImageProcessing::GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, ImageParams iparam){
    if (img.empty()) return;
    cv::Mat converted_img;
    if(img.channels() > 1)
        cv::cvtColor(img, converted_img, cv::COLOR_BGR2GRAY);
    else
        converted_img = img.clone();

    if(iparam.threshold_value > 0)
        cv::threshold(converted_img, converted_img, iparam.threshold_value, iparam.max_binary_value, iparam.threshold_type);

    if(skip_blur_){
        // no need blur
    }
    else{
        if(iparam.blur_type == 0 ){
            cv::GaussianBlur(converted_img, converted_img, cv::Size(iparam.blur_size * 2 + 1, iparam.blur_size * 2 + 1), iparam.blur_amp);
        }else if(iparam.blur_type == 1){
            cv::medianBlur(converted_img, converted_img, iparam.blur_size * 2 + 1);
        }
    }

    if (iparam.clahe_of_off > 0) {
        if (iparam.clahe_size > 1) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(iparam.clahe_limit, cv::Size(iparam.clahe_size, iparam.clahe_size));
            clahe->apply(converted_img, converted_img);
        }
    }

    cv::Canny(converted_img, converted_img, iparam.canny_th1, iparam.canny_th2, 3, true);

    if(iparam.dilate_erode_size > 0){
        cv::morphologyEx(converted_img, converted_img, cv::MORPH_DILATE,
                        cv::Mat::ones(cv::Size(iparam.dilate_erode_size, iparam.dilate_erode_size) , CV_8UC1));
        cv::morphologyEx(converted_img, converted_img, cv::MORPH_ERODE,
                        cv::Mat::ones(cv::Size(iparam.dilate_erode_size, iparam.dilate_erode_size) , CV_8UC1));
    }
    ProcessContour(converted_img, out_img, iparam);
}



void ImageProcessing::GetImageBySavedInfo(cv::Mat& img, cv::Mat& out_img, ImageParams iparam, bool use_cuda){
#ifdef OPENCV4_CUDA
    if(use_cuda){

        cv::cuda::GpuMat converted_img;
        cv::Mat final_img;
        cv::Mat gray_img;
        if(img.channels() > 1)
            cv::cvtColor(img, gray_img, cv::COLOR_BGR2GRAY);
        else
            gray_img = img.clone();

        // There is no OSTU method in CUDA, so that cpu Mat is used.
        if(iparam.threshold_value > 0)
            cv::threshold(gray_img, gray_img, iparam.threshold_value, iparam.max_binary_value, iparam.threshold_type);


        converted_img.upload(gray_img);

        if(skip_blur_){
            // no need blur
        }
        else{
            cv::Ptr<cv::cuda::Filter> cuda_filter;
            if(iparam.blur_type == 0 ){
                cuda_filter = cv::cuda::createGaussianFilter(converted_img.type(), converted_img.type(),
                                                                                   cv::Size(iparam.blur_size * 2 + 1, iparam.blur_size * 2 + 1), iparam.blur_amp);
            }
            else{
                cuda_filter = cv::cuda::createMedianFilter(converted_img.type(), iparam.blur_size * 2 + 1);
            }
            cuda_filter->apply(converted_img, converted_img);
        }
        if (iparam.clahe_of_off > 0) {
            if (iparam.clahe_size > 1) {
                cv::Ptr<cv::cuda::CLAHE> clahe = cv::cuda::createCLAHE(iparam.clahe_limit, cv::Size(iparam.clahe_size, iparam.clahe_size));
                clahe->apply(converted_img, converted_img);
            }
        }
        cv::Ptr<cv::cuda::CannyEdgeDetector> canny_detector = cv::cuda::createCannyEdgeDetector(iparam.canny_th1, iparam.canny_th2);
        canny_detector->detect(converted_img, converted_img);

        final_img = cv::Mat::zeros(converted_img.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;

        converted_img.download(gray_img);

        ProcessContour(gray_img, out_img, iparam);
    }
    else{
        GetImageBySavedInfo(img, out_img, iparam);
    }
#else
    GetImageBySavedInfo(img, out_img, iparam);
#endif
}

// Jang 20220330
std::vector<std::vector<cv::Point>> ImageProcessing::GetCurrentContourVector(){
    return this->current_contours_;
}


cv::Mat ImageProcessing::SelectImage(const int img_num){
    selected_img = img_num;
    if(selected_img == 0) return orig_img;
    else return processed_img;
}

cv::Mat ImageProcessing::UpdateImage(const int blur, const int thres){
    if(orig_img.empty()) return orig_img;

    cv::Mat converted_img, final_img;
    cvtColor(orig_img, converted_img, cv::COLOR_BGR2GRAY);


    cv::GaussianBlur(converted_img, converted_img, cv::Size(blur * 2 + 1, blur * 2 + 1), blur);
    cv::threshold(converted_img, converted_img, 0, 255, cv::THRESH_OTSU);

    cv::Canny(converted_img, converted_img, thres, thres);

    final_img = cv::Mat::zeros(converted_img.size(), CV_8UC1);
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(converted_img, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    int contour_thickness = final_img.cols / 400;
    if( contour_thickness < 1) contour_thickness = 1;
    // Draw all contours
    cv::drawContours(final_img, contours, -1, (255, 255, 255), contour_thickness, cv::LINE_8);
    processed_img = final_img.clone();

    // Save
    current_iparam.blur_size = blur;
    current_iparam.blur_amp = blur;
    current_iparam.canny_th1 = thres;
    current_iparam.canny_th2 = thres;
    current_iparam.threshold_type = cv::THRESH_OTSU;


//    lw << "Updated Params: (Blur: " << blur << ", Thres: " << thres << lw.endl;
//    lw << "Image size: " << processed_img.size() << lw.endl;
    if(selected_img == 0) return orig_img;
    else return processed_img;
}

// Jang 20220630
void ImageProcessing::AdjustParametersForDownsampling(ImageParams input_param, ImageParams& output_param, int downsample_val) {
    if(downsample_val > 0){
//                float reduce_val = 1.0 - (0.1 * (float)i);
//                float reduce_blur_val = 1.0 - (0.2 * (float)i);
        float reduce_scale = (float)input_param.reduce_param_scale;
        float reduce_val = 1.0 - (0.02 * reduce_scale * (float)downsample_val);
        float reduce_blur_val = 1.0 - (0.04 * reduce_scale * (float)downsample_val);

        output_param.canny_th1 = (input_param.canny_th1 * reduce_val > 0) ? input_param.canny_th1 * reduce_val : 1;
        output_param.canny_th2 = (input_param.canny_th2 * reduce_val > 0) ? input_param.canny_th2 * reduce_val : 1;
        output_param.blur_size = (input_param.blur_size * reduce_blur_val > 0) ? input_param.blur_size * reduce_blur_val : 0;
    }
}
