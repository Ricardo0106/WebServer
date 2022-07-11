#include "LogStream.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;

// From muduo
//反向存储数字再反转
template <typename T>
size_t convert(char buf[], T value) {
  T i = value;
  char* p = buf;

  //先从后往前存，之后在对存储的数进行反转 
  do {
    int lsd = static_cast<int>(i % 10);
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0) {
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

template class FixedBuffer<kSmallBuffer>;
template class FixedBuffer<kLargeBuffer>;

template <typename T>
void LogStream::formatInteger(T v) {
  // buffer容不下kMaxNumericSize个字符的话会被直接丢弃(32)
  if (buffer_.avail() >= kMaxNumericSize) {
    size_t len = convert(buffer_.current(), v);
    buffer_.add(len);
  }
}

LogStream& LogStream::operator<<(short v) {
  *this << static_cast<int>(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned short v) {
  *this << static_cast<unsigned int>(v);
  return *this;
}

LogStream& LogStream::operator<<(int v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned int v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long long v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long long v) {
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(double v) {
  if (buffer_.avail() >= kMaxNumericSize) {
    int len = (buffer_.current(), kMaxNumericSize, "%.12g", v);
    buffer_.add(len);
  }
  return *this;
}

LogStream& LogStream::operator<<(long double v) {
  if (buffer_.avail() >= kMaxNumericSize) {
    int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12Lg", v);
    //超过KMaxNumericSize的大小会被截断
    //buffer_拷贝过后，add(len)来更新cur
    buffer_.add(len);
    // int snprintf(char *str, size_t size, const char *format, ...) 
    //设将可变参数(...)(这里是v)按照 format 格式化成字符串，并将字符串复制到 str 中，size 为要写入的字符的最大数目，超过 size 会被截断。
  }
  return *this;
}