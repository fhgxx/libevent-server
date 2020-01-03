//字符串操作工具类

#pragma once

#include <string>
#include <vector>

class StringUtil {
private:
    StringUtil() = delete;
    ~StringUtil() = delete;
    StringUtil(const StringUtil& rhs) = delete;
    StringUtil& operator = (const StringUtil& rhs) = delete;

public:
    static void Split(const std::string& str, std::vector<std::string>& v, const char* delimiter = "|");
    static void Cut(const std::string& str, std::vector<std::string>& v, const char* delimiter = "|");
    static std::string& Replace(std::string& str, const std::string& toReplaced, const std::string& newStr);

    static void trimLeft(std::string& str, char trimed = ' ');
    static void trimRight(std::string& str, char trimed = ' ');
    static void trim(std::string& str, char trimed = ' ');
};