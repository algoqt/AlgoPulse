#include <requests/AsyncRequest.hpp>
#include <requests/Context.hpp>
#include <requests/Exception.hpp>
#include <requests/Url.hpp>
#include <requests/Utils.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
using namespace requests;

AsyncRequest::AsyncRequest(std::size_t threadNum)
    : service_(),
      resolver_(service_),
      work_(new Work(service_))
{
    for (int i = 0; i < threadNum; ++i)
    {
        threads_.emplace_back([this] () {   
            SPDLOG_INFO("thread for http request.");
            service_.run();
                              });
    }
}

AsyncRequest::~AsyncRequest()
{
    work_.reset(nullptr);

    for (auto &t: threads_)
    {
        t.join();
    }
}

void AsyncRequest::get(const Url &url, UserCallback cb, ErrorCallback errorCb)
{
    // empty query parameters
    StringMap params;
        
    return get(url, params, cb, errorCb);
}
    
void AsyncRequest::get(const Url &url, const StringMap &params, UserCallback cb, ErrorCallback errorCb)
{
    Resolver::query query(url.host(), url.port());
        
    auto context = std::make_shared<Context>(service_, url, Context::Method::Get, params, cb, errorCb);        
    
    resolver_.async_resolve(query,
                            [this, context] (const ErrorCode &err, Resolver::iterator iter)
                            {
                                handleResolve(err, iter, context);
                            });                
}    

void AsyncRequest::post(const Url &url, const StringMap &data, UserCallback cb, ErrorCallback errorCb)
{ 
    Resolver::query query(url.host(), url.port());

    auto context = std::make_shared<Context>(service_, url, Context::Method::Post, data, cb, errorCb);        
        
    resolver_.async_resolve(query,
                            [this, context] (const ErrorCode &err, Resolver::iterator iter)
                            {
                                handleResolve(err, iter, context);
                            });                        
}
    
void AsyncRequest::handleResolve(const ErrorCode &err, Resolver::iterator iter, ContextPtr context)
{
    if (err)
    {        
        Exception e("Resolve domain name error: " + err.message());
        context->handleError(e);
        return;        
    }
        
    auto endpoint = iter->endpoint();
    auto &sock = context->socket();

    ++iter;        
    sock.async_connect(endpoint,
                       [this, iter, context] (const ErrorCode &err)
                       {
                           handleConnect(err, iter, context);
                       });
}

void AsyncRequest::handleConnect(const ErrorCode &err, Resolver::iterator iter, ContextPtr context)
{
    auto &sock = context->socket();
    auto &buffer = context->reqBuff();
        
    if (err)
    {
        Resolver::iterator nullIter;            
        if (iter == nullIter)
        {            
            Exception e("Resolve domain name error: " + err.message());
            context->handleError(e);
            return;            
        }
        sock.close();
            
        //ErrorCode nullErr;
        //handleResolve(nullErr, iter, context);            
    }
    else
    {
        boost::asio::async_write(sock, buffer,
                                 [this, context] (const ErrorCode &err,
                                                  std::size_t /* bytesTransferred */) 
                                 {
                                     handleWrite(err, context);
                                 });            
    }
}
    
void AsyncRequest::handleWrite(const ErrorCode &err, ContextPtr context)
{
    if (err)
    {
        Exception e("Send request headers to server error: " + err.message());
        context->handleError(e);
        return;
    }

    auto &sock = context->socket();
    auto &respBuffer = context->respBuff();

    // shutdown on write
    //sock.shutdown(Socket::shutdown_send);
        
    boost::asio::async_read_until(sock, respBuffer, "\r\n\r\n",
                                  [this, context] (const ErrorCode &err,
                                                   std::size_t /* bytesTransferred */)
                                  {
                                      handleReadHeaders(err, context);
                                  });
}

void AsyncRequest::handleReadHeaders(const ErrorCode &err, ContextPtr context)
{
    if (err)
    {
        Exception e("Read response headers error: " + err.message());
        context->handleError(e);
        return;        
    }

    auto &sock = context->socket();
    auto &respBuffer = context->respBuff();

    // NOTE: str may contain mutiple "\r\n\r\n"
    auto str = bufferToString(respBuffer);
    auto parts = splitString(str, "\r\n\r\n", 1);
    String befor_content, content;
    String statusLine,headers;
    if (parts.size() > 0)
    {
        befor_content = std::move(parts[0]);
    }
    auto status_headers = splitString(befor_content, "\r\n", 1);

    if (status_headers.size() > 0)
    {
        statusLine = std::move(status_headers[0]);
    }
    if (status_headers.size() > 1)
    {
        headers = std::move(status_headers[1]);
    }

    if (parts.size()  > 1)
    {
        content = std::move(parts[1]);
    }

    context->setStatusLine(std::move(statusLine));
    context->setHeaders(std::move(headers));
    context->appendContent(content);
                
    boost::asio::async_read(sock,
                            respBuffer,
                            boost::asio::transfer_at_least(1),
                            [this, context] (const ErrorCode &err,
                                             std::size_t /* bytes_transferred */)
                            {
                                handleReadBody(err, context);
                            });        
}

void AsyncRequest::handleReadBody(const ErrorCode &err, ContextPtr context)
{
    if (err)
    {
        if (err != boost::asio::error::eof)
        {            
            Exception e("Read response body error: " + err.message());
            context->handleError(e);
            return;            
        }
            
        context->handleResponse();

        return;
    }

    auto &sock = context->socket();
    auto &respBuffer = context->respBuff();

    auto content = bufferToString(respBuffer);
    context->appendContent(content);
        
    boost::asio::async_read(sock, respBuffer,
                            boost::asio::transfer_at_least(1),
                            [this, context] (const ErrorCode &err,
                                             std::size_t /* bytes_transferred */)
                            {
                                handleReadBody(err, context);
                            });
}
