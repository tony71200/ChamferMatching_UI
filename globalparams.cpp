#include "globalparms.h"
// Jang 20220411
// Global variables
namespace debug_mode {
    bool log = false ;
    bool match_img = false;
    bool plc_log = false;
    bool align_img  = true;
    bool iqa_all_img = false;
}

namespace operator_log {
    logwriter::LogWriter lw;

    void WriteLog(std::string str){
        if(lw.IsReady()){
            lw << str << lw.endl;
        }
    }

//    void WriteLog(QString str){
//        if(lw.init_finished){
//            lw << str.toStdString() << lw.endl;
//        }
//    }
}
