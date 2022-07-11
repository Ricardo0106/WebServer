/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#include "Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>
#include "Util.h"
#include "base/Logging.h"

Server::Server(
    EventLoop *loop, int threadNum, int port,
    int sqlPort, const char *sqlUser, const char *sqlPwd,
    const char *dbName, int connPoolNum)
    : loop_(loop),
      threadNum_(threadNum),
      eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)), //初始化一个线程池,线程池传入一个baseloop
      started_(false),
      acceptChannel_(new Channel(loop_)),
      port_(port),
      listenFd_(socket_bind_listen(port_)) {
  acceptChannel_->setFd(listenFd_);//主reactor的accpetchannel封装listenfd
  handle_for_sigpipe();
  srcDir_ = getcwd(nullptr, 256);
  assert(srcDir_);
  strncat(srcDir_, "/resources/", 16);
  HttpConn::srcDir = srcDir_;
  SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
  if (setSocketNonBlocking(listenFd_) < 0) {
    perror("set socket non block failed");
    abort();
  }
}

Server::~Server() {
  free(srcDir_);
  SqlConnPool::Instance()->ClosePool();
}

void Server::start() {
  eventLoopThreadPool_->start();
  // SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);
  // acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
  acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));  //处理新的连接
  acceptChannel_->setConnHandler(bind(&Server::handThisConn, this)); //处理这个连接
  loop_->addToPoller(acceptChannel_, 0);
  started_ = true;
}
//server有个专门的接受连接的channle，所以这个channnel对应的事件的回调在server中设置
void Server::handNewConn() {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;
  while ((accept_fd = accept(listenFd_, (struct sockaddr *)&client_addr,
                             &client_addr_len)) > 0) {
    //给新连接分配一个EventLoopThread，之后这个连接就从属于这个Thread
    EventLoop *loop = eventLoopThreadPool_->getNextLoop(); //对应这个连接的loop从线程池里找一个,将连接分发给他管理
    LOG << "********New connection from[" << inet_ntoa(client_addr.sin_addr) << ":"
        << ntohs(client_addr.sin_port) << "]accept fd:" << accept_fd;
    // cout << "new connection" << endl;
    // cout << inet_ntoa(client_addr.sin_addr) << endl;
    // cout << ntohs(client_addr.sin_port) << endl;
    /*
    // TCP的保活机制默认是关闭的
    int optval = 0;
    socklen_t len_optval = 4;
    getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
    cout << "optval ==" << optval << endl;
    */
    // 限制服务器的最大并发连接数
    if (accept_fd >= MAXFDS) {
      close(accept_fd);
      continue;
    }
    // 设为非阻塞模式
    if (setSocketNonBlocking(accept_fd) < 0) {
      LOG << "Set non block failed!";
      // perror("Set non block failed!");
      return;
    }

    setSocketNodelay(accept_fd);
    // setSocketNoLinger(accept_fd);
    //将此HttpData对应的fd，以及它从属的loop 生成一个HttpData
    shared_ptr<HttpConn> req_info(new HttpConn(loop, accept_fd));//将刚才从io线程中获得的一个subreactor与新建的连接绑定在一起，(将新连接分给这个subreactor)
    //将新生成的HttpData
    req_info->getChannel()->setHolder(req_info);
    loop->queueInLoop(std::bind(&HttpConn::newEvent, req_info));
  }
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}
