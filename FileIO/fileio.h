#ifndef FILEIO_H
#define FILEIO_H

/*****************************************************
 * Goal:    Do file system input output operation.
 * Author:  Jimmy.  V1B3    20220420.
 ****************************************************/

#include <fstream>
#include <iostream>
#include <string>
#include <streambuf>
#include <sstream>
#include <vector>
#include <iterator>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <memory>

#include "inimanager.h"

#if defined(_WIN32)
    #include <filesystem> // if Windows
    namespace fs = std::filesystem;
#elif defined(__unix)
    #include <experimental/filesystem> // if Linux on Jetson
    namespace fs = std::experimental::filesystem;
#endif

// DEFINITION
#define DEBUG_FIO

namespace file_io {

#if defined(__unix)
    // Test file exist or not
    inline bool exists (const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }
    inline bool mkdir (const std::string &path) {
        return fs::create_directories(path);
    }
#endif

enum class FileioStatus {
    IDLE = 0,
    OK,
    FAIL,
    NO_FILE_FOUND,
    NO_TARGET_FOUND,
    WRONG_FORMAT,
};

enum class FileioFormat {
    RW = 0,
    R,
    W,
    INI,
};

class FileIO
{
    friend class ini_manager::INIManager;

public:
    FileIO(const std::string& file_path = "", FileioFormat format = FileioFormat::RW);
    FileIO operator =(const FileIO& fileio);
    FileIO(const FileIO& fileio);
    ~FileIO();

    // Setter and Getter
    FileioStatus status() const { return status_; }
    int GetErrorNum() const { return error_cnt_; }

    // Other functions
    std::string StatusLog() const;
    bool isEmpty() const;
    bool isOK() const { if (status_ == FileioStatus::OK) return true; return false; }
    bool isAllOK() const { if (!error_cnt_) return true; return false; }
    void clear();
    void close();

    // Write something
    void Write(const std::string& content);
    void Write(const std::string& key, const std::string& content);
    void Write(const std::string& key, const std::string& delimiter, const std::string& content);

    void Replace(const std::string& old_str, const std::string& new_str);
    void ReplaceByKey(const std::string& key, const std::string& content);
    void ReplaceByKey(const std::string &key, const std::string& delimiter, const std::string &content);

    // Read something
    std::string ReadAll();
    std::string Read(const std::string& key);
    std::string Read(const std::string& key,
                     const std::string& delimiter,
                     const std::string &default_val = "\0");

    int ReadtoInt(const std::string& key,
                  const std::string& delimiter,
                  const int& default_val = 0);
    int ReadtoInt(const std::string& key, const int& default_val = 0) {
        return ReadtoInt(key, "=", default_val);
    }

    float ReadtoFloat(const std::string& key,
                      const std::string& delimiter,
                      const float& default_val = 0.0);
    float ReadtoFloat(const std::string& key, const float& default_val = 0.0) {
        return ReadtoFloat(key, "=", default_val);
    }

    double ReadtoDouble(const std::string& key,
                        const std::string& delimiter,
                        const double& default_val = 0.0);
    double ReadtoDouble(const std::string& key, const double& default_val = 0.0) {
        return ReadtoDouble(key, "=", default_val);
    }

    ////////////////////////////////////////////////////////////////////////
    // INI Read and Write function
    void IniSetSection(const std::string& section);

    std::string IniRead(const std::string& section,
                        const std::string& key,
                        const std::string &default_val = "\0");
    std::string IniRead(const std::string& key) {
        return IniRead(inim_->curr_section(), key, "\0");
    }
    int IniReadtoInt(const std::string& section,
                     const std::string& key,
                     const int& default_val = 0);
    int IniReadtoInt(const std::string& key, const int& default_val = 0) {
        return IniReadtoInt(inim_->curr_section(), key, default_val);
    }
    float IniReadtoFloat(const std::string& section,
                         const std::string& key,
                         const float& default_val = 0.0);
    float IniReadtoFloat(const std::string& key, const float& default_val = 0.0) {
        return IniReadtoFloat(inim_->curr_section(), key, default_val);
    }
    double IniReadtoDouble(const std::string& section,
                           const std::string& key,
                           const double& default_val = 0.0);
    double IniReadtoDouble(const std::string& key, const double& default_val = 0.0) {
        return IniReadtoDouble(inim_->curr_section(), key, default_val);
    }

    void IniInsert(const std::string& section, const std::string& key, const std::string& value);
    void IniInsert(const std::string& key, const std::string& value) { IniInsert(inim_->curr_section(), key, value); }
    void IniInsert(const std::string& section, const std::string& key, const int& value)
                { IniInsert(section, key, std::to_string(value)); }
    void IniInsert(const std::string& key, const int& value) { IniInsert(inim_->curr_section(), key, value); }
    void IniInsert(const std::string& section, const std::string& key, const float& value)
                { IniInsert(section, key, std::to_string(value)); }
    void IniInsert(const std::string& key, const float& value) { IniInsert(inim_->curr_section(), key, value); }
    void IniInsert(const std::string& section, const std::string& key, const double& value)
                { IniInsert(section, key, std::to_string(value)); }
    void IniInsert(const std::string& key, const double& value) { IniInsert(inim_->curr_section(), key, value); }
    void IniSave();

private:
    std::string file_path_;
    std::shared_ptr<ini_manager::INIManager> inim_;
    std::unique_ptr<std::fstream> fs_;

    FileioStatus status_;
    FileioFormat format_;

    int error_cnt_;

    // Functions
    bool IsFileOpened();
    void InitPos();

};

// inline functions
inline int FileIO::ReadtoInt(const std::string &key, const std::string &delimiter, const int &default_val)
{
    int result = default_val;
    try{
        result = std::stoi(this->Read(key, delimiter));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

inline float FileIO::ReadtoFloat(const std::string &key, const std::string &delimiter, const float &default_val)
{
    float result = default_val;
    try {
        result = std::stof(this->Read(key, delimiter));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

inline double FileIO::ReadtoDouble(const std::string &key, const std::string &delimiter, const double &default_val)
{
    double result = default_val;
    try {
        result = std::stod(this->Read(key, delimiter));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

inline float FileIO::IniReadtoFloat(const std::string& section, const std::string& key, const float &default_val)
{
    float result = default_val;
    try {
        result = std::stof(this->IniRead(section, key));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

inline int FileIO::IniReadtoInt(const std::string &section, const std::string &key, const int &default_val)
{
    int result = default_val;
    try{
        result = std::stoi(this->IniRead(section, key));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

inline double FileIO::IniReadtoDouble(const std::string& section, const std::string& key, const double &default_val)
{
    double result = default_val;
    try {
        result = std::stod(this->IniRead(section, key));
    } catch(std::invalid_argument const& ex) {
        result = default_val;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] invalid argument: " << ex.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return result;
    }
    status_ = FileioStatus::OK;
    return result;
}

}

#endif // FILEIO_H
