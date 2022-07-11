#pragma once
#include <stdint.h>

namespace CurrentThread {
// internal
extern __thread int t_cachedTid;
extern __thread char t_tidString[32];
extern __thread int t_tidStringLength;
extern __thread const char* t_threadName;
/**
 *  __thread 只能用于修饰POD类型，不能修饰class类型，因为无法自动调用构造和析构函数。
 *  所有标量类型(基本类型和指针类型)、POD结构类型、POD联合类型、以及这几种类型的数组、const/volatile修饰的版本都是POD类型。
 *  __thread可以用于修饰全局变量，函数内的静态变量，到那时不能用于修饰函数的局部变量或者class的普通成员变量
 *  __thread变量的初始化只能用于编译器常量
 *  __thread变量是每个线程有一份独立的实体，各个线程的变量值互不干扰。
 *  __thread还可以修饰那些值可能会变，带有全局性，但又不值得用全局锁保护的变量
**/
void cacheTid();
inline int tid() {
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}

inline const char* tidString()  // for logging
{
  return t_tidString;
}

inline int tidStringLength()  // for logging
{
  return t_tidStringLength;
}

inline const char* name() { return t_threadName; }
}
