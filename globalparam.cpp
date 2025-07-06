#include "globalparams.h"
#include "Log/logwriter.h"

namespace debug_mode {
    bool log = true ;
    bool match_img = false;
    bool plc_log = false;
    bool align_img  = true;
}

namespace operator_log {
    logwriter::LogWriter lw;

    void WriteLog(std::string str){
        if(lw.IsReady()){
            lw << str << lw.endl;
        }
    }
}
