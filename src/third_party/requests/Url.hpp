#ifndef URL_H
#define URL_H

#include <string>
#include <unordered_map>

namespace requests {

class Url
{
public:
    using String     = std::string;
    using StringMap  = std::unordered_map<String, String>;
    
    Url(const String &url);

    void addQueries(const StringMap &params);
    
    bool hasQueries() const;

    String pathAndQueries() const;

    const String &host() const;

    const String &port() const;
    
    const String &schema() const;

    const String &path() const;

    const String &queries() const;

    String toString() const;
    
private:
    String         url_;
    String         schema_;
    String         host_;
    String         port_;
    String         path_;
    String         queries_;    
};

std::ostream &operator<<(std::ostream &os, const Url &url);
    
} // namespace requests

#endif /* URL_H */
