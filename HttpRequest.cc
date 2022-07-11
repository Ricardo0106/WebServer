/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "HttpRequest.h"
#include "HttpConn.h"

using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::deInit() {
    inited_ = false;
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::isInited() {
    return inited_;
}

void HttpRequest::Init(std::shared_ptr<HttpConn> sp_conn_) {
    inited_ = true;
    wp_conn = sp_conn_;
    clientFd_ = sp_conn_->getFd();            
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return (header_.find("Connection")->second == "keep-alive" 
                || header_.find("Connection")->second == "Keep-Alive") && version_ == "1.1";
    }
    return false;
}

ssize_t HttpRequest::getContentLen() const {
    if (header_.find("Content-Length") != header_.end()) {
        return static_cast<ssize_t>(stoi(header_.find("Content-Length")->second));
    }
    return -1;
}

void HttpRequest::setError() {
    if (!wp_conn.expired()) {
        std::shared_ptr<HttpConn> temp_sp(wp_conn.lock());
        temp_sp->setError();
        //temp_sp 离开作用域，自动析构；
        // temp_sp.reset();
    }
}

//post请求总是出错，没有对于conntent-Length的解析和处理，导致如果body没有\r\n结尾就会导致buffer中的body滞留，EpollIN一直触发
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // std::string url(buff.Peek(), buff.BeginWriteConst());
    // LOG << "Request:" << url; //需要拷贝构造一个很大的string，十分浪费性能，后面会用下面的代替
    while(buff.ReadableBytes() && state_ != FINISH) {
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        //还有没有读到的信息
        // if (lineEnd == buff.BeginWriteConst()) {

        // }
        std::string line(buff.Peek(), lineEnd);
        // LOG << "Url line:" << line << "line size" << line.size();
        switch(state_)
        {
        case REQUEST_LINE:
            if(!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;    
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if(lineEnd == buff.BeginWrite()) { 
            if (getContentLen() == ssize_t(buff.BeginWriteConst() - buff.Peek())) {
                buff.RetrieveAll();
            }
            break; 
        }
        buff.RetrieveUntil(lineEnd + 2);
    }
    std::string url_(buff.Peek(), buff.BeginWriteConst());
    // LOG << "url_" << url_;
    LOG << "Parsed Request: Fd=" << clientFd_
        << "\nmethod:" << method_.c_str() << "\nsource path:" << path_.c_str() 
        << "\nVersion:HTTP" << version_.c_str();
    return true;
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    smatch subMatch;
    if(regex_match(line, subMatch, parseRequestLinePattern)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        //method path verson这三个是固定的前三个，之后的是不一定出现，所以用到了map
        state_ = HEADERS;
        return true;
    }
    LOG << "Error ParseRequestLine error!******";
    setError();
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    smatch subMatch;
    if(regex_match(line, subMatch, parseHeaderPattern)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    // LOG << "Body:" << line.c_str();
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    setError();
                    path_ = "/error.html";
                }
            }
        }
    } else {
        setError();
    }  
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }
    // LOG << "Verify name:" << name.c_str() << " password:" << pwd.c_str();
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());

    // LOG << order;

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG << "Mysql Row: " << row[0] << " " << row[1];
        string password(row[1]);
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG << "Password error!******";
            }
        } 
        else { 
            flag = false; 
            LOG << "User used!******";
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG << "regirster!";
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        // LOG << order;
        if(mysql_query(sql, order)) { 
            LOG << "Insert error!******";
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    // LOG << "UserVerify success!";
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}