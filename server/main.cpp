#include <http_server.hpp>
#include <VideoDB.hpp>
#include <Log.hpp>
#include <RequestProcessor.hpp>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <getopt.h>




namespace {

using namespace tws;

int my_handler(Response& resp, const Request& req)
{
    using VideoMatch::RequestProcessor;
    std::string body;
    
    /* 
       support operation:
       GET /info
       GET /save
       GET /exit
       GET /querykey/$key
       POST json to add/query by frames
    */
    auto prefixeq = [](const std::string& base, const std::string& match) {
        if (base.size() > match.size()
                && strncasecmp(base.c_str(), match.c_str(), match.size()) == 0)
            return true;
        return false;
    };
 
    if (req.type() == HTTP_GET) {
        if (req.path() == "/info") {
            /* Show DB info, need not in thread pool */
            RequestProcessor::Info(body);
        } else if (req.path() == "/exit") {
            exit(0);
        } else if (req.path() == "/save" 
                || prefixeq(req.path(), "/querykeyplain/")
                || prefixeq(req.path(), "/querykey/")) {
            if (!req.in_threadpool()) 
                return HTTP_SWITCH_THREAD;

            if (req.path() == "/save") {
                RequestProcessor::SaveDB();
                body = "Done\n";
            } else if (prefixeq(req.path(), "/querykey/")) {
                std::string key = req.path().substr(strlen("/querykey/"));
                RequestProcessor::Query(key, body, false);
            } else if (prefixeq(req.path(), "/querykeyplain/")) {
                std::string key = req.path().substr(strlen("/querykeyplain/"));
                RequestProcessor::Query(key, body, true);
            }
        } else {
            /* show help info */
            body = "Command:\r\n"
                "GET /exit\r\n"
                "GET /info\r\n"
                "GET /save\r\n"
                "GET /querykey/$key\r\n";
        }
    } else if (req.type() == HTTP_POST) {
        if (!req.in_threadpool()) 
            return HTTP_SWITCH_THREAD;

        /* all requests about match engine should be processed in thread pool */
        RequestProcessor::Process(req.postdata(), body);
    }

    resp.set_body(body);
    resp.set_header("Server", "Video Match Server 1.0");
    resp.set_header("Content-Type", "text/html");
    return HTTP_200;
}


void print_usage(const char *sexec)
{
    printf("Usage: %s [opts]\n"
           "\t-p --port <port_number> [default 8964]        the server port\n"
           "\t-d --dir <path> [default ./]                  the db file load/save directory\n"
           "\t-l --log-file <filename> [default time.txt]   the log file name\n"
           "\t-L --log-level <level> [default info]         the log level, one in [debug|info|error]\n"
          , sexec);
}

} //end of namespace

int main(int argc, char *argv[])
{
    const char *dir = "./";
    int port = 8964;
    char default_log_file[255];
    char *log_file = default_log_file;
    LOG_LEVEL log_level = LINFO;

    snprintf(default_log_file, 255, "./%ld.log", time(NULL));
    static struct option long_options[] = {
        {"dir",     required_argument, 0,  'd' },
        {"port",     required_argument, 0,  'p' },
        {"log-file",     required_argument, 0,  'l' },
        {"log-level",     required_argument, 0,  'L' },
        {      0,     0,     0,     0},  
    };

    int long_index = 0;
    int opt;
    while((opt = getopt_long(argc, argv, "d:p:l:L:", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'd':
                dir = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'L':
                if (strcasecmp(optarg,"DEBUG") == 0)
                    log_level = LDEBUG;
                else if (strcasecmp(optarg,"INFO") == 0)
                    log_level = LINFO;
                else if (strcasecmp(optarg,"ERROR") == 0)
                    log_level = LERROR;
                break;
            default:
                print_usage(argv[0]);
                return 1;
                break;
        }
    }

    LogInit(log_file, log_level);

    VideoMatch::VideoDB video_db(dir);
    video_db.Load();

    VideoMatch::RequestProcessor::SetVideoDB(&video_db);
    tws::HttpServer http_server(port, &my_handler, 4);
    http_server.run();

    return 0;
}


