/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#ifndef TCPBUFFER_H
#define TCPBUFFER_H
#include <cstring>
#include <iostream>
#include <vector>
#include <sys/uio.h> //iovec
#include <unistd.h>
#include <assert.h>


/**
 *   ----------------------------------------------------------
 *  | prependable bytes |  readable bytes  |  writable bytes  |
 *  ----------------------------------------------------------
 *  0              readerIndex        writerIndex           size
 * We think about a question why use the application layer to send buffers,
 * 
 * At this point we should know that what we said about the server sending is completed is actually written to the os buffer, 
 * and the rest can be done by tcp
 * If you want to send 40KB of data, but the TCP send buffer of the operating system has only 25KB of remaining space, 
 * if the remaining 15KB of data has to wait for the TCP buffer to be available again, the current thread will be blocked, 
 * because it does not know when the other party will read it Pick
 * So you can store the 15kb data, store it in the buffer of the application layer of the TCP connection, 
 * and send it when the socket becomes writable
 * Why use the accept buffer?
 * 
 * If the first one we read is not a complete package, then we should temporarily store the read data in readbuffer
 * 
 * Then how big should our buffer be?
 * 
 * If the initial size is relatively small, then if it is not enough, 
 * it needs to be expanded, which will waste a certain amount of time. 
 * If it is larger than 50kb, 10,000 connections will waste 1GB of memory, which is obviously inappropriate.
 * So we use the space buffer on the stack + the buffer of each connection to use with readv
 * 
 * If there is not much read and the buffer is completely enough, it will be read into the buffer. 
 * If it is not enough, it will be read into the upper area of ​​the stackbuffer, 
 * and then the program will append the data in the stackbuf to the buffer.
**/
class TcpBuffer {
 public:
  TcpBuffer(int bufferSize = 1024);
  ~TcpBuffer();

  size_t getWritableBytes() const;
  size_t getReadableBytes() const;
  size_t getPrependableBytes() const;

  const char* peek() const;
  void ensureWritable(size_t len);
  void hasWritten(size_t len);

  void retrieve(size_t len);
  void retrieveUntil(const char* end);

  void RetrieveAll();
  std::string RetrieveAllToStr();

  const char* beginWriteConst() const;
  char* beginWrite();

  void append(const std::string& str);
  void append(const char* str, size_t len);
  void append(const void* data, size_t len);
  void append(const TcpBuffer& buff);

  ssize_t readFd(int fd, int* Errno);
  ssize_t writeFd(int fd, int* Errno);

 private:
  char* getBeginPtr();
  const char* getBeginPtr() const;  //provide a const version for const member function calls
  void makeSpace(size_t len);

  std::vector<char> buffer_;

  size_t readerIndex_;
  size_t writerIndex_;
};


#endif