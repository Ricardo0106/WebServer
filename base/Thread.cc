/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "Thread.h"
#include <assert.h>
#include <errno.h>
#include <linux/unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory>
#include "CurrentThread.h"


#include <iostream>
using namespace std;

namespace CurrentThread {
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "default";
}

pid_t gettid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

void CurrentThread::cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = gettid();//给当前的线程赋予id
    t_tidStringLength =
        snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
  }
}

// 为了在线程中保留name,tid这些数据
struct ThreadData {
  typedef Thread::ThreadFunc ThreadFunc;
  ThreadFunc func_;  //当前线程的回调函数 
  string name_;      
  pid_t* tid_;
  CountDownLatch* latch_;

  ThreadData(const ThreadFunc& func, const string& name, pid_t* tid,
             CountDownLatch* latch)
      : func_(func), name_(name), tid_(tid), latch_(latch) {}

  void runInThread() {
    *tid_ = CurrentThread::tid();
    tid_ = NULL;
    latch_->countDown();
    latch_ = NULL;

    CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
    prctl(PR_SET_NAME, CurrentThread::t_threadName);//prtcl设置进程名

    func_();
    CurrentThread::t_threadName = "finished";
  }
};

void* startThread(void* obj) {  //创建线程时的回调函数
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();
  delete data;
  return NULL;
}

Thread::Thread(const ThreadFunc& func, const string& n)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      tid_(0),
      func_(func),
      name_(n),
      latch_(1) {
  setDefaultName();
}

Thread::~Thread() {
  if (started_ && !joined_) pthread_detach(pthreadId_);
}

void Thread::setDefaultName() {
  if (name_.empty()) {
    char buf[32];
    snprintf(buf, sizeof buf, "Thread");
    name_ = buf;
  }
}

void Thread::start() {
  assert(!started_);
  started_ = true;
  // 当前thread构造的时候func_,name_,tid_,latch_就确定了
  //这里在利用的thread中的属性，抽象出来一个threaddata，之后thread的回调其实都是通过这个threaddata
  ThreadData* data = new ThreadData(func_, name_, &tid_, &latch_);
  if (pthread_create(&pthreadId_, NULL, &startThread, data)) { //pthread_creat成功时返回0失败时返回错误码
    started_ = false;
    delete data;
  } else { 
    latch_.wait();  //确保线程已经开始运行了,因为在线程创建完完之后回调的时候
    //在runInThread中会countdown，这时候保证线程已经在运行了,这样这里wait()之后不会陷入循环，也就是说这里可以退出了
    //保证了thread在运行之后，start才算退出
    assert(tid_ > 0);
  }
}

int Thread::join() {
  assert(started_);
  assert(!joined_);
  joined_ = true;
  return pthread_join(pthreadId_, NULL);
}