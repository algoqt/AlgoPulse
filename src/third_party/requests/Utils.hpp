#ifndef UTILS_H
#define UTILS_H

#include <boost/asio.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace requests {

std::vector<std::string> splitString(const std::string &str, const std::string &separator);
    
std::vector<std::string> splitString(const std::string &str, const std::string &separator, std::size_t splitTimes);

std::string bufferToString(boost::asio::streambuf &buffer);

std::string join(const std::string &separator, const std::vector<std::string> &strs);
    
std::string urlEncode(const std::unordered_map<std::string, std::string> &params);
    
} // namespace requests

#endif /* UTILS_H */
