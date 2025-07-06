#ifndef INIMANAGER_H_
#define INIMANAGER_H_

/****************************************
 * Goal:    Read and Write the INI files.
 * Author:  Jimmy.  V1B1    20220111.
 ****************************************/

#if defined(_WIN32)
    #include <experimental/filesystem> // if Windows
#elif defined(__unix)
    #include <sys/stat.h>   // only for linux
#endif

#include <fstream>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <iterator>
#include <map>
#include <unordered_map>
#include <memory>

namespace ini_manager {

#if defined(_WIN32)
    inline bool exists (const std::string& path) {
        return std::experimental::filesystem::exists(path);
    }
#elif defined(__unix)
    // Test file exist or not
    inline bool exists (const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }
#endif

    /// Hint: if you wanna "new" a class, use:
    /// std::unique_ptr<im::INIManager> inim_ptr(new im::INIManager(path));
    /// To handle pointer class.
    class INIManager
    {
    public:
        INIManager(const std::string& file_path);
        INIManager operator =(const INIManager& inim);
        INIManager(const INIManager& inim);
        ~INIManager();

        std::string curr_section() const { return curr_section_; }

        void SetSection(const std::string& section) { curr_section_ = section; }
        //ERROR_STATUS GetStatus() const { return (ERROR_STATUS)curr_status_; }

        std::string Read(const std::string& section, const std::string& key) const ;

        void Insert(const std::string& section, const std::string& key, const std::string& value);
        void Save();

        void Close();
        // status
        //bool exists() const { return this->fs_->is_open(); }

    private:
        std::string file_path_;
        std::unique_ptr< std::fstream > fs_;
        std::string curr_section_;
        // curr_status_;

        std::vector< std::map<std::string, std::string> > root_map_;
        std::map<std::string, int> idx_map_;

    };
}

#endif // INIMANAGER_H
