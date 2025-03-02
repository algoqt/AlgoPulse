#ifndef CONTEXT_H
#define CONTEXT_H

#include "Callback.hpp"
#include "Response.hpp"
#include "Url.hpp"

#include <assert.h>
#include <boost/asio.hpp>
#include <unordered_map>

namespace requests {

class Context
{
public:
    using IOService  = boost::asio::io_service;
    using Socket     = boost::asio::ip::tcp::socket;
    using Buffer     = boost::asio::streambuf;
    using String     = std::string;
    using StringMap  = std::unordered_map<String, String>;

    enum class Method { Get, Post };

    Context(IOService &service, const Url &url, Method method, const StringMap &data);
    
    Context(IOService &service, const Url &url, Method method, const StringMap &data, UserCallback cb, ErrorCallback errorCb);

    // disable the copy operations
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    // enable the move operations
    Context(Context &&) = default;
    Context &operator=(Context &&) = default;
    
    const Socket &socket() const
    {
        return sock_;
    }    

    const Url &url() const
    {
        return url_;
    }
    
    const Buffer &reqBuff() const
    {
        return requestBuff_;
    }
    
    const Buffer &respBuff() const
    {
        return responseBuff_;
    }

    Socket &socket()
    {
        return sock_;
    }    

    Url &url()
    {
        return url_;
    }
    
    Buffer &reqBuff()
    {
        return requestBuff_;
    }
    
    Buffer &respBuff()
    {
        return responseBuff_;
    }
    
    void setHeaders(const String &headers)
    {
        headers_ = headers;
    }
    
    void setHeaders(String &&headers) noexcept
    {
        headers_ = std::move(headers);
    }
    void setStatusLine(const String& statusLine)
    {
        statusLine_ = statusLine;
    }

    void setStatusLine(String&& statusLine) noexcept
    {
        statusLine_ = std::move(statusLine);
    }
    
    void appendContent(const String &content)
    {
        content_.append(content);
    }
    
    void handleResponse()
    {
        assert(callback_);
        
        Response resp(std::move(statusLine_),std::move(headers_), std::move(content_));
        callback_(resp);
    }    

    void handleError(Exception &e)
    {
        assert(errorCallback_);

        errorCallback_(e);        
    }
    
    Response getResponse()
    {
        assert(!callback_);
        
        Response resp(std::move(statusLine_), std::move(headers_), std::move(content_));
        return resp;
    }
    
private:
    Socket        sock_;
    Url           url_;
    UserCallback  callback_;
    ErrorCallback errorCallback_;    
    Method        method_;
    Buffer        requestBuff_;
    Buffer        responseBuff_;
    String        statusLine_;
    String        headers_;
    String        content_;
};

}  // namespace requests

#endif /* CONTEXT_H */
