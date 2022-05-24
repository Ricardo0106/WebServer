/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "TcpBuffer.h"

TcpBuffer::TcpBuffer(int bufferSize) : buffer_(bufferSize), writerIndex_(0), readerIndex_(0) {}

size_t TcpBuffer::getReadableBytes() const {
  return writerIndex_ - readerIndex_;
}

size_t TcpBuffer::getWritableBytes() const {
  return buffer_.size() - writerIndex_;
} 

size_t TcpBuffer::getPrependableBytes() const {
  return readerIndex_;
}

const char*TcpBuffer::peek() const {
  return getBeginPtr() + readerIndex_;
}

void TcpBuffer::ensureWritable(size_t len) {
  if (getWritableBytes() < len) 
    makeSpace(len);
  assert(getWritableBytes() >= len);
}

void TcpBuffer::hasWritten(size_t len) {
  writerIndex_ += len;
}

//when we resize the buffer_, we use retrieve() to retrieve our readerIndex
void TcpBuffer::retrieve(size_t len) {
  assert(len <= getReadableBytes());
  readerIndex_ += len;
}

void TcpBuffer::retrieveUntil(const char *end) {
  assert(peek() <= end);
  retrieve(end - peek());
}

void TcpBuffer::RetrieveAll() {
  bzero(&buffer_[0], buffer_.size());
  readerIndex_ = 0;
  writerIndex_ = 0;
}

std::string TcpBuffer::RetrieveAllToStr() {
  std::string str(peek(), getReadableBytes());
  RetrieveAll();
  return str;
}

const char* TcpBuffer::beginWriteConst() const {
  return getBeginPtr() + writerIndex_;
}

char* TcpBuffer::beginWrite() {
  return getBeginPtr() + writerIndex_;
}

//The difference between data() and c_str() is that the array returned by the former is '\0'
void TcpBuffer::append(const std::string &str) {
  append(str.data(), str.size());
}

void TcpBuffer::append(const char* str, size_t len) {
  assert(str);
  ensureWritable(len);
  std::copy(str, str + len, getBeginPtr()); //append the content of stackBuff to buffer_                                                
  hasWritten(len);
} 

void TcpBuffer::append(const void* data, size_t len) {
  assert(data);
  append(static_cast<const char*> (data), len);
}

void TcpBuffer::append(const TcpBuffer& buff) {
  append(buff.peek(), buff.getReadableBytes());
}

ssize_t TcpBuffer::writeFd(int fd, int* saveErrno) {
  size_t readSize = getReadableBytes();
  ssize_t len = write(fd, peek(), readSize);
  if (len < 0) {
    *saveErrno = errno;
  }
  readerIndex_ += len;
  return len;
}

ssize_t TcpBuffer::readFd(int fd, int* saveErrno) {
  //Utilize the space on the temporary stack 
  //to avoid memory waste caused by too large initial Buffer of each connection
  //Also avoids the overhead of calling read() repeatedly
  char stackBuffer[65535];

  struct iovec vec[2];
  const size_t writable = getWritableBytes();

  vec[0].iov_base = getBeginPtr() + writerIndex_;
  vec[0].iov_len = writable;                        
  vec[1].iov_base = stackBuffer;
  vec[0].iov_len = sizeof(stackBuffer);

  const ssize_t len = readv(fd, vec, 2);
  
  if (len < 0) {
    *saveErrno = errno;
  } else if (static_cast<size_t>(len) <= writable) {
    //The amount of data read is small, the buffer is completely sufficient
    writerIndex_ += len;
  } else {
    //need to read is too much, buffer is not enough, part of it is read to stackbuffer
    writerIndex_ = buffer_.size();
    append(stackBuffer, len - writable);
  }
  return len;
}

//typeid(buffer_.begin()).name() is the N9__gnu_cxx17__normal_iteratorIPiSt6vectorIiSaIiEEEE
//typeid(*buffer_.begin()).name() is i
//typeid(&*buffer_.begin()).name() is Pi
char* TcpBuffer::getBeginPtr() {
  return &*buffer_.begin();
}

const char* TcpBuffer::getBeginPtr() const {
  return &*buffer_.begin();
} //provide a const version for const member function calls

void TcpBuffer::makeSpace(size_t len) {
  if (getWritableBytes() + getPrependableBytes() < len) {
    buffer_.resize(writerIndex_ + len + 1);
  } else {
    size_t readable = getReadableBytes();
    //Discard the preappendable area
    std::copy(getBeginPtr() + readerIndex_, getBeginPtr() + writerIndex_, getBeginPtr());
    readerIndex_ = 0;
    writerIndex_ = readerIndex_ + readable;
    assert(readable == getReadableBytes());
  }
}