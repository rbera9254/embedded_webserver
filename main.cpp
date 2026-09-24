#include <iostream>
#include <sstream>
#include "./libhttpserver/RequestRouter.h"
#include "./libhttpserver/WebServer.h"

int main() {
    RequestRouter router;

    
    router["/hello"].get([](const HTTPMessage& req) -> HTTPMessage
                         {
                            HTTPMessage response;
                            response.body = "hello world!";
                            response.status = boost::beast::http::status::ok;
                            response.header["Content-Type"] = "text/plain";
                            return response;
                         });

    
    router["/greet"].get([](const HTTPMessage& req) -> HTTPMessage
                         {
                             HTTPMessage response;
                             std::string name = req.getQuery("name", "Guest");
                             response.body = "Hello, " + name + "!";
                             response.status = boost::beast::http::status::ok;
                             response.header["Content-Type"] = "text/plain";
                             return response;
                         });

    
    router["/search"].get([](const HTTPMessage& req) -> HTTPMessage
                          {
                              HTTPMessage response;
                              std::stringstream ss;
                              ss << "Search Query : " << req.getQuery("q", "(none)") << "\n";
                              ss << "Page Num     : " << req.getQuery("page", "1") << "\n\n";
                              ss << "All Query Parameters:\n";
                              for (const auto& [key, value] : req.query)
                              {
                                  ss << "  " << key << " = " << value << "\n";
                              }
                              response.body = ss.str();
                              response.status = boost::beast::http::status::ok;
                              response.header["Content-Type"] = "text/plain";
                              return response;
                          });

    WebServer server(router, "localhost", 8888);
    server.setTlsCertificates("/tmp/ssl/localhost_certificate.crt",
                              "/tmp/ssl/localhost_private.key",
                              "/tmp/ssl/diffey_hellman.pem",
                              "private_key_password");
    server.run();
    return 0;
}
