#pragma once
#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

// CountDownLatch的主要作用是确保Thread中传进去的func真的启动了以后
// 外层的start才返回
// 使一个线程等待其他线程各自执行完毕后再执行
// CountDownLatch构造函数会以线程的数量为初始值

// void wait();
// 调用wait方法的线程会被挂起，它会等待直到count_值为0才继续执行

// void countDown();
// 当一个线程执行完毕后，计数器的值就-1，当计数器的值为0时，表示所有线程都执行完毕，然后在闭锁上等待的线程就可以恢复工作了

// int getCount() const;
// 获取计数
class CountDownLatch : noncopyable {
 public:
  explicit CountDownLatch(int count);
  void wait();
  void countDown();

 private:
  mutable MutexLock mutex_;
  Condition condition_;
  int count_;
};
/**
 * CountDownLatch是一个同步辅助类，在完成一组正在其他线程中执行的操作之前，它允许一个或多个线程一直等待。
 * 假设我们周末要去旅游，出游前需要提前订好机票、巴士和酒店，都订好后就可以出发了，这个用代码怎么实现？可以用CountDownLatch。
 * 主线程发起多个子线程，等这些子线程各自都完成一定的任务之后，主线程才继续执行。通常用于主线程等待多个子线程完成初始化。
 * 主线程发起多个子线程，子线程都等待主线程，主线程完成其他一些任务之后通知所有子线程开始执行。
 * 通常用于多个子线程等待主线程发出“起跑”命令。
**/
