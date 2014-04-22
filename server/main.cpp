#include <http_server.hpp>
#include <VideoDB.hpp>
#include <Log.hpp>
#include <RequestProcessor.hpp>
#include <cstdio>
#include <ctime>
#include <strings.h>
#include <getopt.h>


using namespace http_server;


static int my_handler(HttpConnPtr conn)
{
    using VideoMatch::RequestProcessor;
    std::string body;
    
    if (conn->req_type() == HTTP_GET) {
        if (conn->path() == "/save") 
            RequestProcessor::SaveDB();
    } else if (conn->req_type() == HTTP_POST) {
        if (conn->in_threadpool()) {
            RequestProcessor::Process(conn->post_data(), body);
        } else {
            /* all requests about match engine should be processed in thread pool */
            return HTTP_SWITCH_THREAD;
        }
    }

    conn->set_body(body);
    conn->set_header("Server", "Video Match Server 1.0");
    conn->set_header("Content-Type", "text/html");
    return HTTP_200;
}

static void print_usage(const char *sexec)
{
    printf("Usage: %s [opts]\n"
           "\t-p --port <port_number> [default 8964]        the server port\n"
           "\t-d --dir <path> [default ./]                  the db file load/save directory\n"
           "\t-l --log-file <filename> [default time.txt]   the log file name\n"
           "\t-L --log-level <level> [default info]         the log level, one in [debug|info|error]\n"
          , sexec);
}

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

    boost::asio::io_service io_service;
    VideoMatch::RequestProcessor::SetVideoDB(&video_db);
    http_server::HttpServer http_server(io_service, port, &my_handler);
    io_service.run();

    return 0;
}


