#include <ImageProcessor.hpp>
#include <Requester.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void print_usage(const char *sexec)
{
    printf("Usage: %s [opts]\n"
           "\t-p --port <port_number> [default 8964]        the server port\n"
           "\t-d --dir <path> [default ./]                  the db file load/save directory\n"
           "\t-l --log-file <filename> [default time.txt]   the log file name\n"
           "\t-L --log-level <level> [default info]         the log level, one in [debug|info|error]\n"
          );
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
    };

    int long_index = 0;
    int opt;
    while((opt = getopt_long(argc, argv, "", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'd':
                dir = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
                log_file = optarg;
            case 'l':
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


  CURL *curl;
  CURLcode res;

  static const char *postthis="moo mooo moo moo";

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postthis);

    /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by
       itself */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(postthis));

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  return 0;
}
