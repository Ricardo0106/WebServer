#pragma once
#include <assert.h>
#include <string.h>
#include <string>
#include "noncopyable.h"

class AsyncLogging;
const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

//AsyncLogging中要用的buffer 要用四个
template <int SIZE>
class FixedBuffer : noncopyable {
 public:
  FixedBuffer() : cur_(data_) {}

  ~FixedBuffer() {}
  //往buf里面添加东西，由LogStream中的<<调用
  void append(const char* buf, size_t len) {
    if (avail() > static_cast<int>(len)) {
      memcpy(cur_, buf, len);
      //void *memcpy(void *destin, void *source, unsigned n);
      //destin-- 指向用于存储复制内容的目标数组，类型强制转换为 void* 指针。
      //source-- 指向要复制的数据源，类型强制转换为 void* 指针。
      //n-- 要被复制的字节数。
      //char占1字节 short 2   int 4 = 32bit 32位
      cur_ += len;
    }
  }

  const char* data() const { return data_; }
  int length() const { return static_cast<int>(cur_ - data_); }

  char* current() { return cur_; }
  int avail() const { return static_cast<int>(end() - cur_); }
  void add(size_t len) { cur_ += len; }

  void reset() { cur_ = data_; }
  void bzero() { memset(data_, 0, sizeof data_); }

 private:
  const char* end() const { return data_ + sizeof data_; }

  char data_[SIZE];
  char* cur_;
};

class LogStream : noncopyable {
 public:
  //LogStream主要用来格式化输出，重载了<<运算符，同时也有自己的一块缓冲区，这里缓冲区的存在是为了缓存一行，把多个<<的结果连成一块
  //下面这个缓冲区就是为了缓存一行
  typedef FixedBuffer<kSmallBuffer> Buffer; //这里是ksamllbuffer
  //FixedBuffer中是data_[kSamllBuffer]

  LogStream& operator<<(bool v) {
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  LogStream& operator<<(short);
  LogStream& operator<<(unsigned short);
  LogStream& operator<<(int);
  LogStream& operator<<(unsigned int);
  LogStream& operator<<(long);
  LogStream& operator<<(unsigned long);
  LogStream& operator<<(long long);
  LogStream& operator<<(unsigned long long);

  LogStream& operator<<(const void*);

  LogStream& operator<<(float v) {
    *this << static_cast<double>(v);
    return *this;
  }
  LogStream& operator<<(double);
  LogStream& operator<<(long double);

  LogStream& operator<<(char v) {
    buffer_.append(&v, 1);
    return *this;
  }

  LogStream& operator<<(const char* str) {
    if (str)
      buffer_.append(str, strlen(str)); // strlen(str) c string not cpp string
    else
      buffer_.append("(null)", 6);
    return *this;
  }

  LogStream& operator<<(const unsigned char* str) {
    return operator<<(reinterpret_cast<const char*>(str));
  }

  LogStream& operator<<(const std::string& v) {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

  void append(const char* data, int len) { buffer_.append(data, len); }
  const Buffer& buffer() const { return buffer_; }
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template <typename T>
  void formatInteger(T);

  Buffer buffer_;

  static const int kMaxNumericSize = 32;
};