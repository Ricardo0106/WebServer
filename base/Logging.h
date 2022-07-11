#pragma once
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "LogStream.h"


class AsyncLogging;

//Logging is an external interface, and the Logging class contains a LogStream object
//The main purpose is to add fixed formatted information before and after the log each time you log, such as the log line, file name and other information
class Logger {
 public:
  Logger(const char *fileName, int line);
  ~Logger();
  //returns a reference of type LogStream
  LogStream &stream() { return impl_.stream_; }

  static void setLogFileName(std::string fileName) { logFileName_ = fileName; }
  static std::string getLogFileName() { return logFileName_; }

 private:
  //Class for printing standard time and date
  class Impl {
   public:
    Impl(const char *fileName, int line);
    void formatTime();

    LogStream stream_;
    int line_;
    std::string basename_;
  };
  Impl impl_;
  static std::string logFileName_;
};

#define LOG Logger(__FILE__, __LINE__).stream()
/**
ANSI C标准中几个标准预定义宏：
__LINE__：在源代码中插入当前源代码行号；
__FILE__：在源文件中插入当前源文件名；
__DATE__：在源文件中插入当前的编译日期
__TIME__：在源文件中插入当前编译时间；
__STDC__：当要求程序严格遵循ANSI C标准时该标识被赋值为1；
**/