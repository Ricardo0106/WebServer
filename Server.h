/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#pragma once
#include <memory>
#include "Channel.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"

#include "base/sqlconnpool.h"
#include "base/sqlconnRAII.h"
class Server {
 public:
  Server(
        EventLoop *loop, int threadNum, int port, 
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum);
  ~Server();
  EventLoop *getLoop() const { return loop_; }
  void start();
  void handNewConn();
  void handThisConn() { loop_->updatePoller(acceptChannel_); } //
 private:
  EventLoop *loop_;  //baseloop
  int threadNum_;
  std::unique_ptr<EventLoopThreadPool> eventLoopThreadPool_; //server类独占一个线程池，一个webserver只有一个线程池
  bool started_;
  std::shared_ptr<Channel> acceptChannel_;  //接受连接的channel
  int port_;
  int listenFd_;
  static const int MAXFDS = 100000;
  string s;
  char* srcDir_;
};
