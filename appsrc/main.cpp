#pragma warning(disable: 4996)

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string>
#include <iostream>

#ifndef _WIN32

#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#include <arpa/inet>
#endif
#include <sys/socket.h>
#endif 

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

#include "../websocketsrc/WebSocketSession.h"

static const char MESSAGE[] = "Hello, Connection Successful!\n";
static const int PORT = 9999;

//监听回调函数，调用该函数的时候，连接已经建立，为新建立的fd创建bufferevent
static void listener_cb(struct evconnlistener*, evutil_socket_t, struct sockaddr*,
    int socklen, void*);

//读回调
static void conn_readcb(struct bufferevent*, void*);

//连接session
static void conn_session(struct bufferevent*, void*);

//写完成的回调函数
static void conn_writecb(struct bufferevent*, void*);

//连接回调函数
static void conn_eventcb(struct bufferevent*, short, void*);

//信号回调函数
static void signal_cb(evutil_socket_t, short, void*);

static std::shared_ptr<WebSocketSession> spSession;

int main(int argc, char** argv)
{
    struct event_base* base;
    struct evconnlistener* listener;
    struct event* signal_event;

    struct sockaddr_in sin;
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(0x0201, &wsa_data);
#endif

    base = event_base_new();
    if (!base)
    {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);

    listener = evconnlistener_new_bind(base, listener_cb, (void*)base, 
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, (struct sockaddr*)&sin, sizeof(sin));

    if (!listener)
    {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }

    //创建信号事件
    signal_event = evsignal_new(base, SIGINT, signal_cb, (void*)base);

    if (!signal_event || event_add(signal_event, NULL) < 0)
    {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return 1;
    }

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);

    return 0;

}

static void listener_cb(struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* sa, int socklen, void* user_data)
{
    struct event_base* base = (event_base*)user_data;
    struct bufferevent* bev;
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev)
    {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    spSession = std::make_shared<WebSocketSession>();
    bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_enable(bev, EV_READ);
    
    //bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void conn_readcb(struct bufferevent* bev, void* user_data)
{
    char buf[8192];
    size_t len = bufferevent_read(bev, buf, sizeof(buf));
    buf[len] = '\0';
    int i = strlen(buf);
    printf("Recive data is : %s\n", buf);
    //char reply[] = "server have read data\n";
    //bufferevent_write(bev, reply, sizeof(reply));
    //std::string buffer = std::string::to_string(buf);
    //std::shared_ptr<WebSocketSession> spSession(new WebSocketSession());
    std::string res;
    std::string buffer(buf);
    spSession->onRead(buffer, res);
    int n = bufferevent_write(bev, res.c_str(), res.length());
    //std::cout << "n = " << n << std::endl;
    std::cout << "res: " << res << std::endl;
    static bool lg = true;
    //发送登录成功的消息
    if (lg)
    {
        std::string login;
        spSession->send(MESSAGE, login);
        bufferevent_write(bev, login.c_str(), login.length());
        lg = false;
    }
    
    //std::cout << "login: " << login << std::endl;


}

static void conn_writecb(struct bufferevent* bev, void* user_data)
{
    struct evbuffer* output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0)
    {
        printf("数据已经写完\n");
       // bufferevent_free(bev);
    }
}

static void conn_eventcb(struct bufferevent* bev, short events, void* user_data)
{
    if (events & BEV_EVENT_EOF)
    {
        printf("Connection closed \n");
    }
    else if (events & BEV_EVENT_ERROR)
    {
        printf("Got an error on the connection: %s\n", strerror(errno));
    }
    bufferevent_free(bev);
}

static void signal_cb(evutil_socket_t sig, short events, void* user_data)
{
    struct event_base* base = (event_base*)user_data;
    struct timeval delay = {2, 0};

    printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");
    event_base_loopexit(base, &delay);
}

//static void conn_session(struct bufferevent* bev, void* user_data)
//{
//    std::shared_ptr<WebSocketSession> spSession(new WebSocketSession());
//    conn_readcb(bev, user_data, spSession);
//}