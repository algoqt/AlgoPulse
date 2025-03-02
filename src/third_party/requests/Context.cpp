#include <requests/Context.hpp>
#include <requests/Utils.hpp>
#include <requests/Exception.hpp>
#include <iostream>
using namespace requests;

Context::Context(IOService &service, const Url &url, Method method, const StringMap &data)
    : Context(service, url, method, data, UserCallback(), ErrorCallback())
{        
}        
    
Context::Context(IOService &service, const Url &url, Method method, const StringMap &data, UserCallback cb, ErrorCallback errorCb)
    : sock_(service),
      url_(url),
      callback_(cb),
      errorCallback_(errorCb),
      method_(method)
{
    std::ostream reqStream(&requestBuff_);
    responseBuff_.prepare(1024);
    if (method_ == Method::Get)           
    {
        url_.addQueries(data);
        
        reqStream << "GET " << url_.pathAndQueries() << " HTTP/1.0\r\n";
        reqStream << "Host: " << url_.host() << "\r\n";
        reqStream << "Accept: */*\r\n";
        reqStream << "Connection: close\r\n\r\n";

        //std::cout << &requestBuff_ << std::endl;
    }
    else if (method_ == Method::Post)
    {
        auto requestBody = urlEncode(data);
        auto length = std::to_string(requestBody.size());
        
        reqStream << "POST " << url_.path() << " HTTP/1.1\r\n";
        reqStream << "Host: " << url_.host() << "\r\n";
        reqStream << "Accept: */*\r\n";
        reqStream << "Content-Type: application/x-www-form-urlencoded\r\n";
        reqStream << "Content-Length: " << length << "\r\n";
        reqStream << "Connection: close\r\n\r\n";
        
        reqStream << requestBody;
    }
}
