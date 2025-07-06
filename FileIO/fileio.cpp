#include "fileio.h"

file_io::FileIO::FileIO(const std::string& file_path, FileioFormat format)
    : file_path_(file_path),
      inim_(nullptr), fs_(nullptr),
      status_(FileioStatus::IDLE),
      format_(format),
      error_cnt_(0)
{
    if (file_path == "\0") {
        fs_ = std::make_unique<std::fstream>(nullptr);
        return;
    }

    switch (format) {
    // For file input/output, this is default case.
    case FileioFormat::RW:
        // 1. Check the file exist
        if (exists(file_path)) {
            // 2. if file is existed, read / write file
            fs_ = std::make_unique<std::fstream>(file_path, std::fstream::in  |
                                                            std::fstream::out |
                                                            std::fstream::app);
        } else {
            // 3. if file is not existed, create a new one and write it.
            fs_ = std::make_unique<std::fstream>(file_path, std::fstream::out);
        }

        if (!IsFileOpened())
            return;

        status_ = FileioStatus::OK;

        break;

    // for input(Only Read) file.
    case FileioFormat::W:
        fs_ = std::make_unique<std::fstream>(file_path, std::fstream::out);

        if (!IsFileOpened())
            return;

        status_ = FileioStatus::OK;
        break;

    // for output(Only Write) file.
    case FileioFormat::R:
        fs_ = std::make_unique<std::fstream>(file_path, std::fstream::in);

        if (!IsFileOpened())
            return;

        status_ = FileioStatus::OK;
        break;

    // for INI file input/output.
    case FileioFormat::INI:
        inim_ = std::make_shared<ini_manager::INIManager>(file_path);

        if (inim_)
            status_ = FileioStatus::OK;
        break;

    default:
        return;
    }
}

file_io::FileIO file_io::FileIO::operator =(const file_io::FileIO &fileio)
{
    // protect against invalid self-assignment
    if (this == &fileio)
        return *this;

    // 1: allocate new memory and copy the elements
    std::unique_ptr<std::fstream> new_fs = nullptr;
    std::shared_ptr<ini_manager::INIManager> new_inim = nullptr;
    if (fileio.format_ == FileioFormat::RW)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::in  |
                                                                   std::fstream::out |
                                                                   std::fstream::app);
    else if (fileio.format_ == FileioFormat::W)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::out);
    else if (fileio.format_ == FileioFormat::R)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::in);
    else if (fileio.format_ == FileioFormat::INI)
        new_inim = std::make_shared<ini_manager::INIManager>(fileio.file_path_);

    // 2: deallocate old memory
    this->fs_.release();
    inim_.reset();

    // 3: assign the new memory to the object
    this->fs_ = std::move(new_fs);
    this->inim_ = std::move(new_inim);
    this->file_path_ = fileio.file_path_;
    this->format_ = fileio.format_;
    this->status_ = fileio.status_;

    return *this;
}

file_io::FileIO::FileIO(const file_io::FileIO &fileio)
{
    // 1: allocate new memory and copy the elements
    std::unique_ptr<std::fstream> new_fs = nullptr;
    std::shared_ptr<ini_manager::INIManager> new_inim = nullptr;
    if (fileio.format_ == FileioFormat::RW)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::in  |
                                                                   std::fstream::out |
                                                                   std::fstream::app);
    else if (fileio.format_ == FileioFormat::W)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::out);
    else if (fileio.format_ == FileioFormat::R)
        new_fs = std::make_unique<std::fstream>(fileio.file_path_, std::fstream::in);
    else if (fileio.format_ == FileioFormat::INI)
        new_inim = std::make_shared<ini_manager::INIManager>(fileio.file_path_);

    // 2: deallocate old memory
    this->fs_.release();
    inim_.reset();

    // 3: assign the new memory to the object
    this->fs_ = std::move(new_fs);
    this->inim_ = std::move(new_inim);
    this->file_path_ = fileio.file_path_;
    this->format_ = fileio.format_;
    this->status_ = fileio.status_;
}

file_io::FileIO::~FileIO()
{
    this->clear();
    this->close();
}

std::string file_io::FileIO::StatusLog() const
{
    if (status() == FileioStatus::OK)
        return "[FileIO stat log] Ok.";
    else if (status() == FileioStatus::IDLE)
        return "[FileIO stat log] Idle.";
    else if (status() == FileioStatus::FAIL)
        return "[FileIO stat log] Failed!";
    else if (status() == FileioStatus::NO_FILE_FOUND)
        return "[FileIO stat log] File isn't found!";
    else if (status() == FileioStatus::NO_TARGET_FOUND)
        return "[FileIO stat log] Target isn't found!";
    else if (status() == FileioStatus::WRONG_FORMAT)
        return "[FileIO stat log] FileIO format is wrong! Please check.";
    else
        return "[FileIO stat log] None.";
}

void file_io::FileIO::clear()
{
    // Clean the status
    status_ = FileioStatus::IDLE;
    error_cnt_ = 0;
}

void file_io::FileIO::close()
{
    if (format_ == FileioFormat::INI) {
        inim_->Close();
        //inim_.reset();    //FIXME: No memory leaking, but need to check.
    }

    if (this->fs_) {
        if (fs_->is_open())
            fs_->close();
        fs_.release();
    }
}

void file_io::FileIO::Write(const std::string &content)
{
    if (!IsFileOpened())
        return;
    if (format_ == FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return;
    }


    try {
        *fs_ << content;
        *fs_ << std::endl;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }

    status_ = FileioStatus::OK;
}

void file_io::FileIO::Write(const std::string &key, const std::string &content)
{
    Write(key, "=", content);
}

void file_io::FileIO::Write(const std::string &key, const std::string &delimiter, const std::string &content)
{
    if (!IsFileOpened())
        return;
    if (format_ == FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return;
    }

    try {
        *fs_ << key;
        *fs_ << delimiter;
        *fs_ << content;
        *fs_ << std::endl;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }

    status_ = FileioStatus::OK;
}

void file_io::FileIO::Replace(const std::string &old_str, const std::string &new_str)
{
    if (!IsFileOpened())
        return;
    if (format_ != FileioFormat::RW) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return;
    }

    try {
        InitPos();  // Go to start of file position

        // Read file
        std::string contents = "\0";
        for (char ch; fs_->get(ch); contents.push_back(ch)) {}

        auto pos = contents.find(old_str);
        if (pos == std::string::npos) {
            // if cannot find target string, return.
            status_ = FileioStatus::NO_TARGET_FOUND;
            error_cnt_++;
            InitPos();  // Go to start of file position
            return;
        }

        while (pos != std::string::npos) {
            contents.replace(pos, old_str.length(), new_str);
            // Continue searching from here.
            pos = contents.find(old_str, pos);
        }

        InitPos();  // Go to start of file position

        // write back to file
        fs_->close();
        fs_->open(file_path_, std::fstream::in | std::fstream::out | std::fstream::trunc);
        *fs_ << contents;

        status_ = FileioStatus::OK;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }
}

void file_io::FileIO::ReplaceByKey(const std::string &key, const std::string &content)
{
    ReplaceByKey(key, "=", content);
}

void file_io::FileIO::ReplaceByKey(const std::string &key, const std::string &delimiter, const std::string &content)
{
    if (format_ != FileioFormat::RW) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return;
    }

    if (!IsFileOpened())
        return;

    try {
        InitPos();  // Go to start of file position

        // Read file
        std::string contents = "\0";
        for (char ch; fs_->get(ch); contents.push_back(ch)) {}

        std::string key_del = key + delimiter;
        auto pos = contents.find(key_del);
        if (pos == std::string::npos) {
            // if cannot find target string, return.
            status_ = FileioStatus::NO_TARGET_FOUND;
            error_cnt_++;
            InitPos();  // Go to start of file position
            return;
        }

        while (pos != std::string::npos) {
            auto pos_newline = contents.find('\n', pos + key_del.length());
            contents.replace(pos + key_del.length(), pos_newline - (pos + key_del.length()), content);
            // Continue searching from here.
            pos = contents.find(key_del, pos_newline);
        }

        InitPos();  // Go to start of file position

        // write back to file
        fs_->close();
        fs_->open(file_path_, std::fstream::in | std::fstream::out | std::fstream::trunc);
        *fs_ << contents;

        status_ = FileioStatus::OK;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }
}

std::string file_io::FileIO::ReadAll()
{
    if (format_ == FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return "\0";
    }

    if (!IsFileOpened())
        return "\0";

    std::stringstream ss;
    try {
        InitPos();  // Go to start of file position

        ss << fs_->rdbuf();

        InitPos();  // Go to start of file position
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return "\0";
    }
    status_ = FileioStatus::OK;

    return ss.str();
}

std::string file_io::FileIO::Read(const std::string &key)
{
    return Read(key, "=", "\0");
}

std::string file_io::FileIO::Read(const std::string &key, const std::string &delimiter, const std::string &default_val)
{
    if (!IsFileOpened())
        return default_val;
    if (format_ == FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
        return default_val;
    }

    std::string contents;
    try {
        std::string str_line;
        contents.clear();
        contents = "\0";

        InitPos();  // Go to start of file position

        while (std::getline(*fs_, str_line)) {
            size_t pos = 0;

            if (str_line.find(key) != std::string::npos) {
                if ((pos = str_line.find(delimiter)) != std::string::npos) {
                    contents = str_line.substr(pos + delimiter.length(), str_line.length());
                }
                break;
            }
        }

        InitPos();  // Go to start of file position

        if (contents.empty()) {
            status_ = FileioStatus::NO_TARGET_FOUND;
            error_cnt_++;
            return default_val;
        }

        status_ = FileioStatus::OK;
        return contents;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return default_val;
    }
}

void file_io::FileIO::IniSetSection(const std::string &section)
{
    if (format_ != FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Format wrong!";
#endif
        return;
    }

    try {
        inim_->SetSection(section);
        status_ = FileioStatus::OK;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }
}

std::string file_io::FileIO::IniRead(const std::string &section, const std::string &key, const std::string &default_val)
{
    if (format_ != FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Format wrong!";
#endif
        return default_val;
    }

    try {
        std::string output = inim_->Read(section, key);

        if (output == "\0") {
            status_ = FileioStatus::NO_TARGET_FOUND;
            error_cnt_++;
        } else {
            status_ = FileioStatus::OK;
        }
        return output;

    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
        return default_val;
    }
}

void file_io::FileIO::IniInsert(const std::string &section, const std::string &key, const std::string &value)
{
    if (format_ != FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Format wrong!";
#endif
        return;
    }

    try {
        inim_->Insert(section, key, value);

        status_ = FileioStatus::OK;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }
}

void file_io::FileIO::IniSave()
{
    if (format_ != FileioFormat::INI) {
        status_ = FileioStatus::WRONG_FORMAT;
        error_cnt_++;
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Format wrong!";
#endif
        return;
    }

    try {
        inim_->Save();

        status_ = FileioStatus::OK;
    } catch (std::exception e) {
#ifdef DEBUG_FIO
        std::cerr << "[FileIO Error] Get error: " << e.what() << std::endl;
#endif
        status_ = FileioStatus::FAIL;
        error_cnt_++;
    }
}

bool file_io::FileIO::isEmpty() const
{
    if (fs_->peek() == std::ifstream::traits_type::eof())
        return true;
    return false;
}

bool file_io::FileIO::IsFileOpened()
{
    if (!fs_->is_open()) {
        status_ = FileioStatus::FAIL;
        error_cnt_++;
#ifdef DEBUG_FIO
        if (file_path_ != "\0")
            std::cerr << "[FileIO Error] File: "<< file_path_ << " isn't opened." << std::endl;
        else
            std::cerr << "[FileIO Warning] File empty." << std::endl;
#endif
        return false;
    }
    return true;
}

void file_io::FileIO::InitPos()
{
    // Go back to the beginning to keep searching with other keywords
    fs_->clear();
    fs_->seekg(0, std::ios::beg);
}
