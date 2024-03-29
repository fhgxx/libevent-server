/*
 * ѹ��������
 * gaoxx 2020.01.02
 */

#pragma once

#include <string>

class ZlibUtil
{
private:
    ZlibUtil() = delete;
    ~ZlibUtil() = delete;
    ZlibUtil(const ZlibUtil& rhs) = delete;

public:
    static bool CompressBuf(const char* pSrcBuf, size_t nSrcBufLength, char* pDestBuf, size_t& nDestBufLength);
    static bool CompressBuf(const std::string& strSrcBuf, std::string& strDestBuf);
    static bool UncompressBuf(const std::string& strSrcBuf, std::string& strDestBuf, size_t nDestBufLength);

    //gzipѹ��
    static bool inflate(const std::string& strSrc, std::string& dest);

    //gzip��ѹ
    static bool deflate(const std::string& strSrc, std::string& sreDest);
};
