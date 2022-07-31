set(SRCS
    Channel.cc
    Epoll.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    HttpConn.cc
    HttpRequest.cc
    HttpResponse.cc
    Main.cc
    Server.cc
    Timer.cc
    Util.cc
)
include_directories(${PROJECT_SOURCE_DIR}/base)


add_executable(WebServer ${SRCS})
target_link_libraries(WebServer libserver_base)


add_subdirectory(base)
add_subdirectory(tests)