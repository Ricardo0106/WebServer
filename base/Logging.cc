#include "Logging.h"
#include "CurrentThread.h"
#include "Thread.h"
#include "MainLogUtils.h"
#include <assert.h>
#include <iostream>
#include <time.h>  
#include <sys/time.h> 


static pthread_once_t once_control_ = PTHREAD_ONCE_INIT;
static AsyncLogging *AsyncLogger_;

std::string Logger::logFileName_ = "./WebServer.log";
void once_init()
{
    AsyncLogger_ = new AsyncLogging(Logger::getLogFileName());
    AsyncLogger_->start(); //此时Log线程启动也就是 AsyncLogging的回调函数才开始运行，
}

//output会被多次调用，但是这个log线程只会初始化一次
//所以append每次调用output的时候都会调用，但是线程只会初始一次
void output(const char* msg, int len)
{
    //本函数使用初值为PTHREAD_ONCE_INIT的once_control变量保证once_init()函数在本进程执行序列中仅执行一次
    pthread_once(&once_control_, once_init);
    //客户端通过调用这个append来往客户端的currentBuffer中写日志
    AsyncLogger_->append(msg, len);
}

Logger::Impl::Impl(const char *fileName, int line)
  : stream_(),
    line_(line),
    basename_(fileName)
{
    formatTime(); //构造Impl的时候就会打印时间
}

void Logger::Impl::formatTime()  //日志打印 日期时间
{
    struct timeval tv;
    time_t time;
    char str_t[26] = {0};
    gettimeofday (&tv, NULL);
    time = tv.tv_sec;
    struct tm* p_time = localtime(&time);   
    strftime(str_t, 26, "\n%Y-%m-%d %H:%M:%S\n", p_time);
    //size_t strftime(char *str, size_t maxsize, const char *format, const struct tm *timeptr) 
    //根据 format 中定义的格式化规则，格式化结构 timeptr 表示的时间，并把它存储在 str 中
    stream_ << str_t; 
}
//每次__LOG__的时候都会自动构造一个Logger
Logger::Logger(const char *fileName, int line)
  : impl_(fileName, line)
{ }

//析构的时候才会写log?
Logger::~Logger()
{
    impl_.stream_ << " >>>> " << impl_.basename_ << ':' << impl_.line_ << '\n';
    const LogStream::Buffer& buf(stream().buffer());
    //LogStream中的行buffer(kSmallBuffer)
    output(buf.data(), buf.length());
}