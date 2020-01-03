#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <list>

class WebSocketServer final
{
public:
    WebSocketServer();
    ~WebSocketServer() = default;
    WebSocketServer(const WebSocketServer& rhs) = delete;
    WebSocketServer& operator =(const WebSocketServer& rhs) = delete;

public:
    bool init(const char* ip, short port);
    void uninit();

    void onConnection();

    void onClose();

private:
    std::mutex       m_mutexForSession;

    std::string      m_strWsHost;

    int              m_wsPort;
};