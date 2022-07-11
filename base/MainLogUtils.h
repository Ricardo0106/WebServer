#pragma once
#include <functional>
#include <string>
#include <vector>
#include "CountDownLatch.h"
#include "LogStream.h"
#include "MutexLock.h"
#include "Thread.h"
#include "noncopyable.h"

//The client has two buffers and the server has two buffers. 
//The client is responsible for filling the log into the buffer, and the server writes the log information in the buffer to the log file.
// buffers_ is to be written to the file by the server
//currentBuffer_ nextBuffer_ is the buffer input by the client, the server has two clients, the client is full, and the server calls over.

class AsyncLogging : noncopyable {
 public:
  AsyncLogging(const std::string basename, int flushInterval = 2);
  ~AsyncLogging() {
    if (running_) stop();
  }
  void append(const char* logline, int len);

  void start() {
    running_ = true;
    thread_.start();
    latch_.wait();  //只要log线程的回调函数开始调用了，count就变成0了，AsyncLogging中初始化的latch中的count为1
  }

  void stop() {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:
  void threadFunc();
  typedef FixedBuffer<kLargeBuffer> Buffer;
  typedef std::vector<std::shared_ptr<Buffer>> BufferVector;
  typedef std::shared_ptr<Buffer> BufferPtr;
  const int flushInterval_;
  bool running_;
  std::string basename_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;
  BufferPtr currentBuffer_;
  BufferPtr nextBuffer_;
  BufferVector buffers_;
  CountDownLatch latch_;
};