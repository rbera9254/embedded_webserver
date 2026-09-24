#include <sstream>
#include <boost/asio/io_service.hpp>
#include <boost/tokenizer.hpp>

#include "ssl_certificate.h"
#include "WebServer.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void
abort_server(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// This is the C++11 equivalent of a generic lambda.
// The function object is used to send an HTTP message.
template<class Stream>
struct send_lambda
{
    Stream& stream_;
    bool& close_;
    beast::error_code& ec_;

    explicit
    send_lambda(
            Stream& stream,
            bool& close,
            beast::error_code& ec)
            : stream_(stream)
            , close_(close)
            , ec_(ec)
    {
    }

    template<bool isRequest, class Body, class Fields>
    void
    operator()(http::message<isRequest, Body, Fields>&& msg) const
    {
        // Determine if we should close the connection after
        close_ = msg.need_eof();

        // We need the serializer here because the serializer requires
        // a non-const file_body, and the message oriented version of
        // http::write only works with const messages.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(stream_, sr, ec_);
    }
};

void WebServer::do_session(boost::asio::ip::tcp::socket &socket, boost::asio::ssl::context& ctx)
{
    bool close = false;
    beast::error_code ec;

    // Construct the stream around the socket
    beast::ssl_stream<tcp::socket&> stream{socket, ctx};

    // Perform the SSL handshake
    stream.handshake(ssl::stream_base::server, ec);
    if(ec)
        return abort_server(ec, "handshake");

    // This buffer is required to persist across reads
    beast::flat_buffer buffer;
    beast::flat_buffer header_buffer;

    // This lambda is used to send messages
    send_lambda<beast::ssl_stream<tcp::socket&>> lambda{stream, close, ec};

    for(;;)
    {
        // Read a request
        http::request<http::string_body> req;
        http::request_parser<http::string_body> parser;
        parser.body_limit(std::numeric_limits<std::uint64_t>::max());
        http::read(stream, buffer, parser, ec);
        //http::read(stream, buffer, req, ec);

        if(ec == http::error::end_of_stream)
            break;
        else if (ec == boost::asio::ssl::error::stream_truncated)
        {
            /*
             * HTTP clients may not close connection cleanly. This case is to be ignored.
             */
            return;
        }
        else if(ec)
            return abort_server(ec, "read");

        // Send the response
        auto _req = parser.get();
        handle_request(std::move(_req), lambda);
        if(ec)
            return abort_server(ec, "write");
        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            break;
        }
    }

    // Perform the SSL shutdown
    stream.shutdown(ec);
    if(ec)
        return abort_server(ec, "shutdown");

    // At this point the connection is closed gracefully
}

void WebServer::run()
{
    try
    {
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(this->host, std::to_string(this->port));
        boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);

        boost::asio::ip::tcp::endpoint endpoint = iter->endpoint();
        std::string ipAddr = endpoint.address().to_string();

        auto const address = net::ip::make_address(ipAddr);

        // The io_context is required for all I/O
        net::io_context ioc{1};

        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12};

        // This holds the self-signed certificate used by the server
        load_server_certificate(ctx, this->ssl_certificate, this->ssl_private_key, this->diffie_hellman_key, this->private_key_password);

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address, port}};

        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{std::bind(&WebServer::do_session, this, std::move(socket), std::ref(ctx))}.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

template<class Body, class Allocator, class Send>
void WebServer::handle_request(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req, Send&& send)
{
    // Returns a bad request response
    auto const bad_request =
            [&req](beast::string_view why)
            {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = std::string(why);
                res.prepare_payload();
                return res;
            };

    HTTPMessage message;
    message.isRequest = true;

    if (req.method() == http::verb::get)
        message.type = RequestType::GET;
    else if (req.method() == http::verb::post)
        message.type = RequestType::POST;
    else if (req.method() == http::verb::delete_)
        message.type = RequestType::DELETE;
    else if (req.method() == http::verb::put)
        message.type = RequestType::PUT;
    else if (req.method() == http::verb::head)
        message.type = RequestType::HEAD;
    else if (req.method() == http::verb::options)
        message.type = RequestType::OPTIONS;

    std::stringstream sstr;

    for (auto const& hdr : req)
    {
        sstr.str(std::string());
        sstr << hdr.name();

        std::string name, value;
        name = sstr.str();
        sstr.str(std::string());

        sstr << hdr.value();
        value = sstr.str();

        message.header[name] = value;
    }

    message.body = req.body();
    std::string target_full = std::string(req.target());
    std::string target_path = target_full;
    std::string query_string;

    size_t qmark_pos = target_full.find('?');
    if (qmark_pos != std::string::npos)
    {
        target_path = target_full.substr(0, qmark_pos);
        query_string = target_full.substr(qmark_pos + 1);
    }

    if (!query_string.empty())
    {
        std::stringstream ss(query_string);
        std::string item;
        while (std::getline(ss, item, '&'))
        {
            if (item.empty()) continue;
            size_t eq_pos = item.find('=');
            if (eq_pos != std::string::npos)
            {
                std::string key = urlDecode(item.substr(0, eq_pos));
                std::string value = urlDecode(item.substr(eq_pos + 1));
                if (!key.empty())
                {
                    message.query[key] = value;
                }
            }
            else
            {
                std::string key = urlDecode(item);
                if (!key.empty())
                {
                    message.query[key] = "";
                }
            }
        }
    }

    
    HTTPMessage reply = this->router.run(target_path, message);


    http::response<http::string_body> res{reply.status, req.version()};

    for (auto &it : reply.header)
    {
        res.set(it.first, it.second);
    }

    res.body() = reply.body;
    res.content_length(reply.body.length());
    res.keep_alive(req.keep_alive());
    res.prepare_payload();

    return send(std::move(res));
}

WebServer::WebServer(RequestRouter router, std::string host, unsigned short port)
{
    this->router = router;
    this->host = std::move(host);
    this->port = port;
}

void WebServer::setTlsCertificates(std::string ssl_certificate, std::string ssl_private_key,
                                   std::string diffie_hellman_key, std::string private_key_password)
{
    this->ssl_certificate = ssl_certificate;
    this->ssl_private_key = ssl_private_key;
    this->diffie_hellman_key = diffie_hellman_key;
    this->private_key_password = private_key_password;
}