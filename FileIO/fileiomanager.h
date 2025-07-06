#ifndef FILEIOMANAGER_H
#define FILEIOMANAGER_H

#include <fstream>
#include <iostream>
#include <string>
#include <streambuf>
#include <iterator>
//#include <filesystem>

#if defined(_WIN32)
    #include <filesystem> // if Windows
    namespace fs = std::filesystem;
    using namespace std::filesystem;
#elif defined(__unix)
    #include <experimental/filesystem> // if Linux on Jetson
    namespace fs = std::experimental::filesystem;
    using namespace std::experimental::filesystem;
#endif

namespace file_io_manager {
    enum ERROR_STATUS{
        IDLE = 0,
        OK,
        FAIL,
        NO_TARGET_FOUND

    };

    int WriteText(std::ofstream& fout, std::string name, std::string content);
    int WriteText(std::ofstream& fout, std::string name, std::string delimiter, std::string content);
    //int WriteINIText(std::ofstream& fout, const std::string& section, const std::string& name, const std::string& content);

    int ReadText(std::ifstream& fin, std::string name, std::string delimiter, std::string& content);
    int ReadText(std::ifstream& fin, std::string name, std::string& content);
    int ReadINIText(std::ifstream& fin, const std::string& section, const std::string& name, std::string& content);
    int ReadINIText(std::ifstream& fin, const std::string& section, const std::string& name, int& content);

    bool IsFilesSame(std::ifstream& fin1, std::ifstream& fin2);
}
#endif // FILEIOMANAGER_H
