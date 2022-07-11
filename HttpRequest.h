/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql
#include <memory.h>

#include "base/TcpBuffer.h"
#include "base/Logging.h"
#include "base/Sqlconnpool.h"
#include "base/SqlconnRAII.h"

class HttpConn;

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void deInit();
    void Init();
    void Init(std::shared_ptr<HttpConn> conn_);;
    bool parse(Buffer& buff);
    void setError();
    bool isInited();
    
    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    PARSE_STATE getState() const { return state_; }
    bool IsKeepAlive() const;
    ssize_t getContentLen() const;

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */


private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    bool inited_;
    int clientFd_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;
    std::weak_ptr<HttpConn> wp_conn;
    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static const regex parseRequestLinePattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    static const regex parseHeaderPattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    static int ConverHex(char ch);
};

