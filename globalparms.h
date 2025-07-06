#ifndef GLOBALPARMS_H
#define GLOBALPARMS_H

#define MC_PROTOCOL_PLC // add by garly

/* ======== System Version Edition ======== */
//garly_20211221, add for software version
#define MACHINE_TYPE        "AS"        //alignment system
#define WORK_ORDER_NUM      "1980B021"
#define SOFTWARE_VERSION    "1.5.0"
#define SUB_VERSION         "3"

/// \brief
///     { Jimmy 20240409 }
///         1. Fixed the software terminated problem while starting up the program.
///             1.1 Reason is PLC setting parms. loading problem: std::stoi() error.
///             1.2 in "plcconnector.cpp", modified the LoadSettingfromFile() function.
///         2. Added: LoadSettingFromBackupFile() for reading back up setting parms.
///         3. Added: Warning message when LoadSettingFromBackupFile() raise error: cannot read the file.
///

// This definition must be changed when the system is different.
#define SYSTEM_CONTREL_1
//#define SYSTEM_CONTREL_2
#ifdef SYSTEM_CONTREL_1	//contrel ver.
#define PLC_RADIUS  90
#define ThetaX1     90
#define ThetaX2     270
#define ThetaY      180
#define MM2PULSE    10000   // 1 mm = 10000 pulses
#define DEG2PULSE   1000    // 1 degree = 1000 pulses
#define MM2PLCUNIT  10000   // 1 mm -> 10000
#define DEG2PLCUNIT 10000   // 1 degree -> 10000
#else
#ifdef SYSTEM_CONTREL_2
#define PLC_RADIUS  90
#define ThetaX1     90
#define ThetaX2     270
#define ThetaY      180
#define MM2PULSE    10000
#else	//ncku ver.
#define PLC_RADIUS  42.43
#define ThetaX1     315
#define ThetaX2     135
#define ThetaY      225
#define MM2PULSE    1600
#endif
#endif

//#define IMAGE_FROM_PICTURE    //for offline test
//#define REPEAT_TEST_WITHOUT_MOVE
//#define ORIGINAL_SHAPE_ALIGN

//#define DEBUG_MODE    // This has been replaced to dm::log // Jang 20220418
#define DEBUG(x) do { std::cerr << #x << ": " << x << std::endl; } while(0)
//#define DEBUG_PLC     // This has been replaced to dm::plc_log // Jang 20220418
//#define DEBUG_SAVE_IMGS // saving images while matching process // This has been replaced to dm::match_img // Jang 20220418
//#define SPINNAKER_CAM_API;

//auto-update system time from plc
//20220224 note: plc didn't finish the function
#define SYNC_TIME_FROM_PLC
//#define BARCODE_READER
#define PRE_DETECT //garly_20221007
#define DEFECT_DETECTION    //garly_20241024
#define TRIAL_FUNCTION	    //garly_20241024
//#define ADJUST_ALIGN_TOL  //note: 20241209, customer request take it off

/// ETC
#define ROUNDF(f,c) (((float)((int)((f) * (c))) / (c)))
//result formate
#define RESULT_FORMAT_XXY     //default
//#define RESULT_FORMAT_XYT   //change to use XYT
#define OPENCV4_CUDA // Need to build OpenCV with CUDA
#define ADAPT_CAM_RESOLUTION_SEPARATE
#define COARSE_ALIGN_SYSTEM
// For light controller (OPT device)
#ifdef COARSE_ALIGN_SYSTEM
#define DEVICE_IP "192.168.1.16" // test device: "192.168.10.61"
#endif
#ifdef FINE_ALIGN_SYSTEM
#define DEVICE_IP "192.168.1.17" // test device: "192.168.10.61"
#endif
#define PC_IP "192.168.1.208"
#define DEVICE_PORT 8000

/// UNDONE FUNCTION
#define ENABLE_SHAPE_ALIGNMENT
// add for disable lightsourcethread and class cameraSettingDialog. Jimmy. 20220224.
//#define ENABLE_AUTO_ADJUST_BRIGHTNESS

enum _AUTHORITY_ {
    _AUTH_OPERATOR_ = 0,
    _AUTH_ENGINEER_,
    _AUTH_SUPER_USER_
};

enum _SYSTEM_TYPE_ {        // To check the alignment type
    _SYSTEM_NONE_ = 0,
    _SYSTEM_COARSE_ALIGN_ = 1,
    _SYSTEM_FINE_ALIGN_ = 2,
    _SYSTEM_BARCODE_READER = 3	//garly_20220310
};

enum _ALIGN_TYPE_ {
    _SKIP_ALIGN_SEND_OFFSET_ = 0,
    _DO_ALIGN_
};

enum _Mode_
{
    _MODE_IDLE_ = 0,
    _MODE_MARKER_,
    _MODE_CHAMFER_,
    _MODE_SHAPE_ROI_,       // The alignment using the shape detection with ROI (But it doesn't work for calibration process)
    _MODE_SHAPE_ONE_WAY_,    // The alignment using the one-way shape detection
    _MODE_BARCODE_READER_   //add for barcode reader	//garly_20220310
};

enum _CALIB_MODE_ {
    _MODE_CALIB_XYT_ = 0,   // 2 axis and rotation
    _MODE_CALIB_YT_ = 1
};

enum _MACHINE_TYPE_ {
    _MACHINE_UVW_ = 0,  // 2 axis and rotation
    _MACHINE_YT_        // 1 axis and rotation
};

enum _SYSTEM_UNIT_ {
    _UNIT_PULSE_ = 0,
    _UNIT_MM_DEG_
};

enum _FUNC_MODE_
{
    _MODE_NONE_ = 0,
    _MODE_MARKER_DETECT_,
    _MODE_CALIBRATION_,
    _MODE_SET_REF_,
    _MODE_ALIGNMENT_,
    _MODE_PREDETECT_,
    _MODE_DEFECT_
};

enum _MARKER_MODE_ {
    _ONLY_TEMPLATE_ = 0,    // Only one template is used for both cameras
    _EACH_TEMPLATE_L_ = 1,     // Each template is used for each camera
    _EACH_TEMPLATE_R_ = 2
};

enum _CAMERA_SDK_ {
    BASLER_PYLON = 0,
    FLIR_SPINNAKER = 1
};

enum _LightTiming_
{
    _ALWAYS_OPEN_ = 0,
    _ALWAYS_CLOSE_,
    _ALIGNMENT_OPEN_
};

enum _LightState_
{
    _LightTurnOff_ = 0,
    _LightTurnOn_
};

enum _PLC_Format_
{
    _PLC_Format_Binary_ = 0,
    _PLC_Format_ASCII_
};

// Jang 20220214
enum _ALIGN_VALID_TYPE_{
    _ALIGN_VALID_TWO_REF_PTS_ = 0,
    _ALIGN_VALID_REF_CENTER_
};

// Jang 20220425
enum _MACHINE_COORD_ {
    _MACHINE_COORD_000_ = 0,
    _MACHINE_COORD_CW_090_,
    _MACHINE_COORD_CW_180_,
    _MACHINE_COORD_CW_270_
};
// Jang 20220425
enum class _MACHINE_THETA_ {
    _CW_ = 0,
    _CCW_
};

// for Barcode Detection. Jimmy. 20220302.
enum class BarcodeDetectState {
    kIdle = 0,
    kOK,
    kNG,
    kError,
};

#include <opencv2/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef OPENCV4_CUDA
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudafilters.hpp>
#endif

// Define coordinate structure
struct Coor {
    double x;
    double y;
};

struct CropInfo{
    bool do_crop_roi = false;
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = 0;
    int crop_h = 0;
    cv::Rect2i crop_roi;    // Rect2i == Rect
};

//limit setting	//garly_20241209
struct Range{
    double Max = 0.00;
    double Min = 0.00;
};
struct LimitRangeData{
    //[Max, Min]
    Range  calibDist    = {2.0, 1.8};
    Range  calibAng     = {0.3, 0.15};
    Range  calibTol     = {10, 5};
    Range  alignDistTol = {15, 10};
    Range  tol          = {0.10, 0.02};
    Range  alignRepeat  = {10, 3};
};
struct DefectDetectionSetting{
    bool  enableThresh = true;
    int   model = 1;  //0:default model, 1:new model
    int   thresh_objNum = 5;     //defect detection threshold obj num
    float thresh_score = 0.30;  //defect detection threshold score
    int   thresh_sizeW = 5;
    int   thresh_sizeH = 5;
};
struct DefectErrorResult{
    bool frame_enable = false;
    std::string errScore = "";
    int errSize = 0;
    bool bResult = false;
};

struct SystemSetting{
    int cam_rot_l = 0;
    int cam_rot_r = 0;

    int cam_resolution_w = 1280;
    int cam_resolution_h = 1024;

    int system_type = _SYSTEM_FINE_ALIGN_;
    int machine_type = _MACHINE_TYPE_::_MACHINE_UVW_;
    int align_type = _ALIGN_TYPE_::_DO_ALIGN_;
    int calibration_mode = _MODE_CALIB_XYT_;
    int system_unit = _UNIT_MM_DEG_;

    int camera_l_idx = 1;
    int camera_r_idx = 0;
    bool cam_flip_l = true;
    bool cam_flip_r = true;
    int lightsource_value = 30;
    bool cam_title_swap = false;    //default : left = cam1, right = cam2   //garly_20230206, cam title swap
#ifdef SYNC_TIME_FROM_PLC
    bool sync_time_mode = true;  //false: manual, true: auto
    uint8_t sync_time_unit = 5;		//default: disable
#endif

    //garly_20241024
    bool TrialEnable = false;
    int TrialCounter = 0;
    int TrialCounterMax = 0;
    std::string CreatTrialDate = "";
#ifdef DEFECT_DETECTION
    DefectDetectionSetting defectSetting;
#endif

    bool use_cuda_for_image_proc = false;
    bool enable_mask = false;

    int align_repeat = 3;
    float tolerance = 0.02;
    double align_distance_tol = 10; // Jimmy 20221124
    float calib_distance = 1.8;
    float calib_rot_degree = 0.15;
    float cam_exposure = 150;
    float cam_gain = 0;
    int calib_dist_diff_tolerance = 10;
    std::string tolerance_unit = "mm";

    std::string opt_light_IP = "192.168.1.17";  // "192.168.1.16" "192.168.1.17"
    int opt_light_port = 8000;
    std::string opt_light_sn = "None";

    CropInfo crop_l;
    CropInfo crop_r;

    //limit setting	//garly_20241209
    LimitRangeData limitSetting;
};

// Jang 20220411
// Global variables
namespace debug_mode {
    extern bool log ;
    extern bool match_img ;
    extern bool plc_log ;
    extern bool align_img ;
    extern bool iqa_all_img;    // 20230613, image quality analize for all align images
}
namespace dm = debug_mode;

// Jang 20220413
#include "Log/logwriter.h"
namespace operator_log {
    extern logwriter::LogWriter lw;

    void WriteLog(std::string str);
}
namespace ol = operator_log;

#endif // GLOBALPARMS_H
