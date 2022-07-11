/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include <getopt.h>
#include <string>
#include "EventLoop.h"
#include "Server.h"
#include "base/Logging.h"


int main(int argc, char *argv[]) {
  int threadNum = 1;
  int port = 5123;
  std::string logPath = "./0526.log";
  Logger::setLogFileName(logPath); //整个程序只在这里设置了一次LogFilename
// STL库在多线程上应用
#ifndef _PTHREADS
  LOG << "_PTHREADS is not defined !";
#endif
  EventLoop mainLoop;
  Server myHTTPServer(
    &mainLoop, threadNum, port,  /* MainEventLoop ServerPort ThreadsNum */
    3306, "debian-sys-maint", "Hw4y56hdH4mdPfop", "yin_webserver", 8);/* Mysql_Config SQL_Pool_Num */
  myHTTPServer.start();
  mainLoop.loop();
  return 0;
}
