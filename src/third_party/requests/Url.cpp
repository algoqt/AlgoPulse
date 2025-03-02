#include <requests/Exception.hpp>
#include <requests/Url.hpp>
#include <requests/Utils.hpp>

using namespace requests;

Url::Url(const String &url)
    : url_(url),
      port_("80")
{
    auto tokens = splitString(url, "://");        
    if (tokens.size() != 2)
    {
        throw Exception("Invalid URL - no schema supplied: " + url_);                        
    }
        
    schema_ = std::move(tokens[0]);

    auto other = splitString(tokens[1], "/", 1);

    auto hostAndPort = splitString(other[0], ":");
    if (hostAndPort.size() != 1 && hostAndPort.size() != 2)
    {
        throw Exception("Invalid Host and Port: " + other[0]);
    }        

    host_ = std::move(hostAndPort[0]);

    if (hostAndPort.size() == 2)
    {
        port_ = std::move(hostAndPort[1]);
    }

    path_ = "/";

    if (other.size() == 2)
    {
        auto pathQueries = splitString(other[1], "?", 1);
            
        if (pathQueries.size() == 1)
        {
            queries_ = std::move(pathQueries[0]);
        }
        else
        {
            path_.append(pathQueries[0]);
            queries_ = std::move(pathQueries[1]);
        }
    }
}

void Url::addQueries(const StringMap &params)
{
    auto queries = urlEncode(params);
    if (hasQueries())
    {
        queries_.append("&").append(queries);
    }
    else
    {
        queries_ = std::move(queries);
    }
}
    
bool Url::hasQueries() const
{
    return !queries_.empty();        
}

Url::String Url::pathAndQueries() const
{
    if (hasQueries())
    {
        return path_ + "?" + queries_;
    }
    return path_;
}
    
const Url::String &Url::host() const
{
    return host_;
}

const Url::String &Url::port() const
{
    return port_;
}
    
const Url::String &Url::schema() const
{
    return schema_;
}

const Url::String &Url::path() const
{
    return path_;
}

const Url::String &Url::queries() const
{
    return queries_;
}

Url::String Url::toString() const
{
    return schema() + "://" + host() + pathAndQueries();
}

std::ostream &requests::operator<<(std::ostream &os, const Url &url)
{
    os << url.toString() << std::endl;
    return os;
}
