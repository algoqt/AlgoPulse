#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <vector>
#include <unordered_map>

namespace requests {

class Response
{
public:
    using String     = std::string;
    using StringList = std::vector<String>;
    using StringMap  = std::unordered_map<String, String>;
    
    Response(const String& statuLine,const String &headers, const String &content);

    unsigned short statusCode() const
    {
        return statusCode_;
    }

    const String &content() const
    {
        return content_;
    }

    const String &statusMessage() const
    {
        return statusMessage_;
    }

    const StringMap &headers() const
    {
        return headers_;
    }   

    StringMap &headers()
    {
        return headers_;
    }
    
private:
    void splitStatusLine(const String &statusLine);

    void splitHeader(const String &header);
    
    String                               headersStr_;
    String                               content_;
    String                               version_;
    String                               statusMessage_;
    unsigned short                       statusCode_;        
    StringMap                            headers_;
};

} // namespace requests

#endif /* RESPONSE_H */
