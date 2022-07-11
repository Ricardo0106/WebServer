/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "HttpConn.h"
#include "base/Logging.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include "Channel.h"
#include "EventLoop.h"
#include "Util.h"
#include "time.h"

using namespace std;


const __uint32_t DEFAULT_EVENT = EPOLLIN | EPOLLET | EPOLLONESHOT;
const char* HttpConn::sources_ = "/home/yin/WebServer_master/build/Debug/WebServer/resources/";
const int DEFAULT_EXPIRED_TIME = 2000;              // ms
const int DEFAULT_KEEP_ALIVE_TIME = 5 * 60 * 1000;  // ms
// const int DEFAULT_EXPIRED_TIME = 1000 * 60 * 30;              // ms
// const int DEFAULT_KEEP_ALIVE_TIME = 30 * 60 * 1000;  // ms
const char* HttpConn::srcDir;
bool HttpConn::isET;
HttpConn::HttpConn(EventLoop* loop, int connfd) 
    : loop_(loop),
      channel_(new Channel(loop, connfd)), 
      fd_(connfd), 
      error_(false), 
      connectionState_(H_CONNECTED),
      isClose_(false), 
      keepLive_(false) { 
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    channel_->setReadHandler(bind(&HttpConn::handleRead, this));
    channel_->setWriteHandler(bind(&HttpConn::handleWrite, this));
    channel_->setConnHandler(bind(&HttpConn::handleConn, this));
};

HttpConn::~HttpConn() { 
    close(fd_);
};

void HttpConn::seperateTimer() {
  // cout << "seperateTimer" << endl;
  if (timer_.lock()) {
    shared_ptr<TimerNode> my_timer(timer_.lock());
    my_timer->clearReq();
    timer_.reset();
  }
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);
    return len;
}
//每次在网页都要第二次请求才能收到
void HttpConn::handleRead() {
    __uint32_t &events_ = channel_->getEvents();
    bool process_state = false;
    LOG << "read";
    do {
        int readErrno = 0;
        int ret = read(&readErrno);
        LOG << "ret:" << ret;
        if (connectionState_ == H_DISCONNECTING) {
            readBuff_.RetrieveAll();
            break;
        }
        if (ret < 0)
        {
            error_ = true;
            connectionState_ = H_DISCONNECTING;
            LOG << "read ret < 0 handleError!";
            handleError();
            return ;
        } else if (ret == 0) {
            connectionState_ = H_DISCONNECTING;
            return ;
        }
        process_state = process_init();
    } while (false);

    //之前客户端一直收不到请求错误时候的回复，就是因为，error_==true的时候，下面process_state==true时候的handleWrite不走
    if (process_state && error_) {
        responseForBad();
    }

    if (!error_) {
        if (process_state) {
            handleWrite();
        }
        if (request_.getState() == HttpRequest::FINISH) {
            this->reset();
            if (readBuff_.ReadableBytes() > 0) {//一次肯定能读完到buffer中
            //post请求body没加\r\n导致没读完，又读了一次，然后出了错误,其实是body是没有\r\n的，需要通过contentlen来判断
                LOG << "read agin anymore! readableBytes:" << readBuff_.ReadableBytes();
                if (connectionState_ != H_DISCONNECTING) {
                    handleRead();
                }
            }
        } else if (connectionState_ != H_DISCONNECTED) {
            events_ |= EPOLLIN;
        }
    }
}

void HttpConn::responseForBad() {
    if (connectionState_ != H_DISCONNECTED) {
        __uint32_t &events_ = channel_->getEvents();
        LOG << "responsefor bad";
        int writeErrno = 0;
        int ret = -1;
        ret = write(&writeErrno);
        
        if (ret <= 0) {
            LOG << "Anser bad for bad things! ";
        }
    }
}

void HttpConn::handleWrite()
{
    if (!error_ && connectionState_ != H_DISCONNECTED)
    {
        __uint32_t &events_ = channel_->getEvents();
        int writeErrno = 0;
        int ret = -1;
        ret = write(&writeErrno);
        if (ret < 0) {
            events_ = 0;
            error_ = true;
        }
        if (ToWriteBytes() > 0 || writeBuff_.ReadableBytes() > 0) {
            events_ |= EPOLLOUT;
        }
        // if (ToWriteBytes() == 0) {()
        //     if (keepLive_) {
        //         if (readBuff_.ReadableBytes() <= 0) {
        //             events_ |= EPOLLIN;
        //         } else if (readBuff_.ReadableBytes() > 0) {
        //             process_init();
        //             events_ |= EPOLLOUT;
        //             // handleWrite();
        //         }
        //         return ;
        //     }
        // }
        // else if (ret < 0)
        // {
        //     events_ = 0;
        //     error_ = true;
        //     perror("writen");
        //     return ;
        // }
        // connectionState_ = H_DISCONNECTING;
        // if (writeBuff_.ReadableBytes() > 0) {
        //     events_ |= EPOLLOUT;
        // }
        // if (writeBuff_.ReadableBytes() > 0)
        //     events_ |= EPOLLOUT;
    }
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        // LOG << "writev len"  << len;
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

void HttpConn::handleError() {
    __uint32_t& events_ = channel_->getEvents();
    string path = "/error.html";
    response_.Init(shared_from_this(), sources_, path, false, 400);
    process_();
    responseForBad();
}

bool HttpConn::process_init() {
    if (!request_.isInited())
        request_.Init(shared_from_this());
    
    if(readBuff_.ReadableBytes() <= 0) {
        return false;
    }
    else if(request_.parse(readBuff_)) {
        keepLive_ = request_.IsKeepAlive();
        if (readBuff_.ReadableBytes() == 0)
            response_.Init(shared_from_this(), srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(shared_from_this(), srcDir, request_.path(), false, 400);
    }
    return process_();
}

bool HttpConn::process_() {
    if (!response_.isInited()) return false;
    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;
    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    return true;
}

void HttpConn::handleConn() {
  seperateTimer();
  __uint32_t &events_ = channel_->getEvents();
  if (!error_ && connectionState_ == H_CONNECTED) {
    if (events_ != 0) {
      int timeout = DEFAULT_EXPIRED_TIME;
      if (keepLive_) timeout = DEFAULT_KEEP_ALIVE_TIME;
      if ((events_ & EPOLLIN) && (events_ & EPOLLOUT)) {
        events_ = __uint32_t(0);
        events_ |= EPOLLOUT;
      }
      // events_ |= (EPOLLET | EPOLLONESHOT);
      events_ |= EPOLLET;
      loop_->updatePoller(channel_, timeout);
      LOG << "1";
    } else if (keepLive_) {
      events_ |= (EPOLLIN | EPOLLET);
      // events_ |= (EPOLLIN | EPOLLET | EPOLLONESHOT);
      int timeout = DEFAULT_KEEP_ALIVE_TIME;
      loop_->updatePoller(channel_, timeout);
      LOG << "2";
    } else {
      // cout << "close normally" << endl;
      // loop_->shutdown(channel_);
      // loop_->runInLoop(bind(&HttpData::handleClose, shared_from_this()));

      loop_->runInLoop(bind(&HttpConn::handleClose, shared_from_this()));
      return ;

    //   events_ |= (EPOLLIN | EPOLLET);
    //   int timeout = (DEFAULT_KEEP_ALIVE_TIME >> 1);
    //   loop_->updatePoller(channel_, timeout);
    //   LOG << "3";
    }
  } else if (!error_ && connectionState_ == H_DISCONNECTING &&
             (events_ & EPOLLOUT)) {
    LOG << "4";
    events_ = (EPOLLOUT | EPOLLET);//客户端已经关闭写端，服务器还有要写的内容，写完之后就会关闭
  } else {
    LOG << "5";
    loop_->runInLoop(bind(&HttpConn::handleClose, shared_from_this()));
  }
}

void HttpConn::reset() {
  // inBuffer_.clear();
  // keepAlive_ = false;
  request_.deInit();
  if (timer_.lock()) {
    shared_ptr<TimerNode> my_timer(timer_.lock());
    my_timer->clearReq();
    timer_.reset();
  }
}

int HttpConn::getFd() const {
    channel_->getFd();
}
//HttpConn怎么析构啊？？？？？？？？我靠
void HttpConn::handleClose() {
    isClose_ = true;
    LOG << "--------Close connection! fd:" << getFd();
    connectionState_ = H_DISCONNECTED;
    shared_ptr<HttpConn> guard(shared_from_this());//防止造成对象的重复析构
    response_.UnmapFile();
    loop_->removeFromPoller(channel_);
}

void HttpConn::newEvent() {
  channel_->setEvents(DEFAULT_EVENT);
  loop_->addToPoller(channel_, DEFAULT_EXPIRED_TIME);
}
