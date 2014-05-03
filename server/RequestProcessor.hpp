#ifndef _REQUESTPROCESSOR_HPP_
#define _REQUESTPROCESSOR_HPP_
#include <string>

namespace VideoMatch
{


class VideoDB;

/* A wrapper of VideoDB, has json parser to make request string into function call */
class RequestProcessor
{
    static VideoDB *vdb_;

public:
    static void SetVideoDB(VideoDB *vdb);
    /* query by frames, add new video are called by HTTP Post, 
       'request' stands for posted data */
    static void Process(const std::string& request, std::string& reply);

    /* return status of VDB */
    static void Info(std::string& reply);
    
    /* query duplicate video by key, the 'key' has to exist in VDB */
    static void Query(const std::string& key, std::string& reply, bool plain);

    /* save the snapshot of current state into a file */
    static void SaveDB();
};


}


#endif

