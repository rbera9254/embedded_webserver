#include "http_common.h"
#include <cctype>
#include <sstream>

HTTPMessage::HTTPMessage()
{
    this->isRequest = false;
    this->status = boost::beast::http::status::ok;
}

std::string HTTPMessage::getQuery(const std::string& key, const std::string& defaultValue) const
{
    auto it = query.find(key);
    if (it != query.end())
    {
        return it->second;
    }
    return defaultValue;
}

bool HTTPMessage::hasQuery(const std::string& key) const
{
    return query.find(key) != query.end();
}

std::string HTTPMessage::getHeader(const std::string& key, const std::string& defaultValue) const
{
    auto it = header.find(key);
    if (it != header.end())
    {
        return it->second;
    }
    return defaultValue;
}

bool HTTPMessage::hasHeader(const std::string& key) const
{
    return header.find(key) != header.end();
}

std::string urlDecode(const std::string& str)
{
    std::string decoded;
    decoded.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%')
        {
            if (i + 2 < str.length())
            {
                int hexValue = 0;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> hexValue)
                {
                    decoded += static_cast<char>(hexValue);
                    i += 2;
                }
                else
                {
                    decoded += '%';
                }
            }
            else
            {
                decoded += '%';
            }
        }
        else if (str[i] == '+')
        {
            decoded += ' ';
        }
        else
        {
            decoded += str[i];
        }
    }
    return decoded;
}