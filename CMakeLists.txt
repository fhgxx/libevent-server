##使用cmake进行编译
##2020.01.03

cmake_minimum_required(VERSION 2.6)

project (LIBEVENT_SERVER)
#ifndef WIN32
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -O0  -pthread")
#endif

set(zlib_srcs
zlib/zutil.c
zlib/uncompr.c
zlib/trees.c
zlib/inftrees.c
zlib/inflate.c
zlib/inffast.c
zlib/infback.c
zlib/gzwrite.c
zlib/gzread.c
zlib/gzlib.c
zlib/gzclose.c
zlib/deflate.c
zlib/crc32.c
zlib/compress.c
zlib/adler32.c
zlib/ZlibUtil.cpp
)

set(utils_srcs
utils/StringUtil.cpp
)

set(websocket_srcs
websocketsrc/WebSocketServer.cpp
websocketsrc/WebSocketSession.cpp
)

set(app_srcs
appsrc/main.cpp
)

include_directories(
libevent_include/
libevent_include/event2/
)

link_directories(
libevent_lib/Debug/
)

add_executable(libeventserver ${app_srcs} ${websocket_srcs} ${utils_srcs} ${zlib_srcs})
target_link_libraries(libeventserver event.lib event_core.lib event_extra.lib ws2_32.lib wsock32.lib)