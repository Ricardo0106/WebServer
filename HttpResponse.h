/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#pragma once
#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap
#include <memory>
#include "base/TcpBuffer.h"
#include "base/Logging.h"

class HttpConn;

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(std::shared_ptr<HttpConn> sp_hc, const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const { return code_; }
    bool isInited() { return inited_; }

private:
    void AddStateLine_(Buffer &buff);
    void AddHeader_(Buffer &buff);
    void AddContent_(Buffer &buff);

    void ErrorHtml_();
    std::string GetFileType_();
    void setError();

    int code_;
    bool isKeepAlive_;
    bool inited_;

    std::string path_;
    std::string srcDir_;
    std::weak_ptr<HttpConn> wp_conn;

    char* mmFile_; 
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};

