/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#pragma once
#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>     

#include <sys/epoll.h>
#include <unistd.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include "Timer.h"


#include "base/SqlconnRAII.h"
#include "base/TcpBuffer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"


class EventLoop;
class TimerNode;
class Channel;

enum ConnectionState { H_CONNECTED = 0, H_DISCONNECTING, H_DISCONNECTED };

class HttpConn  : public std::enable_shared_from_this<HttpConn> {
public:
  HttpConn(EventLoop* loop, int connfd);
  ~HttpConn();

  void init(int sockFd, const sockaddr_in &addr);

  ssize_t read(int *saveErrno);

  ssize_t write(int *saveErrno);

  void seperateTimer();

  void Close();

  int getFd() const;

  void OnProcess();

  int GetPort() const;

  const char *GetIP() const;

  sockaddr_in GetAddr() const;

  bool process_init();
  bool process_();

  void reset();

  int ToWriteBytes()
  {
      return iov_[0].iov_len + iov_[1].iov_len;
  }

  std::shared_ptr<Channel> getChannel() { return channel_; }

  EventLoop *getLoop() { return loop_; }

  void linkTimer(std::shared_ptr<TimerNode> mtimer) {
    // shared_ptr重载了bool, 但weak_ptr没有
    timer_ = mtimer;
  }
  void setError() {
    error_ = true;
  }
  static bool isET;
  static const char *srcDir;
  void handleClose();
  void newEvent();
 private:

  ConnectionState connectionState_;
  EventLoop *loop_;
  std::shared_ptr<Channel> channel_;
  static const char* sources_;
  int fd_;
  bool error_;
  bool isClose_;
  bool keepLive_;
  std::string fileName_;
  std::string path_;
  int nowReadPos_;
  std::map<std::string, std::string> headers_;
  std::weak_ptr<TimerNode> timer_; //这里是weakptr一点TimerNode被删除了，即使这个对象还把持着也没有用

  void handleRead();
  void handleWrite();
  void handleConn();
  void responseForBad();


  void handleError();
  int iovCnt_;
  struct iovec iov_[2];
  Buffer readBuff_;  // 读缓冲区
  Buffer writeBuff_; // 写缓冲区

  HttpRequest request_;
  HttpResponse response_;
};
