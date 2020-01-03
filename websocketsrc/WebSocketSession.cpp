#include "../websocketsrc/WebSocketSession.h"
#include "../utils/StringUtil.h"
#include "../websocketsrc/WebSocketHandshake.h"
#include "../zlib/ZlibUtil.h"

#include <iostream>
#include <sstream>
#include <list>
#include <ctime>

#include <event2/bufferevent.h>


//客户端允许的最大HTTP包， 50M
#define MAX_WEBSOCKET_CLIENT_PACKAGE_LENGTH  50 * 1024 * 1024

//最小的websocket包头
#define MIN_WEBSOCKET_PACKAGE_HEADER_LENGTH  6

//服务器端分包大小，10M
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
                //包还没有收完
                if (buf.size() < MAX_WEBSOCKET_CLIENT_PACKAGE_LENGTH)
                    return; //暂时先退出解包逻辑，继续等待收数据
                else
                {
                    return;
                }
            }

            //包收完了，进行解包
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

            //正常解包
            else
            {
                if (readableBytesCount < MIN_WEBSOCKET_PACKAGE_HEADER_LENGTH)
                    return;
                if (!decodePackage(buf, res))
                {

                }
                buf.clear();

                 return;
            }
        }
    }
}

//正常解包逻辑
bool WebSocketSession::decodePackage(const std::string& buf, std::string& res)
{
    size_t readableBytesCount = buf.length();
    const int32_t TWO_FLAG_BYTES = 2;

    //最大包头长度
    const int32_t MAX_HEADER_LENGTH = 14;
    char pBytes[MAX_HEADER_LENGTH] = { 0 };

    //当收到的数据大于最大包长时仅拷贝可能是包头的最大部分
    if (readableBytesCount > MAX_HEADER_LENGTH)
    {
        memcpy(pBytes, buf.c_str(), MAX_HEADER_LENGTH * sizeof(char));
    }
    //如果不够最大包头部分直接读完
    else
    {
        memcpy(pBytes, buf.c_str(), readableBytesCount * sizeof(char));
    }
    int i = 0;
    while (pBytes[i] != '\0')
    {
        std::cout << "pBytes: " << pBytes[i] << std::endl;
        ++i;
    }
    bool FIN = (pBytes[0] & 0x80);
    std::cout << "buf: " << buf;

    //获取第一个字节的低四位数据
    int32_t opcode = (int32_t)(pBytes[0] & 0xF);

    //取第二个字节的最高位,mask标志位
    bool mask = ((pBytes[1]) & 0x80);

    int32_t headerSize = 0;
    int64_t bodyLength = 0;
    if (mask)
    {
        headerSize += 4;
    }

    //取第二个字节的低7位，这七位是表示数据长度信息
    int32_t payloadLength = (int32_t)(pBytes[1] & 0x7F);
    //表示数据长度信息的7位值最大只能是127
    if (payloadLength <= 0 && payloadLength > 127)
    {
        std::cout << "invalid payload length" << std::endl;
        return false;
    }

    //数据长度是在1到125之间，不用额外位置表示长度
    if (payloadLength > 0 && payloadLength <= 125)
    {
        headerSize += TWO_FLAG_BYTES;
        bodyLength = payloadLength;
    }
    //当这低七位的值是126，那就需要额外的两个字节来表示数据的长度，两个字节最大值是65535
    else if (payloadLength == 126)
    {
        headerSize += TWO_FLAG_BYTES;
        headerSize += sizeof(short);

        if ((int32_t)readableBytesCount < headerSize)
        {
            return true;
        }
        short tmp;
        //拷贝两个字节，这两个字节表示的是数据的长度信息
        memcpy(&tmp, &pBytes[2], 2);
        int32_t extendedPayloadLength = ::ntohs(tmp);
        bodyLength = extendedPayloadLength;
        //包体长度如果不符合，返回错误
        if (bodyLength < 126 || bodyLength > UINT16_MAX)
        {
            std::cout << "illegal extendedPayloadLength" << std::endl;
            return false;
        }
    }

    else if (payloadLength == 127)
    {
        //如果是127， 就用接下来的8个字节表示包体长度
        headerSize += TWO_FLAG_BYTES;
        headerSize += sizeof(uint64_t);

        //包体长度不够直接退出
        if ((uint32_t)readableBytesCount < headerSize)
        {
            return true;
        }

        int64_t tmp;
        //拷贝8个字节的包体长度信息
        memcpy(&tmp, &pBytes[2], 8);
        int64_t extendedPayloadLength = ::ntohll(tmp);
        bodyLength = extendedPayloadLength;
        //校验转换网络字节序后的包体长度是否符合
        if (bodyLength <= UINT16_MAX)
        {
            std::cout << "illegal extendedloadLength" << std::endl;
            return false;
        }
    }

    //如果可读数据的大小不够一个完整的包大小，先退出，下次再进行判断
    if ((int32_t)readableBytesCount < headerSize + bodyLength)
    {
        return true;
    }
    std::string payloadData;
   /* const char* p = buf.c_str();
    for (int i = 0; i < headerSize; ++i)
    {
        p++;
    }
    p[0] = '\0';*/
    //取出包体数据
    payloadData = buf.substr(headerSize, bodyLength);
    int j = payloadData.size();
    //memcpy(&payloadData, p, bodyLength);
    
    if (mask)
    {
        char maskingKey[4] = { 0 };
        memcpy(maskingKey, pBytes + headerSize - 4, 4);
        unmaskData(payloadData, maskingKey);
    }

    if (FIN)
    {
        m_strParsedData.append(payloadData);
        //处理包数据
        if (!processPackage(payloadData, (MyOpCode)opcode, res))
        {
            return false;
        }

        m_strParsedData.clear();
    }

}

void WebSocketSession::unmaskData(std::string& src, const char* maskingKey)
{
    char j;
    for (size_t n = 0; n < src.length(); ++n)
    {
        j = n % 4;
        src[n] = src[n] ^ maskingKey[j];
    }
}

bool WebSocketSession::processPackage(const std::string& data, MyOpCode opcode, std::string& res)
{
    if (opcode == MyOpCode::TEXT_FRAME || opcode == MyOpCode::BINARY_FRAME)
    {
       std::string response;
        if (m_bClientCompressed)
        {
            if (!ZlibUtil::inflate(data, response))
            {
                return false;
            }
        }
        else
        {
            response = data;
        }
        std::cout << "revice data response: " << response << std::endl;
       // std::string message;
        //回显消息
        send(response, res);
    }
}

bool WebSocketSession::handleHandshake(const std::string& buf, std::string& res)
{
    std::vector<std::string> vecHttpHeaders;
    //以“\r\n”结尾标志进行分割每一行
    StringUtil::Split(buf, vecHttpHeaders, "\r\n");
    //至少有3行
    if (vecHttpHeaders.size() < 3)
    {
        return false;
    }

    std::vector<std::string> v;
    size_t vecLength = vecHttpHeaders.size();
    for (size_t i = 0; i < vecLength; ++i)
    {
        //第一行获得参数名称和协议版本号
        if (i == 0)
        {
            if (!parseHttpPath(vecHttpHeaders[i]))
                return false;
        }
        //解析头部标志
        else
        {
            v.clear();
            StringUtil::Cut(vecHttpHeaders[i], v, ":"); //把“：”去掉
            if (v.size() < 2)
                return false;

            StringUtil::trim(v[1]);//空格去掉
            m_mapHttpHeaders[v[0]] = v[1];

        }
    }

    //对头部的各个字段进行判断, Connection的值是Upgrade
    auto target = m_mapHttpHeaders.find("Connection");
    if (target == m_mapHttpHeaders.end() || target->second != "Upgrade")
        return false;

    //Upgrade字段的值是websocket
    target = m_mapHttpHeaders.find("Upgrade");
    if (target == m_mapHttpHeaders.end() || target->second != "websocket")
        return false;
    
    //Host的值不为空
    target = m_mapHttpHeaders.find("Host");
    if (target == m_mapHttpHeaders.end() || target->second.empty())
        return false;
    
    //Origin的值不为空
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
            //判断有没有压缩
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
    //TODO发送response
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

//组装应答报文
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

//将发送的信息组装成websocket协议格式
void WebSocketSession::send(const std::string& data, std::string& login, int32_t opcode /*=MyOpCode::TEXT*/, bool compress /* = false*/)
{
    size_t  dataLength = data.length();
    std::string destbuf;
    if (m_bClientCompressed && dataLength > 0)
    {
        if (!ZlibUtil::deflate(data, destbuf))
        {
            std::cout << "compress buf error, data: " << data << std::endl;
            return;
        }
    }
    else
        destbuf = data;

    std::cout << "destbuf.length() : " << destbuf.length() << std::endl;

    dataLength = destbuf.length();

    char firstTwoBytes[2] = { 0 };
    firstTwoBytes[0] |= 0x80;
    firstTwoBytes[0] |= opcode;

    const char compressFlag = 0x40;
    if (m_bClientCompressed)
        firstTwoBytes[0] |= compressFlag;

    std::string actualSendData;

    if (dataLength < 126)
    {
        firstTwoBytes[1] = dataLength;
        actualSendData.append(firstTwoBytes, 2);
    }

    else if (dataLength <= UINT16_MAX)
    {
        firstTwoBytes[1] = 126;
        char extendedPlayloadLength[2] = { 0 };
        uint16_t tmp = ::htons(dataLength);
        memcpy(&extendedPlayloadLength, &tmp, 2);
        actualSendData.append(firstTwoBytes, 2);
        actualSendData.append(extendedPlayloadLength, 2);


    }
    else
    {
        firstTwoBytes[1] = 127;
        char extendedPlayloadLength[8] = { 0 };
        uint64_t tmp = ::htonl((uint64_t) dataLength);
        memcpy(&extendedPlayloadLength, &tmp, 8);
        actualSendData.append(firstTwoBytes, 2);
        actualSendData.append(extendedPlayloadLength, 8);

    }
    actualSendData.append(destbuf);
    login = actualSendData;

}