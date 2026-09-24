#ifndef FLEET_HTTP_COMMON_H
#define FLEET_HTTP_COMMON_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <string>
#include <unordered_map>

enum RequestType
{
    HEAD,
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS
};

struct HTTPMessage
{
    bool isRequest;
    RequestType type;
    boost::beast::http::status status;
    std::unordered_map<std::string, std::string> header;
    std::unordered_map<std::string, std::string> query;
    std::string body;

    HTTPMessage();

    
    std::string getQuery(const std::string& key, const std::string& defaultValue = "") const;
    bool hasQuery(const std::string& key) const;
    std::string getHeader(const std::string& key, const std::string& defaultValue = "") const;
    bool hasHeader(const std::string& key) const;
};


std::string urlDecode(const std::string& str);

#endif //FLEET_HTTP_COMMON_H
