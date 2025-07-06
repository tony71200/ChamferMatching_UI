#include "fileiomanager.h"


namespace file_io_manager {

    int WriteText(std::ofstream& fout, std::string name, std::string delimiter, std::string content){
        if(!fout.is_open()){
            std::cerr << "File isn't opened." << std::endl;
            return ERROR_STATUS::FAIL;
        }

        fout << name;
        fout << delimiter;
        fout << content;
        fout << std::endl;
        return ERROR_STATUS::OK;
    }

    int WriteText(std::ofstream& fout, std::string name, std::string content){
        return WriteText(fout, name, "=", content);
    }

    int ReadText(std::ifstream& fin, std::string name, std::string delimiter, std::string& content){
        if(!fin.is_open()){
            std::cerr << "File isn't opened." << std::endl;
            return ERROR_STATUS::FAIL;
        }

        std::string str_line;
        content.clear();
        content = "";

        while(std::getline(fin, str_line)){
            size_t pos = 0;

            if(str_line.find(name) != std::string::npos){
                if((pos = str_line.find(delimiter)) != std::string::npos){
                    content = str_line.substr(pos + delimiter.length(), str_line.length());
                }
                break;
            }
        }

        // Go back to the beginning to keep searching with other keywords
        fin.clear();
        fin.seekg(0, std::ios::beg);

        if(content == ""){
            return ERROR_STATUS::NO_TARGET_FOUND;
        }
        return ERROR_STATUS::OK;
    }

    int ReadText(std::ifstream& fin, std::string name, std::string& content){
        return ReadText(fin, name, "=", content);
    }

    bool IsFilesSame(std::ifstream &fin1, std::ifstream &fin2)
    {
        if(!fin1.is_open()){
            std::cerr << "First files isn't opened." << std::endl;
            return false;
        }
        if(!fin2.is_open()){
            std::cerr << "Second files isn't opened." << std::endl;
            return false;
        }

        std::string str_fin1((std::istreambuf_iterator<char>(fin1)),
                             std::istreambuf_iterator<char>());
        std::string str_fin2((std::istreambuf_iterator<char>(fin2)),
                             std::istreambuf_iterator<char>());

//        std::cout << "str_fin1: " << str_fin1 << std::endl;
//        std::cout << "str_fin2: " << str_fin2 << std::endl;

        if (str_fin1 == str_fin2)
            return true;
        else
            return false;
    }

    int ReadINIText(std::ifstream &fin, const std::string &section, const std::string &name, std::string &content)
    {
        if(!fin.is_open()){
            std::cerr << "File isn't opened." << std::endl;
            return ERROR_STATUS::FAIL;
        }

        std::string str_line;
        content.clear();
        content = "";

        while(std::getline(fin, str_line)){
            size_t pos = 0;

            if (str_line.find(section) != std::string::npos){
                while (std::getline(fin, str_line)){
                    if(str_line.find(name) != std::string::npos){
                        if((pos = str_line.find('=')) != std::string::npos){
                            content = str_line.substr(pos + 1, str_line.length());
                        }
                        break;
                    }
                }
                break;
            }
        }

        // Go back to the beginning to keep searching with other keywords
        fin.clear();
        fin.seekg(0, std::ios::beg);

        if(content == ""){
            return ERROR_STATUS::NO_TARGET_FOUND;
        }
        return ERROR_STATUS::OK;
    }

    int ReadINIText(std::ifstream &fin, const std::string &section, const std::string &name, int &content)
    {
        std::string str_content = "";
        int status = 0;
        status = ReadINIText(fin, section, name, str_content);
        content = std::stoi(str_content);

        return status;
    }

//    int WriteINIText(std::ofstream &fout, const std::string &section, const std::string &name, const std::string &content)
//    {
//        if(!fout.is_open()){
//            std::cerr << "File isn't opened." << std::endl;
//            return ERROR_STATUS::FAIL;
//        }

//        fout << name;
//        fout << delimiter;
//        fout << content;
//        fout << std::endl;
//        return ERROR_STATUS::OK;
//    }
}
