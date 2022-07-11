/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <iostream>
#include "Util.h"
#include "base/Logging.h"

using namespace std;

__thread EventLoop* t_loopInThisThread = 0;

int createEventfd() {
  int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // eventfd()创建了一个“eventfd对象”， 
  //通过它能够实现用户态程序间(我觉得这里主要指线程而非进程)的等待/通知机制，以及内核态向用户态通知的机制
  //通过他唤醒IO线程
  if (evtfd < 0) {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

//每个subReactor都有一个自己的wakeupFd，用来被被唤醒，而主线程拥有listenfd用来监听有无新连接
EventLoop::EventLoop()  
    : looping_(false),
      poller_(new Epoll()),
      wakeupFd_(createEventfd()),//通过wakeupFd这个eventfd对象来唤醒IO线程
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),//这个EventLoop归属于当前线程
      pwakeupChannel_(new Channel(this, wakeupFd_)) {//生成一个新的channel来封装wakeupFd
  if (t_loopInThisThread) {//确保当前线程有 且只有 这一个EventLoop
    // LOG << "Another EventLoop " << t_loopInThisThread << " exists in this
    // thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);
  pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));//设置读回调函数
  pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));//主线程用来处理新的连接
  poller_->epoll_add(pwakeupChannel_, 0);//epoll的红黑树上添加对wakeupFd的对应channel监听
}
//每个IO线程都有一个自己eventloop，每一个eventloop对应一个唤醒他的wakeupFd(被channel包装),wakeupFd是用来唤醒某个eventloop的，所以他对应的channel中的callback在eventloop中设置了，同理channel中httpconn对应callback在httpconn中设置
void EventLoop::handleConn() {
  // poller_->epoll_mod(wakeupFd_, pwakeupChannel_, (EPOLLIN | EPOLLET |
  // EPOLLONESHOT), 0);
  updatePoller(pwakeupChannel_, 0);
}

EventLoop::~EventLoop() {
  // wakeupChannel_->disableAll();
  // wakeupChannel_->remove();
  close(wakeupFd_);
  t_loopInThisThread = NULL;
}

void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof one);
  if (n != sizeof one) {
    LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = readn(wakeupFd_, &one, sizeof one);  
  if (n != sizeof one) {
    LOG << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
  // pwakeupChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  pwakeupChannel_->setEvents(EPOLLIN | EPOLLET);  
}

void EventLoop::runInLoop(Functor&& cb) {
  if (isInLoopThread()) //如果是IO线程
    cb();//立即执行这个callback
  else
    queueInLoop(std::move(cb)); //将这个callback加入到queueInloop中去
}

void EventLoop::queueInLoop(Functor&& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.emplace_back(std::move(cb));
  }
  //如果当前线程不是自己loop所属的线程，就唤醒自己loop所属的io线程
  if (!isInLoopThread() || callingPendingFunctors_) wakeup();//唤醒IO线程
}

void EventLoop::loop() {
  assert(!looping_);
  assert(isInLoopThread());//确保是在IO线程
  looping_ = true;
  quit_ = false;
  // LOG_TRACE << "EventLoop " << this << " start looping";
  std::vector<SP_Channel> ret;
  while (!quit_) {
    // cout << "doing" << endl;
    ret.clear();
    ret = poller_->poll();
    eventHandling_ = true;
    for (auto& it : ret) it->handleEvents();  //有事件发生的channel开始处理事件
    eventHandling_ = false;
    doPendingFunctors();
    poller_->handleExpired();  //每循环一次处理一次超时的事件
  }
  looping_ = false;
}

void EventLoop::doPendingFunctors() { // 执行刚才不是IO线程时候放入函数集的函数
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);//因为pendingFunctors不光在IO线程中，还会暴露给普通线程，所以需要加锁
  }

  for (size_t i = 0; i < functors.size(); ++i) functors[i]();
  callingPendingFunctors_ = false;
}

void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}
