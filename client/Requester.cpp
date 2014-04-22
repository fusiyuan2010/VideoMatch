#include <Requester.hpp>
#include <json/json.h>

using namespace std;

/*
    Json format: 
Request:

    type: string "[add | query_duplicate | query_video | remove]"
    name: string  (when add | query_video | remove)
    frames: array of UInt64 (when add | query_duplicate)
    
Replay:
    
    code: int 0 or -1;
    msg: string, if code not equal 0

*/

namespace VideoMatch {

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    string& data = *(string*)stream;
    data += string((char*)ptr, size * nmemb);
    return size * nmemb;
}

Requester::Requester()
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
}

Requester::~Requester()
{
    curl_easy_cleanup(curl_handle);
}

int Requester::InitUrl(const string& url)
{
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    return 0;
}

int Requester::Add(const string& name, const vector<uint64_t>& frames, string &reply) 
{
    Json::StyledWriter writer;
    Json::Value v;

    v["type"] = "add";
    v["name"] = name;
    for(size_t i = 0; i < frames.size(); i++) {
        v["frames"][(int)i] = (Json::Value::UInt64)(frames[i]);
    }
    string req_str = writer.write(v);
    
    printf("Req:\n%s\n", req_str.c_str());
    reply = "";
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, req_str.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &reply);
    curl_easy_perform(curl_handle);

    return 0;
}

int Requester::Query(const vector<uint64_t>& frames, string& reply)
{
    Json::StyledWriter writer;
    Json::Value v;

    v["type"] = "query_duplicate";
    for(size_t i = 0; i < frames.size(); i++) {
        v["frames"][(int)i] = (Json::Value::UInt64)(frames[i]);
    }
    string req_str = writer.write(v);
 
    reply = "";
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, req_str.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &reply);
    curl_easy_perform(curl_handle);
    return 0;
}

}

