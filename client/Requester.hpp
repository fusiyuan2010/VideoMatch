#ifndef _REQUESTER_HPP_
#define _REQUESTER_HPP_
#include <stdint.h>
#include <string>
#include <vector>
#include <curl/curl.h>

namespace VideoMatch {

class Requester
{
    CURL *curl_handle;
public:
    Requester();
    ~Requester();
    int InitUrl(const std::string& url);
    int Add(const std::string& name, const std::vector<uint64_t>& frames, std::string &reply);
    int Query(const std::vector<uint64_t>& frames, std::string& reply);
};
    


}

#endif

