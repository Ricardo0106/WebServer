set(LIB_SRC
    MainLogUtils.cc
    CountDownLatch.cc
    AppendFile.cc
    LogFile.cc
    Logging.cc
    LogStream.cc
    Sqlconnpool.cc
    TcpBuffer.cc
    Thread.cc
)

add_library(libserver_base ${LIB_SRC})
target_link_libraries(libserver_base pthread rt mysqlclient)

set_target_properties(libserver_base PROPERTIES OUTPUT_NAME "server_base")

add_subdirectory(tests)