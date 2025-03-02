#include <requests/Utils.hpp>

#include <iostream>
#include <iomanip>

std::vector<std::string> requests::splitString(const std::string &str, const std::string &separator)
{
    std::vector<std::string> results;

    std::size_t first = 0, last = 0;

    while (first < str.size())
    {
        last = str.find(separator, first);
        if (last == std::string::npos)
        {
            results.push_back(str.substr(first));
            break;
        }
        if (last != first)
        {
            results.push_back(str.substr(first, last - first));            
        }
        first = last + separator.size();
    }

    return results;    
}

std::vector<std::string> requests::splitString(const std::string &str, const std::string &separator, std::size_t splitTimes)
{
    if (splitTimes == 0)
    {
        return { str };
    }

    std::size_t first = 0, last = 0, times = 0;
    std::vector<std::string> results;    

    while (first < str.size())
    {
        last = str.find(separator, first);
        if (last == std::string::npos)
        {
            results.push_back(str.substr(first));
            break;
        }
        if (last != first)
        {
            results.push_back(str.substr(first, last - first));            
        }
        
        first = last + separator.size();

        if (++times == splitTimes)
        {
            results.push_back(str.substr(first));
            break;
        }
    }

    return results;    
}

std::string requests::bufferToString(boost::asio::streambuf &buffer)
{
    auto size = buffer.size();
    auto data = buffer.data();

    std::string str(boost::asio::buffers_begin(data), boost::asio::buffers_begin(data) + size);
    buffer.consume(size);
    return str;
}

std::string requests::join(const std::string &separator, const std::vector<std::string> &strs)
{
    std::string result;

    for (std::size_t i = 0; i < strs.size(); ++i)
    {
        result.append(strs[i]);
        if (i + 1 < strs.size())
        {
            result.append(separator);
        }
    }
    
    return result;
}

static std::string encode(const std::string &str)
{
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (auto c: str)
    {
        if (std::isalnum(c) || c  == '-' || c == '_' || c == '.' || c == '~')
        {
            escaped << c;
            continue;
        }
        escaped << '%' << std::setw(2) << int((unsigned char) c);
    }
    return escaped.str();
};

std::string requests::urlEncode(const std::unordered_map<std::string, std::string> &params)
{   
    std::vector<std::string> querys;
    
    for (const auto &pair: params)
    {
        auto key = encode(pair.first);
        auto value = encode(pair.second);
        
        querys.push_back(key + "=" + value);
    }
    
    return join("&", querys);
}
