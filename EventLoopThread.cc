/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "EventLoopThread.h"
#include <functional>

EventLoopThread::EventLoopThread()
    : loop_(NULL),
      exiting_(false),
      //先生成一个thread对象，但是此时并没有生成一个线程，是在start()的时候才生成线程
      thread_(bind(&EventLoopThread::threadFunc, this), "EventLoopThread"),//thread是start()的时候真正构造好
      mutex_(),
      cond_(mutex_) {} //这个cond持有刚构造好的mutex

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != NULL) {
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() {
  assert(!thread_.started());
  thread_.start();
  {
    MutexLockGuard lock(mutex_);
    // 一直等到threadFun在Thread里真正跑起来
    //保证了退出一定是再thread_.start()真正执行完之后
    while (loop_ == NULL) cond_.wait();//直到threadFunc被调用，loop_才会指向真实地址，这时候才可以返回
  }
  return loop_;
}

//EventLoopThread来调用EventLoop中的loop
//构造EventLoopThread的时候不构造相应的loop_
//而是再构造好的线程运行回调函数的时候构造相应的loop_ 
void EventLoopThread::threadFunc() {
  EventLoop loop;

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  // assert(exiting_);
  loop_ = NULL;
}
