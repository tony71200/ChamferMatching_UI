#include "inimanager.h"

using namespace ini_manager;

INIManager::INIManager(const std::string &file_path)
    : file_path_(file_path), fs_(nullptr), curr_section_("")
{
    fs_ = std::make_unique<std::fstream>(file_path, std::fstream::in | std::fstream::out);

    if (!*fs_){
        fs_ = std::make_unique<std::fstream>(file_path, std::fstream::out);
    } else {
        // Setup the root_
        std::string str_line;
        int count = 0;

        if (std::getline(*fs_, str_line))
            do {
                size_t pos = 0;
                if (str_line.find('[') != std::string::npos){
                    pos = str_line.find('[');
                    std::string section = str_line.substr(pos + 1, str_line.length() - 2);  // Get section name tag
                    std::map<std::string, std::string> section_element;

                    while (std::getline(*fs_, str_line)){
                        if((pos = str_line.find('=')) != std::string::npos){    // Save to root_
                            std::string key = str_line.substr(0, pos);
                            std::string val = str_line.substr(pos + 1, str_line.length());

                            section_element.insert(std::pair<std::string, std::string>(key, val));  // insert the element
                        }else break;
                    }

                    root_map_.push_back(section_element);
                    idx_map_.insert(std::pair<std::string, int>(section, count++));
                }
            } while (std::getline(*fs_, str_line));
        //else: is an empty file!

        // Go back to the beginning to keep searching with other keywords
        fs_->clear();
        fs_->seekg(0, std::ios::beg);
    }
}

INIManager INIManager::operator =(const INIManager &inim)
{
    if (this == &inim)
        return *this;

    if (*this->fs_)
        this->fs_->close();
    fs_.release();

    this->fs_ = std::make_unique<std::fstream>(inim.file_path_);
    this->file_path_ = inim.file_path_;
    this->curr_section_ = inim.curr_section_;
    this->idx_map_ = inim.idx_map_;
    this->root_map_ = inim.root_map_;
    return *this;
}

INIManager::INIManager(const INIManager &inim)
{
    if (*this->fs_)
        this->fs_->close();
    fs_.release();

    this->fs_ = std::make_unique<std::fstream>(inim.file_path_);
    this->file_path_ = inim.file_path_;
    this->curr_section_ = inim.curr_section_;
    this->idx_map_ = inim.idx_map_;
    this->root_map_ = inim.root_map_;
}

INIManager::~INIManager()
{
    this->Close();
}

std::string INIManager::Read(const std::string &section, const std::string &key) const
{
    auto it_sec = idx_map_.find(section);
    std::map< std::string, std::string > element;

    if (it_sec != idx_map_.end()){
        int idx_sec = it_sec->second;
        element = root_map_.at(idx_sec);
        auto it_e = element.find(key);

        if (it_e != element.end()){
            return it_e->second;
        }
    }else {
        return "";
    }
    return "";
}

void INIManager::Insert(const std::string &section, const std::string &key, const std::string &value)
{
    auto it_sec = idx_map_.find(section);
    std::map< std::string, std::string > element;

    if (it_sec != idx_map_.end()){
        int idx_sec = it_sec->second;
        element = root_map_.at(idx_sec);
        auto it_e = element.find(key);

        if (it_e != element.end()){ // if found, rewrite the value
            it_e->second = value;
            root_map_.at(idx_sec).at(key) = value;
        } else {    // add a new element
            root_map_.at(idx_sec).insert(std::pair<std::string, std::string>(key, value));
        }
    } else { // create a new section, and add the element
        std::map< std::string, std::string > new_section;
        new_section.insert(std::pair<std::string, std::string>(key, value));
        idx_map_.insert(std::pair<std::string, int>(section, root_map_.size()));
        root_map_.push_back(new_section);
        //std::cout << ">> I'm insert new section and element." << std::endl;
    }
}

void INIManager::Save()
{
    //auto it_element = root_map_.begin();
    auto it_section = idx_map_.begin();

    while (it_section != idx_map_.end()){
        auto it_element = root_map_.at(it_section->second);
        auto it = it_element.begin();
        std::string section_name = it_section->first;

        *fs_ << "[" << section_name << "]\n";
        //std::cout << "[" << section_name << "]\n";
        while (it != it_element.end()){
            // Write data
            *fs_ << it->first << "=" << it->second << "\n";
            //std::cout << it->first << "=" << it->second << "\n";
            it++;
        }
        *fs_ << "\n";
        //std::cout << std::endl;

        it_section++;
    }
}

void INIManager::Close()
{
    if (fs_){
        file_path_ = "";
        curr_section_ = "";
        root_map_.clear();
        idx_map_.clear();
        if(fs_->is_open())
            fs_->close();
        fs_.release();
    }
}
