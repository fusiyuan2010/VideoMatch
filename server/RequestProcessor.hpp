#ifndef _REQUESTPROCESSOR_HPP_
#define _REQUESTPROCESSOR_HPP_
#include <string>

namespace VideoMatch
{


class VideoDB;

class RequestProcessor
{
    static VideoDB *vdb_;

public:
    static void SetVideoDB(VideoDB *vdb);
    static void Process(const std::string& request, std::string& reply);
    static void SaveDB();
};


}


#endif

