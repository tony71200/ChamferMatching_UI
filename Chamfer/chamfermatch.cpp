#include "chamfermatch.h"


void CudaChamfer::SetLogPath(std::string dir_path){
    if(dir_path.size() > 0){
        std::string::iterator str_itr = dir_path.end();
        if(*(str_itr - 1) == '/'){
            dir_path.erase(dir_path.end() - 1);
        }else{

        }
    }else{
        dir_path = "./Log";
        lw.SetDirPath(dir_path);
        lw.SetLogName("MATCH");
        lw.SetShowDate(true);
        lw << "[WARNING] Input path is invalid. Please check it. "
              << "Code file \"" << __FILE__ << "\", line " << __LINE__ << ", in " << __func__ << lw.endl;
        return;
    }

    lw.SetDirPath(dir_path);
    lw.SetLogName("MATCH");
    lw.SetShowDate(true);
}


void CudaChamfer::InitPaths(std::string data_path, std::string log_path, std::string params_path){
    if(data_path.size() > 0){
        std::string::iterator str_itr = data_path.end();
        if(*(str_itr - 1) == '/'){

        }else{
            data_path = data_path + "/";
        }
    }else{
        this->data_path = "./data/";
        SetLogPath("./Log");
        this->params_path_ = "./data/parameters/";
        lw << "[WARNING] Input path is invalid. Please check it. "
              << "Code file \"" << __FILE__ << "\", line " << __LINE__ << ", in " << __func__ << lw.endl;
        return;
    }

    this->data_path = data_path;
    ///home/contrel/AlignmentSys/Data/0002+fine/methods/rst/match_0/log
    /// SetLogPath(data_path + "log");
    SetLogPath(log_path);

    // Add parameters path
    this->params_path_ = params_path;
}

std::string CudaChamfer::GetDirPath(){
    return this->data_path;
}

// Create "matching_parameters.ini" in parameters folder. Jimmy. 20220113.
bool CudaChamfer::CreateDefaultParamsIniFile(const std::string& path){
    /*
     * Goal:    Create a "matching_parameters.ini" if this file doesn't exist.
     *          And set the default value of parameters.
     * Input:   where "matching_parameters.ini" path.
     * Output:  Boolean: true for successfully create, false for failed.
     */

    if(!fs::is_directory(path) && fs::exists(path)){
        fs::remove(path);
    }

    fio::FileIO inim(path, fio::FileioFormat::INI);
    if(!inim.isOK()){
        std::cerr << "File:system_setting.txt isn't opened." << std::endl;
        lw << "[ERROR] File fail to create. "
           << "Code file \"" << __FILE__ << " " << ", in " << __func__ << lw.endl;
        return false;
    }

    try{
        inim.IniSetSection("ParameterTrainingRST");
        inim.IniInsert("down_sampling", default_params.down_sampling);
        inim.IniInsert("scale_interval", default_params._scaleInterval);
        inim.IniInsert("scale_min", default_params._minScale);
        inim.IniInsert("scale_max", default_params._maxScale);
        inim.IniInsert("angle_interval", default_params._angleInterval);
        inim.IniInsert("circle_num", default_params._circleNum);
        inim.IniSetSection("ParameterTestingRST");
        inim.IniInsert("cifi_threshold", default_params._cifiThreshold);
        inim.IniInsert("rafi_threshold", default_params._rafiThreshold);
        inim.IniInsert("tefi_threshold", default_params._tefiThreshold);
        inim.IniInsert("tefi_threshold_2", default_params._tefiThreshold2);
        inim.IniInsert("position_match_threshold", default_params._posMatchThreshold);
        inim.IniInsert("scale_match_threshold", default_params._scaleMatchThreshold);
        inim.IniSetSection("ParameterChamfer");
        inim.IniInsert("angle_min", default_params._angleMin);
        inim.IniInsert("angle_max", default_params._angleMax);
        inim.IniInsert("angle_coarse_interval", default_params._angleCoarseInterval);
        inim.IniInsert("angle_fine_interval", default_params._angleFineInterval);
        inim.IniInsert("skip_template_pixel", default_params.skip_pixel_);
        inim.IniInsert("chamfer_score_threshold", default_params.chamfer_score_threshold_);
        inim.IniInsert("chamfer_weight_discount", default_params.chamfer_weight_discount_);
        inim.IniSave();
        inim.close();
    }
    catch(std::exception e){
        lw << "[Warning] Error with create matching_parameters.ini file. "
                   << "Code file \"" << __FILE__  << ", in " << __func__ << lw.endl;
        lw << "With error: " << e.what() << lw.endl;
        return false;
    }
    return true;
}

bool CudaChamfer::CreateParamsIniFile(const std::string& path){
    /*
     * Goal:    Create a "matching_parameters.ini" if this file doesn't exist.
     *          And set the default value of parameters.
     * Input:   where "matching_parameters.ini" path.
     * Output:  Boolean: true for successfully create, false for failed.
     */
    // FileIO. 20220420. Jimmy. #fio22
    fio::FileIO inim(path, fio::FileioFormat::INI);
    if (!fio::exists(path)) {
        lw << "[ERROR] File fail to create. "
           << "Code file \"" << __FILE__ << " " << ", in " << __func__ << lw.endl;

        return false;
    }
    try {
        inim.IniSetSection("ParameterTrainingRST");
        inim.IniInsert("down_sampling", down_sampling);
        inim.IniInsert("scale_interval", _scaleInterval);
        inim.IniInsert("scale_min", _minScale);
        inim.IniInsert("scale_max", _maxScale);
        inim.IniInsert("angle_interval", _angleInterval);
        inim.IniInsert("circle_num", _circleNum);

        inim.IniSetSection("ParameterTestingRST");
        inim.IniInsert("cifi_threshold", _cifiThreshold);
        inim.IniInsert("rafi_threshold", _rafiThreshold);
        inim.IniInsert("tefi_threshold", _tefiThreshold);
        inim.IniInsert("tefi_threshold_2", _tefiThreshold2);
        inim.IniInsert("position_match_threshold", _posMatchThreshold);
        inim.IniInsert("scale_match_threshold", _scaleMatchThreshold);

        inim.IniSetSection("ParameterChamfer");
        inim.IniInsert("angle_min", angle_min_);
        inim.IniInsert("angle_max", angle_max_);
        inim.IniInsert("angle_coarse_interval",angle_interval_coarse_align_);
        inim.IniInsert("angle_fine_interval", angle_interval_fine_align_);
        inim.IniInsert("skip_template_pixel", skip_pixel_);
        inim.IniInsert("chamfer_score_threshold", chamfer_score_threshold_);
        inim.IniInsert("chamfer_weight_discount", chamfer_weight_discount_);

        inim.IniSave();
        inim.close();
    } catch(std::exception e) {
        lw << "[Warning] Error with create matching_parameters.ini file. "
                   << "Code file \"" << __FILE__  << ", in " << __func__ << lw.endl;
        lw << "With error: " << e.what() << lw.endl;
        return false;
    }
    return true;
}

void CudaChamfer::LoadParamsIniFile(const std::string& params_path){
//        std::string params_path = this->params_path_ + parameter_matching_file;
    if (!fs::exists(params_path)) {
        lw << "[ERROR] File access failed. "
           << "Code file \"" << __FILE__ << "\", line " << __LINE__ << ", in " << __func__ << lw.endl;

        CreateDefaultParamsIniFile(params_path);
    }
    fio::FileIO inim(params_path, fio::FileioFormat::INI);

    inim.IniSetSection("ParameterTrainingRST");
    down_sampling       = inim.IniReadtoInt("down_sampling",    down_sampling );
    _scaleInterval      = inim.IniReadtoFloat("scale_interval", _scaleInterval);
    _minScale           = inim.IniReadtoFloat("scale_min",      _minScale     );
    _maxScale           = inim.IniReadtoFloat("scale_max",      _maxScale     );
    _angleInterval      = inim.IniReadtoFloat("angle_interval", _angleInterval);
    _circleNum          = inim.IniReadtoFloat("circle_num",     _circleNum    );

    inim.IniSetSection("ParameterTestingRST");
    _cifiThreshold          = inim.IniReadtoFloat("cifi_threshold",             _cifiThreshold      );
    _rafiThreshold          = inim.IniReadtoFloat("rafi_threshold",             _rafiThreshold      );
    _tefiThreshold          = inim.IniReadtoFloat("tefi_threshold",             _tefiThreshold      );
    _tefiThreshold2         = inim.IniReadtoFloat("tefi_threshold_2",           _tefiThreshold2     );
    _posMatchThreshold      = inim.IniReadtoFloat("position_match_threshold",   _posMatchThreshold  );
    _scaleMatchThreshold    = inim.IniReadtoFloat("scale_match_threshold",      _scaleMatchThreshold);

    inim.IniSetSection("ParameterChamfer");
    angle_min_                          = inim.IniReadtoFloat("angle_min",                  angle_min_               );
    angle_max_                          = inim.IniReadtoFloat("angle_max",                  angle_max_               );
    angle_interval_coarse_align_        = inim.IniReadtoFloat("angle_coarse_interval",      angle_interval_coarse_align_    );
    angle_interval_fine_align_          = inim.IniReadtoFloat("angle_fine_interval",        angle_interval_fine_align_      );
    skip_pixel_                         = inim.IniReadtoInt("skip_template_pixel",          skip_pixel_             );
    chamfer_score_threshold_            = inim.IniReadtoFloat("chamfer_score_threshold",    chamfer_score_threshold_);
    chamfer_weight_discount_            = inim.IniReadtoFloat("chamfer_weight_discount",    chamfer_weight_discount_);

    bool is_value_ok = inim.isAllOK();
    inim.close();

    // RST parameters

    if(down_sampling < 1 || down_sampling > 10){
        down_sampling =  default_params.down_sampling;
        is_value_ok = false;
    }
    if(_scaleInterval < 0.01 || _scaleInterval > 1){
        _scaleInterval =  default_params._scaleInterval;
        is_value_ok = false;
    }
    if(_minScale < 0.1 || _minScale > 2){
        _minScale =  default_params._minScale;
        is_value_ok = false;
    }
    if(_maxScale < 0.5 || _maxScale > 5){
        _maxScale =  default_params._maxScale;
        is_value_ok = false;
    }
    if(_maxScale < _minScale){
        _maxScale =  default_params._maxScale;
        _minScale =  default_params._minScale;
        is_value_ok = false;
    }
    if(_angleInterval < 0.5 || _angleInterval > 45){
        _angleInterval =  default_params._angleInterval;
        is_value_ok = false;
    }
    if(_circleNum < 4 || _circleNum > 30){
        _circleNum =  default_params._circleNum;
        is_value_ok = false;
    }
    if(_cifiThreshold < 0.1 || _cifiThreshold > 0.99){
        _cifiThreshold =  default_params._cifiThreshold;
        is_value_ok = false;
    }
    if(_rafiThreshold < 0.1 || _rafiThreshold > 0.99){
        _rafiThreshold =  default_params._rafiThreshold;
        is_value_ok = false;
    }
    if(_tefiThreshold < 0.1 || _tefiThreshold > 0.99){
        _tefiThreshold =  default_params._tefiThreshold;
        is_value_ok = false;
    }
    if(_tefiThreshold2 < 0.1 || _tefiThreshold2 > 0.99){
        _tefiThreshold2 =  default_params._tefiThreshold2;
        is_value_ok = false;
    }

    // Chamfer parameters
    if(angle_min_ >= angle_max_){
        angle_min_ = default_params._angleMin;
        angle_max_ = default_params._angleMax;
        is_value_ok = false;
    }
    else if(angle_min_ < -180 || angle_max_ > 180){
        angle_min_ = default_params._angleMin;
        angle_max_ = default_params._angleMax;
        is_value_ok = false;
    }
    if(angle_interval_fine_align_ >= angle_interval_coarse_align_){
        angle_interval_fine_align_ = default_params._angleFineInterval;
        angle_interval_coarse_align_ = default_params._angleCoarseInterval;
        is_value_ok = false;
    }
    if(angle_interval_coarse_align_ <= 0 || angle_interval_coarse_align_ > 25){
        angle_interval_coarse_align_ = default_params._angleCoarseInterval;
        is_value_ok = false;
    }
    if(angle_interval_fine_align_ <= 0 || angle_interval_fine_align_ > 10){
        angle_interval_fine_align_ = default_params._angleFineInterval;
        is_value_ok = false;
    }
    if(skip_pixel_ < 0 || skip_pixel_ > 10){
        skip_pixel_ = default_params.skip_pixel_;
        is_value_ok = false;
    }
    if(chamfer_score_threshold_ < 0.5 || chamfer_score_threshold_ > 10){
        chamfer_score_threshold_ = default_params.chamfer_score_threshold_;
        is_value_ok = false;
    }
    if(chamfer_weight_discount_ < 0.001|| chamfer_weight_discount_ > 1){
        chamfer_weight_discount_ = default_params.chamfer_weight_discount_;
        is_value_ok = false;
    }

    if(!is_value_ok){
        CreateParamsIniFile(params_path);
    }
}

////////////////////////////////////////////////////////////////////
/// For Chamfer Matching Image Processing
void CudaChamfer::checkCUDAandSysInfo()
{
    // Check the CUDA specification
    cudaError_t cuda_error;
    size_t free_cuda_bytes;
    size_t total_cuda_bytes;

    cuda_error = cudaMemGetInfo(&free_cuda_bytes, &total_cuda_bytes);
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    lw << "--- CUDA ---" << lw.endl;
    lw << "CUDA specification in the current device: " << lw.endl;
    lw << "MaxThreadsPerBlock: " << (int)prop.maxThreadsPerBlock << lw.endl;
    lw << "MaxThreadsDim: " << (int)prop.maxThreadsDim[0]  << ", "<< (int)prop.maxThreadsDim[1] << ", "<< (int)prop.maxThreadsDim[2] << lw.endl;
    lw << "MaxGridSize: " << (int)prop.maxGridSize[0] << ", "<< (int)prop.maxGridSize[1] << ", "<< (int)prop.maxGridSize[2] << lw.endl;
    lw << "TotalGlobalMem: " << (int)prop.totalGlobalMem << lw.endl;
    lw << "SharedMemPerBlock: " << (int)prop.sharedMemPerBlock << lw.endl;
    lw << "Free memory(MB): " << (float)free_cuda_bytes / 1000000 << lw.endl;
    lw << "Total memory(MB): " << (float)total_cuda_bytes / 1000000 << lw.endl;
    /// JANG
    /// Error handling
    cuda_error = cudaGetLastError();
    if(cuda_error != cudaError::cudaSuccess){
        lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
    }
}

void CudaChamfer::createTempNonZeroMat(cv::Mat orig_temp, TrainTemplate& trained_template,
                                float angle_min, float angle_max, float angle_interval,
                                int skip, std::string path)
{
    /**
      * Goal:   Do Chamfer Mathcing Training - prepare the non-zero mat
      * Input:  src: original template; dst: trained_template (NULL);
      *         min angle; max angle; angle interval
      *         skip pixels (how many result pixels we skip)
      *         path (save result images)
      * Output: dst: trained_template (TrainTemplate* type)
      */
    // Get contour
    cv::Mat temp_gray;
    cv::Mat temp_contours;

    //Obtain gray image
    if(orig_temp.channels() > 2)
        cv::cvtColor(orig_temp, temp_gray, cv::COLOR_RGB2GRAY);
    else
        temp_gray = orig_temp.clone();

    float rot_angle = angle_interval;
    int num_rot = (int)((angle_max - angle_min) / angle_interval);

    // Clean and init.
    // Tối ưu: Dùng clear() không cần kiểm tra size > 0
    trained_template.templates_gray.clear();
    trained_template.templates.clear();
    trained_template.template_angle.clear();
    trained_template.template_scale.clear();
    trained_template.non_zero_area_templates.clear();
    trained_template.template_weights.clear();

    // Jang 20220503
    if(fs::exists(path)){
       fs::remove_all(path);
    }
    fs::create_directories(path);

    // Optimize: Only write files when debugging
    if(trained_template.num_non_zeros != nullptr){
        delete []trained_template.num_non_zeros;
        trained_template.num_non_zeros = nullptr;
    }
    trained_template.num_non_zeros = new int[num_rot];
    trained_template.is_template_ok = false;

    int tot_num_templates = 0;
    int tot_mem_space = 0;
    cv::Mat non_zeros_mat;
    if(dm::log) {
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
    }
    // Image Processing:
    // gray -> blur -> Affine -> binary -> morphologyEx -> Canny -> Skip Pixels
    for(int i = 0; i < num_rot; i++){
        cv::Mat temporary_img = temp_gray.clone();
        cv::Mat boundary_mask = cv::Mat::ones(temp_gray.size(), CV_8U) * 255;
        cv::Point2f center_pt(temporary_img.cols/2, temporary_img.rows/2);
        cv::Mat rotation_mat = cv::getRotationMatrix2D(center_pt, angle_min + rot_angle * i, 1.0);
        cv::warpAffine(temporary_img, temporary_img, rotation_mat, temporary_img.size());
        trained_template.templates_gray.push_back(temporary_img.clone());
        // Optimize: Only write files when debugging
        if(dm::match_img) {
            cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_gray.png", temporary_img );
        }
        std::vector<std::vector<cv::Point>> sorted_contours;
        if(!use_image_proc){
            cv::blur(temporary_img, temporary_img, cv::Size(3, 3));
            cv::Canny(temporary_img, temp_contours, 100, 100, 3);
        }
        else{
            // Jang 20220713
            ImageParams iparam;
            image_proc->LoadParams(iparam);
            image_proc->SkipBlurContour(false, false);
            if(orig_template_img_mat_.rows > temporary_img.rows && orig_template_img_mat_.cols > temporary_img.cols) {
                image_proc->AdjustParametersForDownsampling(iparam, iparam, iparam.downsample_step);
            }
            image_proc->GetImageBySavedInfo(temporary_img, temp_contours, iparam, use_cuda_for_improc);

            sorted_contours = this->image_proc->GetCurrentContourVector();
        }
        float weight_threshold = 0.5 / 4;
    // Jang 20220408
        if(0 < chamfer_weight_discount_ && chamfer_weight_discount_ <= 1){
            weight_threshold = chamfer_weight_discount_;
        }
        else{
            weight_threshold = 0.5 / 4;
        }

        cv::Mat contour_weight = cv::Mat::zeros(temp_gray.size(), CV_32F);
        if(sorted_contours.size() > 0){
            float weight = 1.0;
            for(int c = 0; c < sorted_contours.size(); c ++ ){
                int contour_size = sorted_contours.at(c).size();

                for(int p = 0; p < contour_size; p ++ ){
                    contour_weight.at<float>(sorted_contours.at(c).at(p)) = weight;
                }
                weight = weight_threshold * (1.0 / (float)(c + 1));
            }
        }
        // Tối ưu: Chỉ ghi file khi debug
        if(dm::match_img){
            cv::Mat contour_weight_copy_color = cv::Mat::zeros(temp_gray.size(), CV_8UC3);
            for(int c = 0; c < sorted_contours.size(); c ++ ){
                std::vector<std::vector<cv::Point>> one_contour;
                one_contour.push_back(sorted_contours.at(c));
                short blue = (c % 2) == 0 ? 255 : 0;
                short green = (((c + 2) / 2) % 2) == 0 ? 255 : 0;
                short red = (((c + 4) / 4) % 2) == 0 ? 0 : 255;
                if(blue == 0 && green == 0 && red == 0){
                    blue = 125;
                    green = 125;
                    red = 125;
                }
                cv::drawContours(contour_weight_copy_color, one_contour, -1, cv::Scalar(blue, green, red), 1, cv::LINE_8);
            }
            cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_weight_draw.png", contour_weight_copy_color );
            cv::Mat contour_weight_copy;
            cv::threshold(contour_weight, contour_weight_copy, 0, 255, cv::THRESH_BINARY);
            cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_weight.png", contour_weight_copy );
        }

        if(temp_contours.empty()){
            trained_template.num_templates = 0;
            if(trained_template.num_non_zeros != nullptr){
                delete []trained_template.num_non_zeros;
                trained_template.num_non_zeros = nullptr;
            }
            // Note: No contour, return immediately
            return;
        }

        // Jang 20220330
        // Optimize: check for valid mask, only process if mask exists
        cv::Mat dilated_boundary_mask;
        if(template_mask_exists_ && !template_mask_.empty()){
            if(boundary_mask.rows < template_mask_.rows && boundary_mask.cols < template_mask_.cols){
                cv::pyrDown(template_mask_, pyr_template_mask_, boundary_mask.size());    // become 1/2 size
                boundary_mask = pyr_template_mask_;
            }else{
                boundary_mask = template_mask_.clone();
            }
            if(dm::match_img){
                cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_selected_mask.png", boundary_mask );
            }
            cv::warpAffine(boundary_mask, boundary_mask, rotation_mat, temporary_img.size());
            cv::threshold(boundary_mask, boundary_mask, 254, 255, cv::THRESH_BINARY_INV);
            //cv::Mat dilated_boundary_mask;
            cv::morphologyEx(boundary_mask, dilated_boundary_mask, cv::MORPH_DILATE, cv::Mat::ones(cv::Size(3,3), CV_8UC1));
            temp_contours = temp_contours - dilated_boundary_mask;
        }

        // # Speedup: skip pixels on template
        // Optimize: skip pixels more efficiently, avoid repeating math
        if(skip > 0){
            int skip_count = 0;
            for(int n = 0; n < temp_contours.rows; n ++){
                uchar* row_ptr = temp_contours.ptr<uchar>(n);
                for(int m = 0; m < temp_contours.cols; m++){
                    if(row_ptr[m] > 0){
                        if(skip_count % skip != 0){
                            row_ptr[m] = 0;
                        }
                        skip_count++;
                    }
                }
            }
        }


        // Save the templates, and generate the non-zero mat (Chamfer images)
        // Tối ưu: chỉ ghi file khi debug
        if(dm::match_img){
            cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + ".png", temp_contours );
            if(!dilated_boundary_mask.empty())
                cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_dila_mask.png", dilated_boundary_mask );
            if(!boundary_mask.empty())
                cv::imwrite(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_thres_mask.png", boundary_mask );
        }

        cv::findNonZero(temp_contours, non_zeros_mat);
        int len_non_zeros_mat = non_zeros_mat.total(); // Optimize: use total() instead of multiplying size
        trained_template.num_non_zeros[i] = len_non_zeros_mat;
        tot_mem_space += len_non_zeros_mat;

        // Tối ưu: chỉ ghi file khi debug
        if(dm::match_img){
            cv::FileStorage fs;
            try {
                if (fs.open(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + ".data", cv::FileStorage::WRITE)) {
                    fs << "non_zeros_mat" << non_zeros_mat;
                }
                else {
                    lw << "The file couldn't be opened: " << path << ", line " << __LINE__ << lw.endl;
                }
            }
            catch (cv::Exception e) {
                lw << "FileStorage error: " << e.what() << lw.endl;
            }
            fs.release();
        }

        trained_template.templates.push_back(temp_contours.clone());
        trained_template.template_angle.push_back(angle_min + rot_angle * i);
        trained_template.non_zero_area_templates.push_back(non_zeros_mat.clone());
        // Jang 20220330
        if(!contour_weight.empty())
            trained_template.template_weights.push_back(contour_weight.clone());
        tot_num_templates++;
    }

    if(dm::log) {
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
    }
    // Check the memory and container
    trained_template.num_templates = tot_num_templates;
    trained_template.total_memory_size = tot_mem_space;

    // Put the non zero coordinate data into the flatten pointers
    // Why just single pointer? Double pointer is difficult to be allocated in CUDA.
    if(trained_template.flat_template_non_zero_row_ != nullptr){
        delete[] trained_template.flat_template_non_zero_row_;
        trained_template.flat_template_non_zero_row_ = nullptr;
    }
    if(trained_template.flat_template_non_zero_col_ != nullptr){
        delete[] trained_template.flat_template_non_zero_col_;
        trained_template.flat_template_non_zero_col_ = nullptr;
    }
    trained_template.flat_template_non_zero_col_ = new short[tot_mem_space];
    trained_template.flat_template_non_zero_row_ = new short[tot_mem_space];

    // Jang 20220330
    if(trained_template.flat_template_weight_ != nullptr){
        delete[] trained_template.flat_template_weight_;
        trained_template.flat_template_weight_ = nullptr;
    }
    trained_template.flat_template_weight_ = new float[tot_mem_space];

    int memory_index = 0;

    // Allocate the template values
    for(unsigned int i = 0; i < trained_template.non_zero_area_templates.size(); i ++){  // Total template num (diff. angle)

        cv::Mat non_zero_mat_tmp = trained_template.non_zero_area_templates.at(i);
        int len_non_zeros_mat = non_zero_mat_tmp.size().height * non_zero_mat_tmp.size().width;
        for(int j = 0; j < len_non_zeros_mat; j++){                             // non zeros (location)
            trained_template.flat_template_non_zero_col_[memory_index] = non_zero_mat_tmp.at<cv::Point>(j).x;
            trained_template.flat_template_non_zero_row_[memory_index] = non_zero_mat_tmp.at<cv::Point>(j).y;
            // Jang 20220330
            if(trained_template.template_weights.size() > i){
                trained_template.flat_template_weight_[memory_index] = trained_template.template_weights.at(i).at<float>(non_zero_mat_tmp.at<cv::Point>(j));
            }else{
                trained_template.flat_template_weight_[memory_index] = 1;
            }
            memory_index++;
        }
    }
    if(dm::log) {
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        lw <<"Num templates: " << trained_template.num_templates << lw.endl;
    }

    if(trained_template.num_templates == 0){
        trained_template.is_template_ok = false;
    }
    else{
        float trained_template_score = (float)trained_template.total_memory_size / (float)trained_template.num_templates;
        trained_template_score = trained_template_score * (skip + 1) -  ((trained_template.templates.at(0).cols + trained_template.templates.at(0).rows) / 2); // if chamfer_template_score is lower than 0, template is bad.
        if(trained_template_score > 0){
            trained_template.is_template_ok = true;
        }
        else{
            trained_template.is_template_ok = false;
        }

        if(dm::log) {
            lw << "File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            lw <<"trained_template_score: " << trained_template_score << lw.endl;
        }
    }

}

// Jang 20220627
// Rotate contour image, not rotating original image and finding contours.
void CudaChamfer::CreateTempNonZeroMat2(cv::Mat orig_temp, std::vector<TrainTemplatePtr> &trained_template,
                          float angle_min, float angle_max,
                          float fine_angle_interval, float coarse_angle_interval, int skip, std::string path){
    assert(trained_template.size() > 0);
    // Get contour
    cv::Mat temp_gray;
    cv::Mat temp_contours;
    //Obtain gray image
    if(orig_temp.channels() > 2)
        cv::cvtColor(orig_temp, temp_gray, cv::COLOR_RGB2GRAY);
    else
        temp_gray = orig_temp.clone();

    float rot_angle = fine_angle_interval;
    int fine_rot_num = (int)((angle_max - angle_min) / fine_angle_interval);
    int coarse_rot_num = (int)((angle_max - angle_min) / coarse_angle_interval);

    if(dm::log) {
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
    }
    for (int i = 0; i < trained_template.size(); ++i) {
        // Clean and init.
        trained_template.at(i).templates_gray.clear();
        trained_template.at(i).templates.clear();
        trained_template.at(i).template_angle.clear();
        trained_template.at(i).template_scale.clear();
        trained_template.at(i).non_zero_area_templates.clear();
        trained_template.at(i).template_weights.clear();

        trained_template.at(i).is_template_ok = false;

        trained_template.at(i).num_non_zeros.reset();

        if(use_only_start_and_end_){
            if(i != 0 && i != (trained_template.size() - 1)){
                continue;
            }
        }
        if( i == 0 ){
            MakeSharedPtr(trained_template.at(i).num_non_zeros, fine_rot_num);
        }
        else{
            MakeSharedPtr(trained_template.at(i).num_non_zeros, coarse_rot_num);
        }

    }
    if(dm::log) {
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
    }


    // Create directories
    if(fs::exists(path)){
       fs::remove_all(path);
    }
    fs::create_directories(path);
    std::string path_pyrmd;


    int tot_num_templates = 0;
    int tot_mem_space = 0;
    cv::Mat non_zeros_mat;

    // Image Processing:
    // gray -> blur -> Affine -> binary -> morphologyEx -> Canny -> Skip Pixels
    TrainTemplatePtr& fine_template = trained_template.at(0);
    for (int i = 0; i < trained_template.size(); ++i) {
        if(use_only_start_and_end_){
            if(i != 0 && i != (trained_template.size() - 1)){
                continue;
            }
        }
        path_pyrmd = path + "/scale_" + std::to_string(i) + "/";
//        std::cout << "path_pyrmd: " << path_pyrmd << std::endl;
        fs::create_directories(path_pyrmd);
        TrainTemplatePtr& template_ptr = trained_template.at(i);
        tot_num_templates = 0;
        tot_mem_space = 0;
        float coarse_index_step = coarse_angle_interval / fine_angle_interval;  // 1 / 0.1 = 10
        short resize_scale = std::pow(2, i); // 1, 2, 4, 8, ...


        int rot = 0;
        float template_angle = 0;

        cv::Size template_size(temp_gray.cols / resize_scale, temp_gray.rows / resize_scale);
        cv::Mat temporary_img ;
        if(i > 0) cv::resize(temp_gray, temporary_img, template_size);
        else temporary_img = temp_gray.clone();

        cv::imwrite(path_pyrmd + "/rot_" + std::to_string(template_angle) + "_template.png", temporary_img );

        ///// Contours /////
        std::vector<std::vector<cv::Point>> sorted_contours;
        if(!use_image_proc){
            cv::blur(temporary_img, temporary_img, cv::Size(3, 3));
            cv::Canny(temporary_img, temp_contours, 100, 100, 3);
        }
        else{
            ImageParams iparam;
            this->image_proc->LoadParams(iparam);
            this->image_proc->SkipBlurContour(false, false);
            if(i > 0){
                image_proc->AdjustParametersForDownsampling(iparam, iparam, i);
            }
            this->image_proc->GetImageBySavedInfo(temporary_img, temp_contours, iparam, use_cuda_for_improc);
            sorted_contours = this->image_proc->GetCurrentContourVector();
        }
        float weight_threshold = 0.5 / 4;

        if(0 < chamfer_weight_discount_ && chamfer_weight_discount_ <= 1){
            weight_threshold = chamfer_weight_discount_;
        }

        cv::Mat contour_weight = cv::Mat::zeros(temporary_img.size(), CV_32F);
        if(sorted_contours.size() > 0){
            float weight = 1.0;
            for(int c = 0; c < sorted_contours.size(); c ++ ){
                int contour_size = sorted_contours.at(c).size();

                for(int p = 0; p < contour_size; p ++ ){
                    contour_weight.at<float>(sorted_contours.at(c).at(p)) = weight;
                }
                weight = weight_threshold * (1.0 / (float)(c + 1));
            }
        }

        if(dm::match_img){
            cv::Mat contour_weight_copy_color = cv::Mat::zeros(temporary_img.size(), CV_8UC3);
            for(int c = 0; c < sorted_contours.size(); c ++ ){
                std::vector<std::vector<cv::Point>> one_contour;
                one_contour.push_back(sorted_contours.at(c));
                short blue = (c % 2) == 0 ? 255 : 0;
                short green = (((c + 2) / 2) % 2) == 0 ? 255 : 0;
                short red = (((c + 4) / 4) % 2) == 0 ? 0 : 255;
                if(blue == 0 && green == 0 && red == 0){
                    blue = 125;
                    green = 125;
                    red = 125;
                }
                cv::drawContours(contour_weight_copy_color, one_contour, -1, cv::Scalar(blue, green, red), 1, cv::LINE_8);
            }
            cv::imwrite(path_pyrmd + "/rot_" + std::to_string(template_angle) + "_weight_draw.png", contour_weight_copy_color );
            cv::Mat contour_weight_copy;
            cv::threshold(contour_weight, contour_weight_copy, 0, 255, cv::THRESH_BINARY);
            cv::imwrite(path_pyrmd + "/rot_" + std::to_string(template_angle) + "_weight.png", contour_weight_copy );
        }
        ///// END: Contours /////

        if(temp_contours.empty()){
            template_ptr.num_templates = 0;
            template_ptr.num_non_zeros.reset();
            return;
        }


        ///// Mask /////
        // It can be used with circle shape of templates
        if(template_mask_exists_){
            cv::Mat boundary_mask = cv::Mat::zeros(temporary_img.size(), CV_8U);
            cv::add(boundary_mask, 255, boundary_mask);
            if(boundary_mask.rows < template_mask_.rows && boundary_mask.cols < template_mask_.cols){
                cv::resize(template_mask_, pyr_template_mask_, template_size);
                boundary_mask = pyr_template_mask_;
            } else {
                boundary_mask = template_mask_.clone();
            }


            cv::threshold(boundary_mask, boundary_mask, 254, 255, cv::THRESH_BINARY_INV);
            cv::Mat dilated_boundary_mask;
            cv::morphologyEx(boundary_mask, dilated_boundary_mask, cv::MORPH_DILATE, cv::Mat::ones(cv::Size(5,5), CV_8UC1));
            if(dm::match_img){
            //        std::cout << "template_mask_ size: " << template_mask_.size() << std::endl;
            //        std::cout << "boundary_mask size: " << boundary_mask.size() << std::endl;
                cv::imwrite(path_pyrmd + "/rot_" + std::to_string(template_angle) + "_selected_mask.png", boundary_mask );
            }
            temp_contours = temp_contours - dilated_boundary_mask;
        }
        ///// END: Mask /////

        // # Speedup: skip pixels on template
        if(skip > 0){
            int skip_count = 0;
            int skip_distance = skip;
            for(int n = 0; n < temp_contours.rows; n ++){
                for(int m = 0; m < temp_contours.cols; m++){
                    if(temp_contours.at<uchar>(n, m) > 0){
                        if(skip_count % skip_distance != 0){    // skip the matching in some pixels
                            temp_contours.at<uchar>(n, m) = 0;
                        }
                        skip_count++;
                    }
                }
            }
        }

        /////   Rotation    /////
        cv::Point2f center_pt(temporary_img.cols/2, temporary_img.rows/2);
        int rot_num;
        if(i == 0) rot_num = fine_rot_num;
        else rot_num = coarse_rot_num;
        for(int rot = 0; rot < rot_num; rot++){
            short fine_index = (short)(rot * coarse_index_step);
            float template_angle;
            if(i == 0){
                template_angle = angle_min + rot_angle * rot;
            }
            else{
                template_angle = fine_template.template_angle.at(fine_index);
            }


            cv::Mat rotated_template;
            cv::Mat rotated_contour;
            cv::Mat rotated_weight;
            cv::Mat rotation_mat = cv::getRotationMatrix2D(center_pt, template_angle, 1.0);
            cv::warpAffine(temporary_img, rotated_template, rotation_mat, temporary_img.size());
            cv::warpAffine(temp_contours, rotated_contour, rotation_mat, temporary_img.size());
            cv::warpAffine(contour_weight, rotated_weight, rotation_mat, temporary_img.size());


            template_ptr.templates_gray.push_back(rotated_template.clone());


            // Save the templates, and generate the non-zero mat (Chamfer images)
            cv::imwrite(path_pyrmd + "/rot_" + std::to_string(template_angle) + ".png", rotated_contour );


            cv::findNonZero(rotated_contour, non_zeros_mat);
            int len_non_zeros_mat = non_zeros_mat.size().height * non_zeros_mat.size().width;   // the total pixels in non_zeros_mat

            template_ptr.num_non_zeros[rot] = len_non_zeros_mat;
            tot_mem_space += len_non_zeros_mat;

            cv::FileStorage fs;
            try {
                if (fs.open(path_pyrmd + "/rot_" + std::to_string(template_angle) + ".data", cv::FileStorage::WRITE)) {
                    fs << "non_zeros_mat" << non_zeros_mat;
                }
                else {
                    lw << "The file couldn't be opened: " << path_pyrmd << ", line " << __LINE__ << lw.endl;
                }

            }
            catch (cv::Exception e) {
                lw << "FileStorage error: " << e.what() << lw.endl;
            }
            fs.release();

            template_ptr.templates.push_back(rotated_contour.clone());
            template_ptr.template_angle.push_back(template_angle);
            template_ptr.non_zero_area_templates.push_back(non_zeros_mat.clone());
            if(!rotated_weight.empty())
                template_ptr.template_weights.push_back(rotated_weight.clone());
            tot_num_templates++;
        }
        // Check the memory and container
        template_ptr.num_templates = tot_num_templates;
        template_ptr.total_memory_size = tot_mem_space;
        template_ptr.width = orig_temp.cols / resize_scale;
        template_ptr.height = orig_temp.rows / resize_scale;


        if(dm::log) {
            lw << "File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            lw <<"Num templates: " << trained_template.at(0).num_templates << lw.endl;
            lw <<"total_memory_size: " << trained_template.at(0).total_memory_size << lw.endl;
        }

        // Put the non zero coordinate data into the flatten pointers
        // Why just single pointer? Double pointer is difficult to be allocated in CUDA.
        if(dm::log) {
            lw << "File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            lw <<"total_memory_size: " << trained_template.at(i).total_memory_size << lw.endl;
        }
        MakeSharedPtr(trained_template.at(i).flat_template_non_zero_row_, trained_template.at(i).total_memory_size);
        MakeSharedPtr(trained_template.at(i).flat_template_non_zero_col_, trained_template.at(i).total_memory_size);
        MakeSharedPtr(trained_template.at(i).flat_template_weight_, trained_template.at(i).total_memory_size);

        int memory_index = 0;

        // Allocate the template values
        for(int j = 0; j < trained_template.at(i).non_zero_area_templates.size(); j ++){  // Total template num (diff. angle)
            cv::Mat non_zero_mat_tmp = trained_template.at(i).non_zero_area_templates.at(j);
            int len_non_zeros_mat = non_zero_mat_tmp.size().height * non_zero_mat_tmp.size().width;
            for(int k = 0; k < len_non_zeros_mat; k++){                             // non zeros (location)
                trained_template.at(i).flat_template_non_zero_col_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).x;
                trained_template.at(i).flat_template_non_zero_row_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).y;
                if(trained_template.at(i).template_weights.size() > j){
                    trained_template.at(i).flat_template_weight_[memory_index]
                            = trained_template.at(i).template_weights.at(j).at<float>(non_zero_mat_tmp.at<cv::Point>(k));
                } else {
                    trained_template.at(i).flat_template_weight_[memory_index] = 1;
                }
                memory_index++;
            }
        }

        if(trained_template.at(i).num_templates == 0){
            trained_template.at(i).is_template_ok = false;
        }
        else{
            float trained_template_score = (float)trained_template.at(i).total_memory_size / (float)trained_template.at(i).num_templates;
            trained_template_score = trained_template_score * (skip + 1)
                    - ((trained_template.at(i).width + trained_template.at(i).height) / 2); // if chamfer_template_score is lower than 0, template is bad.
            if(trained_template_score > 0){
                trained_template.at(i).is_template_ok = true;
            }
            else{
                trained_template.at(i).is_template_ok = false;
            }

            if(dm::log) {
                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw <<"trained_template_score: " << trained_template_score << lw.endl;
            }
        }
    }
}

// Jang 20220608; Modified
bool CudaChamfer::LoadChamferTemplate(TrainTemplate& trained_template,
                                float angle_min, float angle_max, float angle_interval, std::string path)
{
    /**
      * Goal:   prepare the non-zero mat
      * Input:
      *         min angle; max angle; angle interval
      *         path (save result images)
      * Output: dst: trained_template (TrainTemplate* type)
      */

    cv::Mat temp_contours;
    cv::Mat temp_gray;
    bool load_result = false;

    float rot_angle = angle_interval;
    int num_rot = (int)((angle_max - angle_min) / angle_interval);

    // Clean and init.
    if(trained_template.templates_gray.size() > 0){
        trained_template.templates_gray.clear();
    }
    if(trained_template.templates.size() > 0){
        trained_template.templates.clear();
    }
    if(trained_template.template_angle.size() > 0){
        trained_template.template_angle.clear();
    }
    if(trained_template.template_scale.size() > 0){
        trained_template.template_scale.clear();
    }
    if(trained_template.non_zero_area_templates.size() > 0){
        trained_template.non_zero_area_templates.clear();
    }
    // Jang 20220331
    if(trained_template.template_weights.size() > 0){
        trained_template.template_weights.clear();
    }
    fs::create_directories(path);

    if(trained_template.num_non_zeros != nullptr){
        delete []trained_template.num_non_zeros;
    // Jang 20220411
        trained_template.num_non_zeros = nullptr;
    }
    trained_template.num_non_zeros = new int[num_rot];

    int tot_num_templates = 0;
    int tot_mem_space = 0;
    cv::Mat non_zeros_mat;
    if(dm::log) {
        lw << "num_rot: " << num_rot <<lw.endl;
    }
    for(int i = 0; i < num_rot; i++){
//        temp_contours = cv::imread(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + ".png", CV_8UC1 );
        temp_gray = cv::imread(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + "_gray.png", CV_8UC1 );

        // Jang 20220713
        std::vector<std::vector<cv::Point>> sorted_contours;
        if(!use_image_proc){
            cv::blur(temp_gray, temp_gray, cv::Size(3, 3));
            //Canny edge
            cv::Canny(temp_gray, temp_contours, 100, 100, 3);
        }
        else{
            ImageParams iparam;
            image_proc->LoadParams(iparam);
            image_proc->SkipBlurContour(false, false);
            if(orig_template_img_mat_.rows > temp_gray.rows && orig_template_img_mat_.cols > temp_gray.cols) {
                image_proc->AdjustParametersForDownsampling(iparam, iparam, iparam.downsample_step);
            }
            image_proc->GetImageBySavedInfo(temp_gray, temp_contours, iparam, use_cuda_for_improc);

            sorted_contours = this->image_proc->GetCurrentContourVector();
        }
        float weight_threshold = 0.5 / 4;
        if(0 < chamfer_weight_discount_ && chamfer_weight_discount_ <= 1){
            weight_threshold = chamfer_weight_discount_;
        }
        else{
            weight_threshold = 0.5 / 4;
        }

        cv::Mat contour_weight = cv::Mat::zeros(temp_gray.size(), CV_32F);
        if(sorted_contours.size() > 0){
            float weight = 1.0;
            for(int c = 0; c < sorted_contours.size(); c ++ ){
                int contour_size = sorted_contours.at(c).size();

                for(int p = 0; p < contour_size; p ++ ){
                    contour_weight.at<float>(sorted_contours.at(c).at(p)) = weight;
                }
                weight = weight_threshold * (1.0 / (float)(c + 1));
            }
        }

        cv::FileStorage fs;
        try {
            if (fs.open(path + "/rot_" + std::to_string(angle_min + rot_angle * i) + ".data", cv::FileStorage::READ)) {

                fs["non_zeros_mat"] >> non_zeros_mat;
                load_result = true;
            }
            else {
                lw << "The file couldn't be opened: " << path << ", line " << __LINE__ << lw.endl;
                load_result = false;
            }

        }
        catch (cv::Exception e) {
            lw << "FileStorage error: " << e.what() << lw.endl;
            load_result = false;
        }
        fs.release();
        if(!load_result) {
            if(trained_template.num_non_zeros != nullptr){
                delete []trained_template.num_non_zeros;
                trained_template.num_non_zeros = nullptr;
            }
            return false;
        }
        int len_non_zeros_mat = non_zeros_mat.size().height * non_zeros_mat.size().width;   // the total pixels in non_zeros_mat
        trained_template.num_non_zeros[i] = len_non_zeros_mat;
        tot_mem_space += len_non_zeros_mat;

        trained_template.templates.push_back(temp_contours.clone());
        trained_template.templates_gray.push_back(temp_gray.clone());
        trained_template.template_angle.push_back(angle_min + rot_angle * i);
        trained_template.non_zero_area_templates.push_back(non_zeros_mat.clone());
        // Jang 20220330
        if(!contour_weight.empty())
            trained_template.template_weights.push_back(contour_weight.clone());
        tot_num_templates++;
    }

    // Check the memory and container
    trained_template.num_templates = tot_num_templates;
    trained_template.total_memory_size = tot_mem_space;

    if(trained_template.flat_template_non_zero_row_ != nullptr){
        delete[] trained_template.flat_template_non_zero_row_;
        trained_template.flat_template_non_zero_row_ = nullptr;
    }
    if(trained_template.flat_template_non_zero_col_ != nullptr){
        delete[] trained_template.flat_template_non_zero_col_;
        trained_template.flat_template_non_zero_col_ = nullptr;
    }
    trained_template.flat_template_non_zero_col_ = new short[tot_mem_space];
    trained_template.flat_template_non_zero_row_ = new short[tot_mem_space];

    // Jang 20220330
    if(trained_template.flat_template_weight_ != nullptr){
        delete[] trained_template.flat_template_weight_;
        trained_template.flat_template_weight_ = nullptr;
    }
    trained_template.flat_template_weight_ = new float[tot_mem_space];



    int memory_index = 0;

    // Allocate the template values
    for(unsigned int i = 0; i < trained_template.non_zero_area_templates.size(); i ++){  // Total template num (diff. angle)

        cv::Mat non_zero_mat_tmp = trained_template.non_zero_area_templates.at(i);
        int len_non_zeros_mat = non_zero_mat_tmp.size().height * non_zero_mat_tmp.size().width;
        for(int j = 0; j < len_non_zeros_mat; j++){                             // non zeros (location)
            trained_template.flat_template_non_zero_col_[memory_index] = non_zero_mat_tmp.at<cv::Point>(j).x;
            trained_template.flat_template_non_zero_row_[memory_index] = non_zero_mat_tmp.at<cv::Point>(j).y;
            // Jang 20220330
            if(trained_template.template_weights.size() > i){
                trained_template.flat_template_weight_[memory_index] = trained_template.template_weights.at(i).at<float>(non_zero_mat_tmp.at<cv::Point>(j));
                if(trained_template.flat_template_weight_[memory_index] > 1){
                    lw << "[ERROR] flat_template_weight_: " << trained_template.flat_template_weight_[memory_index] << lw.endl;
                }
            }else{
                trained_template.flat_template_weight_[memory_index] = 1;
            }
            memory_index++;
        }
    }

    return true;
}

void CudaChamfer::LoadChamferTemplate(std::vector<TrainTemplatePtr> &trained_template,
                        float angle_min, float angle_max,
                         float fine_angle_interval, float coarse_angle_interval, std::string path)
{

    cv::Mat temp_contours;
    cv::Mat temp_gray;

    float rot_angle = fine_angle_interval;
    int fine_rot_num = (int)((angle_max - angle_min) / fine_angle_interval);
    int coarse_rot_num = (int)((angle_max - angle_min) / coarse_angle_interval);

    fs::create_directories(path);
    for (int i = 0; i < trained_template.size(); ++i) {
        // Clean and init.
        if(trained_template.at(i).templates_gray.size() > 0){
            trained_template.at(i).templates_gray.clear();
        }
        if(trained_template.at(i).templates.size() > 0){
            trained_template.at(i).templates.clear();
        }
        if(trained_template.at(i).template_angle.size() > 0){
            trained_template.at(i).template_angle.clear();
        }
        if(trained_template.at(i).template_scale.size() > 0){
            trained_template.at(i).template_scale.clear();
        }
        if(trained_template.at(i).non_zero_area_templates.size() > 0){
            trained_template.at(i).non_zero_area_templates.clear();
        }
        if(trained_template.at(i).template_weights.size() > 0){
            trained_template.at(i).template_weights.clear();
        }
        trained_template.at(i).num_non_zeros.reset();
        if( i == 0 ){
            MakeSharedPtr(trained_template.at(i).num_non_zeros, fine_rot_num);
        }
        else{
            MakeSharedPtr(trained_template.at(i).num_non_zeros, coarse_rot_num);
        }

        trained_template.at(i).is_template_ok = false;
    }

    int tot_num_templates = 0;
    int tot_mem_space = 0;
    cv::Mat non_zeros_mat;
    if(dm::log) {
        lw << "num_rot: " << fine_rot_num <<lw.endl;
    }

    TrainTemplatePtr& fine_template_ptr = trained_template.at(0);
    tot_num_templates = 0;
    tot_mem_space = 0;
    for(int j = 0; j < fine_rot_num; j++){
        temp_contours = cv::imread(path + "/rot_" + std::to_string(angle_min + rot_angle * j) + ".png", CV_8UC1 );
        temp_gray = cv::imread(path + "/rot_" + std::to_string(angle_min + rot_angle * j) + "_gray.png", CV_8UC1 );
//        cv::findNonZero(temp_contours, non_zeros_mat);
        cv::Mat contour_weight;
        cv::FileStorage fs;
        try {
            if (fs.open(path + "/rot_" + std::to_string(angle_min + rot_angle * j) + ".data", cv::FileStorage::READ)) {
                fs["non_zeros_mat"] >> non_zeros_mat;
                fs["contour_weight"] >> contour_weight;
            }
            else {
                lw << "The file couldn't be opened: " << path << ", line " << __LINE__ << lw.endl;
            }

        }
        catch (cv::Exception e) {
            lw << "FileStorage error: " << e.what() << lw.endl;
        }
        fs.release();

        int len_non_zeros_mat = non_zeros_mat.size().height * non_zeros_mat.size().width;   // the total pixels in non_zeros_mat
        fine_template_ptr.num_non_zeros[j] = len_non_zeros_mat;
        tot_mem_space += len_non_zeros_mat;

        fine_template_ptr.templates.push_back(temp_contours.clone());
        fine_template_ptr.templates_gray.push_back(temp_gray.clone());
        fine_template_ptr.template_angle.push_back(angle_min + rot_angle * j);
        fine_template_ptr.non_zero_area_templates.push_back(non_zeros_mat.clone());
        if(!contour_weight.empty())
            fine_template_ptr.template_weights.push_back(contour_weight.clone());
        tot_num_templates++;
    }
    fine_template_ptr.num_templates = tot_num_templates;
    fine_template_ptr.total_memory_size = tot_mem_space;
    fine_template_ptr.width = fine_template_ptr.templates.at(0).cols;
    fine_template_ptr.height = fine_template_ptr.templates.at(0).rows;

    // Load coarse templates
    float coarse_index_step = coarse_angle_interval / fine_angle_interval;  // 1 / 0.1 = 10
    for (int i = 1; i < trained_template.size(); ++i) {
        TrainTemplatePtr& template_ptr = trained_template.at(i);
        tot_num_templates = 0;
        tot_mem_space = 0;

        for(int rot = 0; rot < coarse_rot_num; rot++){
            short fine_index = (short)(rot * coarse_index_step);
            float template_angle = fine_template_ptr.template_angle.at(fine_index);
//            temp_contours = cv::imread(path + "/rot_" + std::to_string(template_angle) + "_" + std::to_string(i) + ".png", CV_8UC1 );
//            temp_gray = cv::imread(path + "/rot_" + std::to_string(template_angle) + "_gray_" + std::to_string(i) + ".png", CV_8UC1 );

    //        cv::findNonZero(temp_contours, non_zeros_mat);

            cv::Mat contour_weight;
            cv::FileStorage fs;
            try {
                if (fs.open(path + "/rot_" + std::to_string(template_angle) + "_" + std::to_string(i) + ".data", cv::FileStorage::READ)) {
                    fs["non_zeros_mat"] >> non_zeros_mat;
                    fs["contour_weight"] >> contour_weight;
                }
                else {
                    lw << "The file couldn't be opened: " << path << ", line " << __LINE__ << lw.endl;
                }

            }
            catch (cv::Exception e) {
                lw << "FileStorage error: " << e.what() << lw.endl;
            }
            fs.release();

            int len_non_zeros_mat = non_zeros_mat.size().height * non_zeros_mat.size().width;   // the total pixels in non_zeros_mat
            template_ptr.num_non_zeros[rot] = len_non_zeros_mat;
            template_ptr.template_angle.push_back(template_angle);
            template_ptr.non_zero_area_templates.push_back(non_zeros_mat.clone());
            if(!contour_weight.empty())
                template_ptr.template_weights.push_back(contour_weight.clone());

            tot_mem_space += len_non_zeros_mat;
            tot_num_templates++;
        }
        template_ptr.num_templates = tot_num_templates;
        template_ptr.total_memory_size = tot_mem_space;
        template_ptr.width = fine_template_ptr.width;
        template_ptr.height = fine_template_ptr.height;
    }

    for (int i = 0; i < trained_template.size(); ++i) {
        TrainTemplatePtr& template_ptr = trained_template.at(i);
        MakeSharedPtr(template_ptr.flat_template_non_zero_row_, template_ptr.total_memory_size);
        MakeSharedPtr(template_ptr.flat_template_non_zero_col_, template_ptr.total_memory_size);
        MakeSharedPtr(template_ptr.flat_template_weight_, template_ptr.total_memory_size);

        int memory_index = 0;

        for(unsigned int j = 0; j < template_ptr.non_zero_area_templates.size(); j ++){  // Total template num (diff. angle)

            cv::Mat non_zero_mat_tmp = template_ptr.non_zero_area_templates.at(j);
            int len_non_zeros_mat = non_zero_mat_tmp.size().height * non_zero_mat_tmp.size().width;
            for(int k = 0; k < len_non_zeros_mat; k++){                             // non zeros (location)
                template_ptr.flat_template_non_zero_col_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).x;
                template_ptr.flat_template_non_zero_row_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).y;
                if(template_ptr.template_weights.size() > j){
                    template_ptr.flat_template_weight_[memory_index]
                            = template_ptr.template_weights.at(j).at<float>(non_zero_mat_tmp.at<cv::Point>(k));
                    if(template_ptr.flat_template_weight_[memory_index] > 1){
                        lw << "[ERROR] flat_template_weight_: " << template_ptr.flat_template_weight_[memory_index] << lw.endl;
                    }
                }else{
                    template_ptr.flat_template_weight_[memory_index] = 1;
                }
                memory_index++;
            }
        }

        if(template_ptr.num_templates == 0){
            template_ptr.is_template_ok = false;
        }
        else{
            float trained_template_score = (float)template_ptr.total_memory_size / (float)template_ptr.num_templates;
            trained_template_score = trained_template_score
                    - ((template_ptr.width + template_ptr.height) / 2); // if chamfer_template_score is lower than 0, template is bad.
            if(trained_template_score > 0){
                template_ptr.is_template_ok = true;
            }
            else{
                template_ptr.is_template_ok = false;
            }

            if(dm::log) {
                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw <<"trained_template_score: " << trained_template_score << lw.endl;
            }
        }
    }

}

bool CudaChamfer::LoadChamferTemplate2(std::vector<TrainTemplatePtr> &trained_template,
                        float angle_min, float angle_max,
                         float fine_angle_interval, float coarse_angle_interval, std::string path)
{

    cv::Mat temp_contours;
    cv::Mat temp_gray;
    bool load_result = false;

    float rot_angle = fine_angle_interval;
    int fine_rot_num = (int)((angle_max - angle_min) / fine_angle_interval);
    int coarse_rot_num = (int)((angle_max - angle_min) / coarse_angle_interval);

    fs::create_directories(path);
    std::string path_pyrmd;

    for (int i = 0; i < trained_template.size(); ++i) {
        // Clean and init.
        if(trained_template.at(i).templates_gray.size() > 0){
            trained_template.at(i).templates_gray.clear();
        }
        if(trained_template.at(i).templates.size() > 0){
            trained_template.at(i).templates.clear();
        }
        if(trained_template.at(i).template_angle.size() > 0){
            trained_template.at(i).template_angle.clear();
        }
        if(trained_template.at(i).template_scale.size() > 0){
            trained_template.at(i).template_scale.clear();
        }
        if(trained_template.at(i).non_zero_area_templates.size() > 0){
            trained_template.at(i).non_zero_area_templates.clear();
        }
        if(trained_template.at(i).template_weights.size() > 0){
            trained_template.at(i).template_weights.clear();
        }
        trained_template.at(i).num_non_zeros.reset();
        if( i == 0 ){
            MakeSharedPtr(trained_template.at(i).num_non_zeros, fine_rot_num);
        }
        else{
            MakeSharedPtr(trained_template.at(i).num_non_zeros, coarse_rot_num);
        }

        trained_template.at(i).is_template_ok = false;
    }

    int tot_num_templates = 0;
    int tot_mem_space = 0;
    cv::Mat non_zeros_mat;
    if(dm::log) {
        lw << "num_rot: " << fine_rot_num <<lw.endl;
    }

    TrainTemplatePtr& fine_template_ptr = trained_template.at(0);
    tot_num_templates = 0;
    tot_mem_space = 0;
    for (int i = 0; i < trained_template.size(); ++i) {
        path_pyrmd = path + "/scale_" + std::to_string(i) + "/";
        TrainTemplatePtr& template_ptr = trained_template.at(i);
        float coarse_index_step = coarse_angle_interval / fine_angle_interval;
        int rot_num;
        if(i == 0) rot_num = fine_rot_num;
        else rot_num = coarse_rot_num;
        for(int rot = 0; rot < rot_num; rot++){
            short fine_index = (short)(rot * coarse_index_step);
            float template_angle;
            if(i == 0){
                template_angle = angle_min + rot_angle * rot;
            }
            else{
                template_angle = fine_template_ptr.template_angle.at(fine_index);
            }

            temp_contours = cv::imread(path_pyrmd + "/rot_" + std::to_string(template_angle) + ".png", CV_8UC1 );
            temp_gray = cv::imread(path_pyrmd + "/rot_" + std::to_string(template_angle) + "_gray.png", CV_8UC1 );
    //        cv::findNonZero(temp_contours, non_zeros_mat);
            cv::Mat contour_weight;
            cv::FileStorage fs;
            try {
                if (fs.open(path_pyrmd + "/rot_" + std::to_string(template_angle) + ".data", cv::FileStorage::READ)) {
                    fs["non_zeros_mat"] >> non_zeros_mat;
                    fs["contour_weight"] >> contour_weight;
                    load_result = true;
                }
                else {
                    lw << "The file couldn't be opened: " << path_pyrmd << ", line " << __LINE__ << lw.endl;
                    load_result = false;
                }

            }
            catch (cv::Exception e) {
                lw << "FileStorage error: " << e.what() << lw.endl;
                load_result = false;
            }
            fs.release();
            if(!load_result) {
                trained_template.clear();
                return false;
            }

            int len_non_zeros_mat = non_zeros_mat.size().height * non_zeros_mat.size().width;   // the total pixels in non_zeros_mat
            template_ptr.num_non_zeros[rot] = len_non_zeros_mat;
            tot_mem_space += len_non_zeros_mat;

            template_ptr.templates.push_back(temp_contours.clone());
            template_ptr.templates_gray.push_back(temp_gray.clone());
            template_ptr.template_angle.push_back(template_angle);
            template_ptr.non_zero_area_templates.push_back(non_zeros_mat.clone());
            if(!contour_weight.empty())
                template_ptr.template_weights.push_back(contour_weight.clone());
            tot_num_templates++;
        }
        template_ptr.num_templates = tot_num_templates;
        template_ptr.total_memory_size = tot_mem_space;
        template_ptr.width = template_ptr.templates.at(0).cols;
        template_ptr.height = template_ptr.templates.at(0).rows;

        MakeSharedPtr(template_ptr.flat_template_non_zero_row_, template_ptr.total_memory_size);
        MakeSharedPtr(template_ptr.flat_template_non_zero_col_, template_ptr.total_memory_size);
        MakeSharedPtr(template_ptr.flat_template_weight_, template_ptr.total_memory_size);

        int memory_index = 0;

        for(unsigned int j = 0; j < template_ptr.non_zero_area_templates.size(); j ++){  // Total template num (diff. angle)

            cv::Mat non_zero_mat_tmp = template_ptr.non_zero_area_templates.at(j);
            int len_non_zeros_mat = non_zero_mat_tmp.size().height * non_zero_mat_tmp.size().width;
            for(int k = 0; k < len_non_zeros_mat; k++){                             // non zeros (location)
                template_ptr.flat_template_non_zero_col_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).x;
                template_ptr.flat_template_non_zero_row_[memory_index] = non_zero_mat_tmp.at<cv::Point>(k).y;
                if(template_ptr.template_weights.size() > j){
                    template_ptr.flat_template_weight_[memory_index]
                            = template_ptr.template_weights.at(j).at<float>(non_zero_mat_tmp.at<cv::Point>(k));
                    if(template_ptr.flat_template_weight_[memory_index] > 1){
                        lw << "[ERROR] flat_template_weight_: " << template_ptr.flat_template_weight_[memory_index] << lw.endl;
                    }
                }else{
                    template_ptr.flat_template_weight_[memory_index] = 1;
                }
                memory_index++;
            }
        }

        if(template_ptr.num_templates == 0){
            template_ptr.is_template_ok = false;
        }
        else{
            float trained_template_score = (float)template_ptr.total_memory_size / (float)template_ptr.num_templates;
            trained_template_score = trained_template_score
                    - ((template_ptr.width + template_ptr.height) / 2); // if chamfer_template_score is lower than 0, template is bad.
            if(trained_template_score > 0){
                template_ptr.is_template_ok = true;
            }
            else{
                template_ptr.is_template_ok = false;
            }

            if(dm::log) {
                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw <<"trained_template_score: " << trained_template_score << lw.endl;
            }
        }
    }
    return true;
}

void CudaChamfer::createSrcNonZeroMat(cv::Mat orig_src,
                               PreprocessedSource &preprocessed_source,
                               int temp_w, int temp_h, int src_w, int src_h,
                               std::string path)
{
    cv::Mat src_img = orig_src;//cv::Mat src_img = orig_src.clone();
    cv::Mat src_contours;
    cv::Mat src_contours_inv;

    //Obtain gray image
    if(src_img.channels() > 1){
        cv::cvtColor(src_img, src_img, cv::COLOR_RGB2GRAY);
    }

    struct timespec tp_start, tp_end;
    double host_elapsed;
    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] createSrcNonZeroMat" << lw.endl;
    }
    if(!use_image_proc){
        cv::blur(src_img, src_img, cv::Size(3, 3));
        cv::Canny(src_img, src_contours, 100, 100, 3);
    }
    else{
        // Jang 20220713
        ImageParams iparam;
        image_proc->LoadParams(iparam);
        if(orig_source_img_mat_.rows > src_img.rows && orig_source_img_mat_.cols > src_img.cols) {
            image_proc->AdjustParametersForDownsampling(iparam, iparam, iparam.downsample_step);
        }
        image_proc->GetImageBySavedInfo(src_img, src_contours, iparam, use_cuda_for_improc);
    }

    if(orig_source_contours_mat_.empty()){
        orig_source_contours_mat_ = src_contours.clone();
    }

    // Mask
    cv::Mat src_mask = cv::Mat::zeros(src_img.size(), CV_8UC1);
    if(src_mask_exists_){
        if(orig_source_img_mat_.rows == source_img_mask_.rows && orig_source_img_mat_.cols == source_img_mask_.cols){
            if(orig_source_img_mat_.rows > src_img.rows && orig_source_img_mat_.cols > src_img.cols) {
                if(!source_img_mask_.empty()){
                    cv::pyrDown(source_img_mask_, pyr_source_img_mask_, src_img.size());    // become 1/2 size
                }
                src_mask = pyr_source_img_mask_;
            }else{
                src_mask = source_img_mask_;
            }
            src_contours = src_contours - src_mask;
        }
        else{
            src_mask_exists_ = false;
        }
    }

    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);

        lw << "[DEBUG] Time elapsed - blur and canny: " << host_elapsed << "s" << lw.endl;
        lw << "[DEBUG] temp_w: " << temp_w << ", temp_h: " << temp_h << lw.endl;
    }
    cv::Mat dilated_contours;
    // the size of dilate should be template/2 to reduce the candidates
    //cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(temp_w / 2, temp_h / 2));
    //cv::dilate(src_contours, dilated_contours, element);

    // Jang 20220330
//    cv::morphologyEx(src_contours, dilated_contours, cv::MORPH_DILATE,
//                     cv::Mat::ones(cv::Size(temp_w / 2, temp_w / 2) , CV_8UC1));
    cv::morphologyEx(src_contours, dilated_contours, cv::MORPH_DILATE,
                     cv::Mat::ones(cv::Size(temp_w, temp_w) , CV_8UC1));
    if(dm::match_img){
        cv::imwrite(path + std::to_string(src_w) + "_dilated_contours_orig.png", dilated_contours);
    }
    cv::Mat candidate_mat;// = cv::Mat::zeros(dilated_contours.size(), dilated_contours.type());

    // Jang 20220413
    // Resize the image to both up-left and down-right
    // Expanding size: template size
    cv::Mat expanded_candidate_mat;
    if(expand_img_){
        int expand_w = src_w + temp_w;
        int expand_h = src_h + temp_h;
        cv::Size expand_size;
        expand_size.width = expand_w;
        expand_size.height = expand_h;
        candidate_mat = cv::Mat::zeros(expand_size, dilated_contours.type());

        cv::Rect moving_roi_from(0, 0, src_w, src_h);
        cv::Rect moving_roi_to(0, 0, src_w, src_h);
        dilated_contours(moving_roi_from).copyTo(candidate_mat(moving_roi_to));
        cv::Rect mask_out_of_range_w(expand_w - temp_w - 2, 0, temp_w+2, expand_h);
        cv::Rect mask_out_of_range_h(0, expand_h - temp_h - 2, expand_w, temp_h+2);
        cv::add(candidate_mat(mask_out_of_range_w), -255, candidate_mat(mask_out_of_range_w));
        cv::add(candidate_mat(mask_out_of_range_h), -255, candidate_mat(mask_out_of_range_h));

        // For distans transform img
        expanded_candidate_mat = cv::Mat::zeros(expand_size, src_contours.type());
        cv::Rect moving_contour_to((int)(temp_w / 2), (int)(temp_h / 2), src_w, src_h);
        src_contours(moving_roi_from).copyTo(expanded_candidate_mat(moving_contour_to));
        src_contours.release();
        src_contours = expanded_candidate_mat.clone();

    }
    else{
        candidate_mat = cv::Mat::zeros(dilated_contours.size(), dilated_contours.type());
        cv::Rect moving_roi_from(temp_w / 2, temp_h / 2, src_w - (temp_w/2), src_h - (temp_h/2));
        cv::Rect moving_roi_to(0, 0, src_w - (temp_w/2), src_h - (temp_h/2));
        dilated_contours(moving_roi_from).copyTo(candidate_mat(moving_roi_to));
        cv::Rect mask_out_of_range_w(src_w - temp_w - 2, 0, temp_w+2, src_h);
        cv::Rect mask_out_of_range_h(0, src_h - temp_h - 2, src_w, temp_h+2);
        cv::add(candidate_mat(mask_out_of_range_w), -255, candidate_mat(mask_out_of_range_w));
        cv::add(candidate_mat(mask_out_of_range_h), -255, candidate_mat(mask_out_of_range_h));
    }


    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Time elapsed - morphologyEx: " << host_elapsed << "s" << lw.endl;
    }

    if(dm::match_img){
        cv::imwrite(path + std::to_string(src_w) + "_dilated_contours.png", candidate_mat);
    }
    std::vector<cv::Point> non_zero_area;
    cv::findNonZero(candidate_mat, non_zero_area);  // non_zero_area: coordinate(idx) of non-zero value

    cv::threshold(src_contours, src_contours_inv, 127, 255, cv::THRESH_BINARY_INV);

    if(dm::match_img){
    // Jang 20220307
        cv::imwrite(path + std::to_string(src_w) + "_orig_src.png", orig_src);
        cv::imwrite(path + std::to_string(src_w) + "_src_contours.png", src_contours);
        cv::imwrite(path + std::to_string(src_w) + "_contour_source_img_inv.png", src_contours_inv);
    }
    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Time elapsed - findNonZero: " << host_elapsed << "s" << lw.endl;
    }
    ////////////////////////////////// Create Chamfer Image for source image //////////////////////////////////
    // Calculates the distance to the closest zero pixel for each pixel of the source image
    cv::distanceTransform(src_contours_inv, preprocessed_source.chamfer_img, cv::DIST_L2, CV_16S);
    preprocessed_source.flat_source_dist_transfrom = (float*)preprocessed_source.chamfer_img.data;

    // Set candidate coordinate
    // Non zero area
    preprocessed_source.length_source_candidate = non_zero_area.size();     // The number of non-zero pixels

    if(preprocessed_source.length_source_candidate > 0){
        preprocessed_source.source_num_candidate_width = (short)sqrt(preprocessed_source.length_source_candidate) + 1;
        preprocessed_source.source_num_candidate_height = preprocessed_source.source_num_candidate_width;

        if (preprocessed_source.source_candidate_col != nullptr) {
            delete[] preprocessed_source.source_candidate_col;
            preprocessed_source.source_candidate_col = nullptr;
        }
        if (preprocessed_source.source_candidate_row != nullptr) {
            delete[] preprocessed_source.source_candidate_row;
            preprocessed_source.source_candidate_row = nullptr;
        }
        preprocessed_source.source_candidate_col = new short[preprocessed_source.length_source_candidate];
        preprocessed_source.source_candidate_row = new short[preprocessed_source.length_source_candidate];

        for(int i = 0; i < preprocessed_source.length_source_candidate; i ++){
            preprocessed_source.source_candidate_col[i] = non_zero_area.at(i).x;
            preprocessed_source.source_candidate_row[i] = non_zero_area.at(i).y;
        }
        non_zero_area.clear();
    }
    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Time elapsed - distanceTransform: " << host_elapsed << "s" << lw.endl;
    }
}
// Jang 202203330
void CudaChamfer::SetTemplateMask(cv::Mat template_mask){
    template_mask_ = template_mask;
    if(!template_mask.empty()){
        template_mask_exists_ = true;
    }
    else{
        template_mask_exists_ = false;
    }
}
void CudaChamfer::ClearTemplateMask(){
    template_mask_.release();
    pyr_template_mask_.release();
    template_mask_exists_ = false;
}

// Jang 20220406
void CudaChamfer::SetEnableSrcImageMask(bool enable_mask){
    const std::string orig_path = data_path + "/orig_source/";

    src_mask_exists_ = enable_mask;
    if(enable_mask){
        if(source_img_mask_.empty()){
            source_img_mask_ = cv::imread(orig_path + "source_img_mask_inv.png", cv::IMREAD_GRAYSCALE);
            if(source_img_mask_.empty()){
                src_mask_exists_ = false;
            }
        }
    }
    pyr_source_img_mask_.release();
}

void CudaChamfer::SetSrcImageMask(cv::Mat source_img_mask){
    const std::string orig_path = data_path + "/orig_source/";
    cv::imwrite(orig_path + "source_img_mask.png", source_img_mask );
    cv::threshold(source_img_mask, source_img_mask, 200, 255, cv::THRESH_BINARY_INV);
    cv::imwrite(orig_path + "source_img_mask_inv.png", source_img_mask );
    source_img_mask_ = source_img_mask;
    if(!source_img_mask.empty()){
        src_mask_exists_ = true;
    }
    else{
        src_mask_exists_ = false;
    }
    pyr_source_img_mask_.release();
}
void CudaChamfer::ClearSrcImageMask(){
    source_img_mask_.release();
    pyr_source_img_mask_.release();
    src_mask_exists_ = false;
}
// Jang 20220406
void CudaChamfer::GetSrcImageMask(cv::Mat& source_img_mask){
    const std::string orig_path = data_path + "/orig_source/";
    try{
        source_img_mask = cv::imread(orig_path + "source_img_mask.png", cv::IMREAD_GRAYSCALE);
    }
    catch(cv::Exception e){
        lw << e.what() << lw.endl;
        source_img_mask.release();
    }
}

// Jang 20220711
template <typename T>
void CudaChamfer::MakeSharedPtr(std::shared_ptr<T[]>& shared_ptr_char, int size){
    shared_ptr_char.reset();
    std::shared_ptr<T[]> tmp_ptr(new T[size]());
    shared_ptr_char = tmp_ptr;
    tmp_ptr.reset();
}
// Jang 20220530
void CudaChamfer::MakeSharedPtr(std::shared_ptr<uchar[]>& shared_ptr_char, int size){
    shared_ptr_char.reset();
    std::shared_ptr<uchar[]> tmp_ptr(new uchar[size]());
    shared_ptr_char = tmp_ptr;
    tmp_ptr.reset();
}
void CudaChamfer::MakeSharedPtr(std::shared_ptr<char[]>& shared_ptr_char, int size){
    shared_ptr_char.reset();
    std::shared_ptr<char[]> tmp_ptr(new char[size]());
    shared_ptr_char = tmp_ptr;
    tmp_ptr.reset();
}
void CudaChamfer::MakeSharedPtr(std::shared_ptr<short[]>& shared_ptr_short, int size){
    shared_ptr_short.reset();
    std::shared_ptr<short[]> tmp_ptr(new short[size]());
    shared_ptr_short = tmp_ptr;
    tmp_ptr.reset();
}
void CudaChamfer::MakeSharedPtr(std::shared_ptr<int[]>& shared_ptr_int, int size){
    shared_ptr_int.reset();
    std::shared_ptr<int[]> tmp_ptr(new int[size]());
    shared_ptr_int = tmp_ptr;
    tmp_ptr.reset();
}
void CudaChamfer::MakeSharedPtr(std::shared_ptr<float[]>& shared_ptr_float, int size){
    shared_ptr_float.reset();
    std::shared_ptr<float[]> tmp_ptr(new float[size]());
    shared_ptr_float = tmp_ptr;
    tmp_ptr.reset();
}
void CudaChamfer::MakeSharedPtr(std::shared_ptr<double[]>& shared_ptr_double, int size){
    shared_ptr_double.reset();
    std::shared_ptr<double[]> tmp_ptr(new double[size]());
    shared_ptr_double = tmp_ptr;
    tmp_ptr.reset();
}
CudaChamfer::~CudaChamfer(){
    orig_template_img_mat_.release();
    pyr_template_img_mat_.release();

    FreeMemory(&trained_template.flat_template_non_zero_col_);
    FreeMemory(&trained_template.flat_template_non_zero_row_);
    FreeMemory(&trained_template.num_non_zeros);
    FreeMemory(&trained_template.flat_template_weight_);

    FreeMemory(&trained_template_pyr.flat_template_non_zero_col_);
    FreeMemory(&trained_template_pyr.flat_template_non_zero_row_);
    FreeMemory(&trained_template_pyr.num_non_zeros);
    FreeMemory(&trained_template_pyr.flat_template_weight_);

    FreeMemory(&preprocessed_source.flat_score_map);
//        FreeMemory(&preprocessed_source.flat_source_dist_transfrom); // This is already released
    FreeMemory(&preprocessed_source.source_candidate_col);
    FreeMemory(&preprocessed_source.source_candidate_row);
    preprocessed_source.chamfer_img.release();

    FreeMemory(&preprocessed_source_pyr.flat_score_map);
//        FreeMemory(&preprocessed_source_pyr.flat_source_dist_transfrom); // This is already released
    FreeMemory(&preprocessed_source_pyr.source_candidate_col);
    FreeMemory(&preprocessed_source_pyr.source_candidate_row);
    preprocessed_source_pyr.chamfer_img.release();


    // Clean and init.
    if(trained_template.templates_gray.size() > 0){
        trained_template.templates_gray.clear();
    }
    if(trained_template.templates.size() > 0){
        trained_template.templates.clear();
    }
    if(trained_template.template_angle.size() > 0){
        trained_template.template_angle.clear();
    }
    if(trained_template.template_scale.size() > 0){
        trained_template.template_scale.clear();
    }
    if(trained_template.non_zero_area_templates.size() > 0){
        trained_template.non_zero_area_templates.clear();
    }
    if(trained_template.template_weights.size() > 0){
        trained_template.template_weights.clear();
    }
    if(trained_template.num_non_zeros != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete []trained_template.num_non_zeros;
    }
    if(trained_template.flat_template_non_zero_row_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template.flat_template_non_zero_row_;
        trained_template.flat_template_non_zero_row_ = nullptr;
    }
    if(trained_template.flat_template_non_zero_col_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template.flat_template_non_zero_col_;
        trained_template.flat_template_non_zero_col_ = nullptr;
    }
    if(trained_template.flat_template_weight_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template.flat_template_weight_;
        trained_template.flat_template_weight_ = nullptr;
    }

    if(trained_template_pyr.templates_gray.size() > 0){
        trained_template_pyr.templates_gray.clear();
    }
    if(trained_template_pyr.templates.size() > 0){
        trained_template_pyr.templates.clear();
    }
    if(trained_template_pyr.template_angle.size() > 0){
        trained_template_pyr.template_angle.clear();
    }
    if(trained_template_pyr.template_scale.size() > 0){
        trained_template_pyr.template_scale.clear();
    }
    if(trained_template_pyr.non_zero_area_templates.size() > 0){
        trained_template_pyr.non_zero_area_templates.clear();
    }
    if(trained_template_pyr.template_weights.size() > 0){
        trained_template_pyr.template_weights.clear();
    }
    if(trained_template_pyr.num_non_zeros != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete []trained_template_pyr.num_non_zeros;
    }
    if(trained_template_pyr.flat_template_non_zero_row_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template_pyr.flat_template_non_zero_row_;
        trained_template_pyr.flat_template_non_zero_row_ = nullptr;
    }
    if(trained_template_pyr.flat_template_non_zero_col_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template_pyr.flat_template_non_zero_col_;
        trained_template_pyr.flat_template_non_zero_col_ = nullptr;
    }
    if(trained_template_pyr.flat_template_weight_ != nullptr){
        std::cout << "[DEBUG] Match: Free Memory, line " << __LINE__ << std::endl;
        delete[] trained_template_pyr.flat_template_weight_;
        trained_template_pyr.flat_template_weight_ = nullptr;
    }
}

double CudaChamfer::Train(/*template_img_*/) {
    if(template_img_.empty()) return 0;
    return ChamferTrain(template_img_);
}

double CudaChamfer::Inference(/*template_img_, source_img_*/) {
    if(source_img_.empty() || template_img_.empty()) return 0;
    if(chamfer_method_ == 0){
        return ChamferInference(source_img_, use_weight_);
    }
    else if(chamfer_method_ == 1){
        return ChamferInference2(source_img_, use_weight_);
    }
    else {
        assert(false);
    }
    return 0;
}

void CudaChamfer::SetupImages(const cv::Mat& template_img, const cv::Mat& src_img)
{
    cv::Mat temp_mat(template_img.size(), CV_8UC1);
    cv::Mat src_mat(src_img.size(), CV_8UC1);

    if (template_img.empty() || src_img.empty()) {
        std::cerr << "[Error] No template or source image, pleas check!\n";
        return;
    }

    // Convert BGR images to gray-scale images
    cv::cvtColor(template_img, temp_mat, cv::COLOR_BGR2GRAY);
    cv::cvtColor(src_img, src_mat, cv::COLOR_BGR2GRAY);
    template_img_ = temp_mat;
    source_img_ = src_mat;
}


void CudaChamfer::ShowResultImage(std::string saveFilename)
{
    cv::Mat mat = source_img_.clone();
    cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
    std::vector<cv::Point> pts;
    pts.reserve(6);
    try {
        std::ifstream ifs(data_path + result_matched_area_path, std::fstream::in);
        if (ifs.is_open()) {
            std::string line;
            for (line = "\0"; std::getline(ifs, line);) {
                cv::Point pt(0, 0);
                pt.x = std::stoi(line);
                std::getline(ifs, line);
                pt.y = std::stoi(line);

                pts.push_back(pt);
            }
        } else {
            ifs.close();
            std::cerr << "Cannot open " <<
                         data_path + result_matched_area_path << std::endl;
            return;
        }
        ifs.close();
    } catch (std::exception e) {
        std::cerr << "[IO Error] Load result_drawing_pts.data error: "
             << e.what() << std::endl;
    }
    cv::circle(mat, pts.at(0), 5, cv::Scalar(0, 0, 255), -1);
    cv::line(mat, pts.at(0), pts.at(1), cv::Scalar(0, 255, 0), 2);
    cv::line(mat, pts.at(2), pts.at(3), cv::Scalar(0, 255, 255), 2);
    cv::line(mat, pts.at(3), pts.at(4), cv::Scalar(0, 255, 255), 2);
    cv::line(mat, pts.at(4), pts.at(5), cv::Scalar(0, 255, 255), 2);
    cv::line(mat, pts.at(5), pts.at(2), cv::Scalar(0, 255, 255), 2);
//    cv::namedWindow("Result", cv::WINDOW_GUI_NORMAL);
//    cv::imshow("Result", mat);
    cv::imwrite(data_path + saveFilename, mat);
}

std::vector<cv::Point> CudaChamfer::GetPointResult()
{
    std::vector<cv::Point> pts;
    pts.reserve(6);
    try {
        std::ifstream ifs(data_path + result_matched_area_path, std::fstream::in);
        if (ifs.is_open()) {
            std::string line;
            for (line = "\0"; std::getline(ifs, line);) {
                cv::Point pt(0, 0);
                pt.x = std::stoi(line);
                std::getline(ifs, line);
                pt.y = std::stoi(line);

                pts.push_back(pt);
            }
        } else {
            ifs.close();
            std::cerr << "Cannot open " <<
                         data_path + result_matched_area_path << std::endl;
            return pts;
        }
        ifs.close();
    } catch (std::exception e) {
        std::cerr << "[IO Error] Load result_drawing_pts.data error: "
             << e.what() << std::endl;
    }
    return pts;
}
