#include "WebSocketSession.h"
#include "StringUtil.h"
#include "WebSocketHandshake.h"

#include <iostream>
#include <sstream>
#include <list>
#include <ctime>

#include <event2/bufferevent.h>


//�ͻ�����������HTTP���� 50M
#define MAX_WEBSOCKET_CLIENT_PACKAGE_LENGTH  50 * 1024 * 1024

//��С��websocket��ͷ
#define MIN_WEBSOCKET_PACKAGE_HEADER_LENGTH  6

//�������˷ְ���С��10M
#define MAX_WEBSOCKET_SERVER_PACKAGE_LENGTH  10 * 1024 * 1024

WebSocketSession::WebSocketSession() : m_bUpdateToWebSocket(false), m_bClientCompressed(false)
{
}

void WebSocketSession::onRead( std::string& buf, std::string& res)
{
    while (true)
    {
        size_t readableBytesCount = buf.size();
        if (readableBytesCount >= MAX_WEBSOCKET_CLIENT_PACKAGE_LENGTH)
        {
            return;
        }
        if (!m_bUpdateToWebSocket)
        {
            size_t  pos = buf.find_first_of("\r\n\r\n");
            if (pos == std::string::npos)
            {
                //����û������
                if (buf.size() < MAX_WEBSOCKET_CLIENT_PACKAGE_LENGTH)
                    return; //��ʱ���˳�����߼��������ȴ�������
                else
                {
                    return;
                }
            }

            //�������ˣ����н��
            size_t length = buf.size();
           // std::string currentData(buf, length);
            std::string currentData = buf;
            buf.clear();
            

            if (!handleHandshake(currentData, res))
            {
                return;
            }
            std::cout << "websocket message: " << currentData << std::endl;
        }
        else
        {
            if (readableBytesCount < MIN_WEBSOCKET_PACKAGE_HEADER_LENGTH)
                return;

            //�������
        }
    }
}

bool WebSocketSession::handleHandshake(const std::string& buf, std::string& res)
{
    std::vector<std::string> vecHttpHeaders;
    //�ԡ�\r\n����β��־���зָ�ÿһ��
    StringUtil::Split(buf, vecHttpHeaders, "\r\n");
    //������3��
    if (vecHttpHeaders.size() < 3)
    {
        return false;
    }

    std::vector<std::string> v;
    size_t vecLength = vecHttpHeaders.size();
    for (size_t i = 0; i < vecLength; ++i)
    {
        //��һ�л�ò������ƺ�Э��汾��
        if (i == 0)
        {
            if (!parseHttpPath(vecHttpHeaders[i]))
                return false;
        }
        //����ͷ����־
        else
        {
            v.clear();
            StringUtil::Cut(vecHttpHeaders[i], v, ":"); //�ѡ�����ȥ��
            if (v.size() < 2)
                return false;

            StringUtil::trim(v[1]);//�ո�ȥ��
            m_mapHttpHeaders[v[0]] = v[1];

        }
    }

    //��ͷ���ĸ����ֶν����ж�, Connection��ֵ��Upgrade
    auto target = m_mapHttpHeaders.find("Connection");
    if (target == m_mapHttpHeaders.end() || target->second != "Upgrade")
        return false;

    //Upgrade�ֶε�ֵ��websocket
    target = m_mapHttpHeaders.find("Upgrade");
    if (target == m_mapHttpHeaders.end() || target->second != "websocket")
        return false;
    
    //Host��ֵ��Ϊ��
    target = m_mapHttpHeaders.find("Host");
    if (target == m_mapHttpHeaders.end() || target->second.empty())
        return false;
    
    //Origin��ֵ��Ϊ��
    target = m_mapHttpHeaders.find("Origin");
    if (target == m_mapHttpHeaders.end() || target->second.empty())
        return false;

    //User-Agent
    target = m_mapHttpHeaders.find("User-Agent");
    if (target != m_mapHttpHeaders.end())
    {
        m_strUserAgent = target->second;
    }

    //Sec-WebSocket-Extensions
    target = m_mapHttpHeaders.find("Sec-WebSocket-Extensions");
    if (target != m_mapHttpHeaders.end())
    {
        std::vector<std::string> vecExtensions;
        StringUtil::Split(target->second, vecExtensions, ";");

        for (const auto& iter : vecExtensions)
        {
            //�ж���û��ѹ��
            if (iter == "permessage-deflate")
            {
                m_bClientCompressed = true;
                break;
            }
        }
    }

    target = m_mapHttpHeaders.find("Sec-WebSocket-Key");
    if (target == m_mapHttpHeaders.end() || target->second.empty())
        return false;

    char secWebSocketAccept[29] = {};
    js::WebSocketHandshake::generate(target->second.c_str(), secWebSocketAccept);
    std::string response;
    makeUpgradeResponse(secWebSocketAccept, response);
    //TODO����response
    //send(response);
    res = response;
    m_bUpdateToWebSocket = true;
    return true;

}

bool WebSocketSession::parseHttpPath(const std::string& str)
{
    std::vector<std::string> vecTags;
    StringUtil::Split(str, vecTags, " ");
    if (vecTags.size() != 3)
        return false;

    if (vecTags[0] != "GET")
        return false;

    std::vector<std::string> vecPathAndParams;
    StringUtil::Split(vecTags[1], vecPathAndParams, "?");
    if (vecPathAndParams.empty())
        return false;

    m_strURL = vecPathAndParams[0];
    if (vecPathAndParams.size() >= 2)
        m_strParams = vecPathAndParams[1];

    if (vecTags[2] != "HTTP/1.1")
        return false;

    return true;
}

void WebSocketSession::makeUpgradeResponse(const char* secWebSocketAccept, std::string& response)
{
    response = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Sec-Websocket-Accept: ";
    response += secWebSocketAccept;
    response += "\r\nServer: BTCMEXWebsocketServer 1.0.0\r\n";
    response += "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n";
    if (m_bClientCompressed)
        response += "Sec-WebSocket-Extensions: permessage-deflate; client_no_context_takeover\r\nDate: ";

    char szNow[64];
    time_t now = time(NULL);
    tm time;
#ifdef _WIN32
    localtime_s(&time, &now);
#else
    localtime_r(&now, &time);
#endif
    strftime(szNow, sizeof(szNow), "%Y%m%d%H%M%S", &time);
    response += szNow;
    response += "\r\n\r\n";

}