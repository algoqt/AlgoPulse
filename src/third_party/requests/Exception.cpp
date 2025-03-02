#include <requests/Exception.hpp>

using namespace requests;

Exception::Exception(const std::string &message) 
    : message_(message)
{
}
 
Exception::Exception(std::string &&message)
    : message_(std::move(message))
{        
}
    
const char *Exception::what() const noexcept 
{
    return message_.c_str();
}

