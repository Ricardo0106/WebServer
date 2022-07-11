#include "MainLogUtils.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <functional>
#include "LogFile.h"

AsyncLogging::AsyncLogging(std::string logFileName_, int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(logFileName_),
      //log 线程
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"), // 对线程进行初始化，调用函数（往文件里写）
      mutex_(),
      cond_(mutex_),
      //下面两个buffer是log线程要用的，也就是后端线程的两个buffer
      currentBuffer_(new Buffer),  
      nextBuffer_(new Buffer),
      buffers_(),
      latch_(1) { //这里count初始为1说明，只要有一个countdown就可以开始执行了
  assert(logFileName_.size() > 1);
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

//客户端通过调用这个append来往客户端的currentBuffer中写日志
//这个append调用LogStream中的那个buffer类的append往缓冲区写(也就是客户端调用)
void AsyncLogging::append(const char* logline, int len) {
  MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
    currentBuffer_->append(logline, len);
  else {
    buffers_.push_back(currentBuffer_);
    currentBuffer_.reset();
    if (nextBuffer_) //如果nextbuffer_有，（自己的俩没写满，或者是服务端给调过来了） 
      currentBuffer_ = std::move(nextBuffer_);
    else       //否则新建一个buffer临时用来写
      currentBuffer_.reset(new Buffer);
    currentBuffer_->append(logline, len);
    cond_.notify();
  }
}

//log线程的回调函数，通过这个来往日志文件中写
//log线程中的append是调用的我们封装logfile那个类中的append，也就是往文件中append
void AsyncLogging::threadFunc() {
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_); // 要写入的文件
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_) {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!   //buffers_没有被客户端写入呢
      {
        cond_.waitForSeconds(flushInterval_); //没有客户端写入，就等待几秒
      }
      buffers_.push_back(currentBuffer_);
      currentBuffer_.reset();

      currentBuffer_ = std::move(newBuffer1);  //把服务端newBuffer1移动给客户端，当作currentBuffer 
      /**客户端的两个buffer是 currentBuffer，nextBuffer 服务的两个是newBuffer1，newBuffer2**/
      buffersToWrite.swap(buffers_);  //关键代码，交换客户端和服务端的buffer（这个buffer不是那四个buffer A B C D
      //而是需要服务端写入文件的那个，和客户端写完跟服务端交换的。[A B] [ ] 
      if (!nextBuffer_) {
        nextBuffer_ = std::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());

    if (buffersToWrite.size() > 25) { //要写入的太多了,丢弃
      buffersToWrite.erase(buffersToWrite.begin() + 2, buffersToWrite.end());
    }

    for (size_t i = 0; i < buffersToWrite.size(); ++i) {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      //使用我们封装的filelog中的append
      output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());//往文件里写
      //buffersToWrite是刚才我们从客户端换过来的currentBuffer和nextBuffer
      //现在currentBuffer和nextBuffer已经是newbuffer1和newbuffer2了
      //buffersToWrite[i] is a sharedptr of Buffer   buffersToWrite[*p_A, *p_B]
      //                                                             A     B
    }

    if (buffersToWrite.size() > 2) {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }
    //如果没有newBuffer1  也就是刚才log线程的后端把自己newBuffer1换出去了,换给了客户端的currentBuffer
    if (!newBuffer1) {    
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.back();  //buffersToWrite后面的buffer给到newBuffer1
      buffersToWrite.pop_back();
      newBuffer1->reset();        //清空newBuffer1中的内容
    }

    //如果没有newBuffer2  也就是刚才log线程的后端把自己newBuffer2换出去了,换给了客户端的nextBuffer
    if (!newBuffer2) {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.back();
      buffersToWrite.pop_back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    //完成一次对文件的写 flush()一下
    //同时每append K次也会自动的flush一下
    output.flush();
  }
  output.flush();
}
