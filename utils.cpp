#include "Chamfer/chamfermatch.h"

bool CompareSize(const MatchData& data_0, const MatchData& data_1)
{
    return (data_0.coef > data_1.coef);
}

int ParseLine(char* line){
    int i = strlen(line);
    const char* p = line;
    while(*p < '0' || *p > '9') p++;    // Search until a number is found.
    line[i - 3] = '\0'; // Remove " kB"
    i = atoi(p);
    return i;
}

int GetCurrentMem(){
    std::string cur_proc;
    cur_proc = "/proc/" + std::to_string((int)getpid()) + "/status";

    FILE* fs = fopen(cur_proc.c_str(), "r");
    int result = -1;
    char line[128];

    while(fgets(line, 128, fs) != NULL){
        if(strncmp(line, "VmRSS:", 6) == 0){
            result = ParseLine(line);
            break;
        }
    }
    fclose(fs);
    fs = NULL;  //garly modify, for test memory problem
    return result;
}
