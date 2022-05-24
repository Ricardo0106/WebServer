/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#ifndef LOGFILE
#define LOGFILE
#include <memory>
#include <string>
#include "AppendFile.h"
#include "MutexLock.h"
#include "noncopyable.h"


class LogFile : noncopyable {
 public:
  //Every time it is append flushEveryN times, flush it, it will write to the file, but the file is also buffered
  LogFile(const std::string& basename, int flushEveryN = 1024);
  ~LogFile();

  void append(const char* logline, int len);
  void flush();
  bool rollFile();

 private:
  void append_unlocked(const char* logline, int len);

  const std::string basename_;
  const int flushEveryN_;

  int count_;
  std::unique_ptr<MutexLock> mutex_;
  std::unique_ptr<AppendFile> file_;
};

#endif