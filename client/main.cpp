#include <ImageProcessor.hpp>
#include <Requester.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <getopt.h>
#include <vector>

using namespace std;

static void print_usage(const char *sexec)
{
    printf("Usage: %s [opts]\n"
           "\t-r --req [add|query] [required]                 type of request\n"
           "\t-a --addr <url> [required]                      the server address\n"
           "\t-n --name <string> [required when add]          name(key) of the video to add\n"
           "\t-d --dir <path> [required]                      directory that frames stored\n"
          , sexec);
}

int main(int argc, char *argv[])
{

    const char *dir = nullptr;
    const char *name = nullptr;
    const char *url = nullptr;
    const char *req = nullptr;

    static struct option long_options[] = {
        {"req",     required_argument, 0,  'r' },
        {"addr",     required_argument, 0,  'a' },
        {"name",     required_argument, 0,  'n' },
        {"dir",     required_argument, 0,  'd' },
        { 0, 0, 0, 0}
    };

    int long_index = 0;
    int opt;
    while((opt = getopt_long(argc, argv, "r:a:n:d:", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'd':
                dir = optarg;
                break;
            case 'r':
                req = optarg;
                break;
            case 'a':
                url = optarg;
                break;
            case 'n':
                name = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
                break;
        }
    }

    if (req == nullptr) {
        print_usage(argv[0]);
        return 1;
    }

    if (url == nullptr || dir == nullptr) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcasecmp(req, "add") == 0 && name == nullptr) {
        print_usage(argv[0]);
        return 1;
    }

    VideoMatch::Requester requester;
    requester.InitUrl(url);

    vector<uint64_t> frames;
    struct dirent **filelist;
    int fnum = scandir(dir, &filelist, 0, alphasort);
    for(int i = 0; i < fnum; i++) {
        char filename[128];
        uint64_t hresult;
        int ret;

        const char *sfx = filelist[i]->d_name + strlen(filelist[i]->d_name) - 4;
        if (strcmp(sfx, ".jpg"))
            continue;
        snprintf(filename, 128, "%s/%s", dir, filelist[i]->d_name);

        ret = VideoMatch::GetHashCode(filename, hresult);
        if (ret < 0) 
            fprintf(stderr, "Analyze image %s failed\n", filename);
        else
            frames.push_back(hresult);

        fprintf(stderr, "\r%d / %d\t\t\t\t\t", i, fnum);
        free(filelist[i]);
    }
    printf("\n");
    free(filelist);


    printf("Requesting...");
    string reply;
    if (strcasecmp(req, "add") == 0)
        requester.Add(name, frames, reply);
    if (strcasecmp(req, "query") == 0)
        requester.Query(frames, reply);
    printf("done\n");

    printf("%s\n", reply.c_str());

    return 0;
}

