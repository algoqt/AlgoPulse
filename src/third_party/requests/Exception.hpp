#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <exception>
#include <string>

namespace requests {
    
class Exception : public std::exception
{
public:
    explicit Exception(const std::string &message);

    explicit Exception(std::string &&message);
    
    virtual const char *what() const noexcept override;
    
private:
    std::string message_;
};

} // namespace requests

#endif /* EXCEPTION_H */
