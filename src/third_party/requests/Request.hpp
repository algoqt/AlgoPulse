#ifndef REQUESTS_H
#define REQUESTS_H

#include "Callback.hpp"
#include "Response.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace requests {    

class Url;
class Context;
   
class Request
{
public:
    using IOService   = boost::asio::io_service;
    using Resolver    = boost::asio::ip::tcp::resolver;
    using Socket      = boost::asio::ip::tcp::socket;
    using SocketPtr   = std::unique_ptr<Socket>;
    using ErrorCode   = boost::system::error_code;
    using Buffer      = boost::asio::streambuf;
    using String      = std::string;
    using StringMap   = std::unordered_map<String, String>;
    
    Request();

    // disable the copy operations
    Request(const Request &) = delete;
    Request &operator=(const Request &) = delete;

    // enable the move operations
    Request(Request &&) = default;
    Request &operator=(Request &&) = default;

    Response get(const Url &url);
    
    Response get(const Url &url, const StringMap &params);
    
    Response post(const Url &url, const StringMap &data);            
private:
    // make HTTP request and read response
    void makeHttpRequest(Context &context);

    // resolve domain name synchronously
    void syncResolve(Context &context);

    // send HTTP request headers and body synchronously
    void syncSendRequest(Context &context);

    // read HTTP response synchronously
    void syncReadResponse(Context &context);
        
    IOService service_;
    Resolver  resolver_;
};

} // namespace requests

#endif /* REQUESTS_H */
