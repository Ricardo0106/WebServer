// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Epoll.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <deque>
#include <queue>
#include "Util.h"
#include "base/Logging.h"


#include <arpa/inet.h>
#include <iostream>
using namespace std;
const int EVENTSNUM = 4096;
const int EPOLLWAIT_TIME = 10000;

typedef shared_ptr<Channel> SP_Channel;

Epoll::Epoll() : epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(EVENTSNUM) {
  assert(epollFd_ > 0);
}
Epoll::~Epoll() {}

// 注册新描述符
void Epoll::epoll_add(SP_Channel request, int timeout) {
  int fd = request->getFd();
  if (timeout > 0) {
    add_timer(request, timeout);
    fd2http_[fd] = request->getHolder();  //httpdata持有+1
  }
  struct epoll_event event;
  event.data.fd = fd;
  /**
   *  event.events 用channel封装
   * 使用epoll：将一个文件描述符添加到epoll红黑树，当该文件描述符上有事件发生时，拿到它、处理事件，
   * 这样我们每次只能拿到一个文件描述符，也就是一个int类型的整型值。试想，如果一个服务器同时提供不同的服务，
   * 如HTTP、FTP等，那么就算文件描述符上发生的事件都是可读事件，不同的连接类型也将决定不同的处理逻辑，
   * 仅仅通过一个文件描述符来区分显然会很麻烦，我们更加希望拿到关于这个文件描述符更多的信息。
   * epoll中的data其实是一个联合类型，可以储存一个指针。
   * 而通过指针，理论上我们可以指向任何一个地址块的内容，可以是一个类的对象，这样就可以将一个文件描述符封装成一个Channel类，
   * 一个Channel类自始至终只负责一个文件描述符，对不同的服务、不同的事件类型，都可以在类中进行不同的处理，
   * 而不是仅仅拿到一个int类型的文件描述符。
  **/
  event.events = request->getEvents();  //要监听的事件就是这个http请求的事件

  request->EqualAndUpdateLastEvents();

  fd2chan_[fd] = request;
  if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) { //往红黑树上添加事件
    perror("epoll_add error");
    fd2chan_[fd].reset();
  }
}

// 修改描述符状态
void Epoll::epoll_mod(SP_Channel request, int timeout) {
  if (timeout > 0) add_timer(request, timeout);//如果timeout是0的话那他不是一个定时事件
  int fd = request->getFd();
  if (!request->EqualAndUpdateLastEvents()) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents();
    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0) {//设置epollfd这个红黑树上的fd这个文件描述符对应的event
      perror("epoll_mod error");
      fd2chan_[fd].reset(); //出错之后就把对当前持有的Channel
    }
  }
}

// 从epoll中删除描述符
void Epoll::epoll_del(SP_Channel request) {
  int fd = request->getFd(); //channel不持有fd，所以不能再channel中释放fd，但是channel封装了fd以及他对应的事件，所以可以从channel中getfd
  struct epoll_event event;
  event.data.fd = fd;
  event.events = request->getLastEvents();
  // event.events = 0;
  // request->EqualAndUpdateLastEvents()
  if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &event) < 0) {
    perror("epoll_del error");
  }
  fd2chan_[fd].reset();
  fd2http_[fd].reset();
}



// 返回活跃事件数
std::vector<SP_Channel> Epoll::poll() {
  while (true) {
    int event_count =
        epoll_wait(epollFd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);//将所有就绪事件从内核事件表中复制到events_中
    if (event_count < 0) perror("epoll wait error");
    std::vector<SP_Channel> req_data = getEventsRequest(event_count);
    if (req_data.size() > 0) return req_data;
  }
}

void Epoll::handleExpired() { timerManager_.handleExpiredEvent(); }

// 分发处理函数
// 并不会处理事件，而是分发事件
std::vector<SP_Channel> Epoll::getEventsRequest(int events_num) {
  std::vector<SP_Channel> req_data;
  for (int i = 0; i < events_num; ++i) {
    // 获取有事件产生的描述符
    int fd = events_[i].data.fd;

    SP_Channel cur_req = fd2chan_[fd]; //取出fd对应的channel

    if (cur_req) {
      cur_req->setRevents(events_[i].events);
      cur_req->setEvents(0);
      // 加入线程池之前将Timer和request分离
      //cur_req->seperateTimer();
      req_data.push_back(cur_req);
    } else {
      LOG << "SP cur_req is invalid";
    }
  }
  return req_data;
  //RVO编译器优化，对局部变量和要返回的临时变量的地址进行复用，避免了局部变量作为返回条件时的一次拷贝
}
void Epoll::add_timer(SP_Channel request_data, int timeout) {
  shared_ptr<HttpConn> t = request_data->getHolder();//从当前这个request对象get他持有的http对象的shareptr
  if (t)
    timerManager_.addTimer(t, timeout);
  else
    LOG << "timer add fail";
}
