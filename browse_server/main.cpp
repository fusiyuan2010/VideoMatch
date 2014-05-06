#include <http_server.hpp>
#include <cstdio>
#include <ctime>
#include <strings.h>
#include <getopt.h>
#include <map>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>

using namespace http_server;
using namespace std;


namespace {

class VideoManager
{
    struct DataItem
    {
        string name_;
        map<int, string> frames_;
        uint64_t size_;
        DataItem *prev_, *next_;

        DataItem(const string& name)
            : name_(name), size_(0)
        {
        }
    };

    uint64_t cache_size_;
    DataItem *head_, *tail_;

    map<string, DataItem*> data_;
    map<string, string> raw_path_;

    void check_cache_overflow()
    {
        auto get_size = [&]() {
            uint64_t ret = 0;
            for(auto &i : data_)
                ret += i.second->size_;
            return ret;
        };

        while (get_size() > cache_size_ && tail_ != nullptr) {
            DataItem *di = tail_;
            tail_ = tail_->prev_;
            if (head_ == tail_)
                head_ = nullptr;
            if (tail_)
                tail_->next_ = nullptr;
            delete di;
        }
    }

    int load(const string& key) 
    {
        if (raw_path_.count(key) == 0)
            return -1;

        string path = raw_path_[key];
        char tmppath[128];
        snprintf(tmppath, 128, "./browseserver_%d/", rand());
        if (mkdir(tmppath, 0777) < 0)
            return -1;
        
        string cmdstr = "python read.py " + path + " " + tmppath;
        system(cmdstr.c_str());
        
        
        DataItem *di = new DataItem(key);
        struct dirent **filelist;
        int fnum = scandir(tmppath, &filelist, 0, alphasort);
        for(int i = 0; i < fnum; i++) {
            char filename[255];
            const char *sfx = filelist[i]->d_name + strlen(filelist[i]->d_name) - 4;
            if (strcasecmp(sfx, ".jpg"))
                continue;

            snprintf(filename, 255, "%s/%s", tmppath, filelist[i]->d_name);
            FILE *f = fopen(filename, "rb");
            if (!f) {
                free(filelist[i]);
                continue;
            }

            fseek(f, 0, SEEK_END);
            int fsize = ftell(f);
            if (fsize) {
                (void)fseek(f, 0, SEEK_SET);
                char *buf = new char[fsize];
                fread(buf, 1, fsize, f);
                int fid = atoi(filelist[i]->d_name);
                di->frames_[fid] = string(buf, fsize);
                di->size_ += fsize;
            }
            fclose(f);
            free(filelist[i]);
        }
        printf("\n");
        free(filelist);

        cmdstr = string("rm -rf ") + tmppath;
        system(cmdstr.c_str());

        di->prev_ = nullptr;
        di->next_ = head_;
        if (tail_ == nullptr)
            tail_ = di;
        if (head_)
            head_->prev_ = di;
        head_ = di;
        data_[key] = di;
        check_cache_overflow();
        return 0;
    }

public:
    VideoManager(uint64_t cache_size)
        : cache_size_(cache_size),
        head_(nullptr),
        tail_(nullptr)
    {
    }

    int LoadPath(const char *filename) 
    {
        FILE *f = fopen(filename, "rt");
        if (f == nullptr)
            return -1;

        char fn[128];
        while(fgets(fn, 128, f) != nullptr) {
            char *s = fn;
            char *d = nullptr;
            string key;
            for(char *c = fn; *c != '\0'; c++) {
                if (*c == '/')
                    s = c + 1;
                else if (*c == '\n') {
                    *c = '\0';
                    break;
                } else if (*c == '.') {
                    d = c;
                }
            }

            if (d && strcmp(d, ".pickle") == 0) {
                key.assign(s, d - s);
                raw_path_[key] = fn;
            }
        }

        printf("%d Video path added\n", (int)raw_path_.size());
        fclose(f);
        return 0;
    }

    int LookupSize(const string& vname) 
    {
        auto it1 = data_.find(vname);
        if (it1 == data_.end()) {
            if (load(vname) < 0)
                return -1;
            it1 = data_.find(vname);
        }
        return it1->second->frames_.size();
    }

    int Lookup(const string& vname, int frameid, string& content) 
    {
        auto it1 = data_.find(vname);
        if (it1 == data_.end()) {
            if (load(vname) < 0)
                return -1;
            it1 = data_.find(vname);
        }
        
        DataItem *di = it1->second;
        /* Move To Front */
        if (di != head_) {
            if (di == tail_)
                tail_ = di->prev_;
            if (di->next_)
                di->next_->prev_ = di->prev_;
            if (di->prev_)
                di->prev_->next_ = di->next_;
            di->prev_ = nullptr;
            di->next_ = head_;
            head_->prev_ = di;
            head_ = di;
        }

        auto it2 = di->frames_.find(frameid);
        if (it2 == di->frames_.end())
            return -1;
        content = it2->second;
        return 0;
    }

    
};

VideoManager *gvm = nullptr;

/* 
http://xxxxx/img/######/00000#.jpg
http://xxxxx/browse/#######/000000#.html
 */

int my_handler(HttpConnPtr conn)
{
    using namespace std;
    string body;
    vector<string> params;

    string laststr;
    for(auto c : conn->path()) {
        if (c == '/') {
            if (!laststr.empty())
                params.push_back(laststr);
            laststr.clear();
            continue;
        }
        laststr.append(1, c);
    }
    if (!laststr.empty())
        params.push_back(laststr);
    if (params.size() < 2)
        return HTTP_404;
    
    if (params[0] == "browse") {
        int curid;
        char curidstr[32];
        char titlestr[32];
        char prevaddr[32];
        char nextaddr[32];
        if (params.size() == 2)
            curid = 1;
        else
            curid = atoi(params[2].c_str());
        int frm_cnt = gvm->LookupSize(params[1]);
        if (frm_cnt < 0)
            return HTTP_404;
        snprintf(curidstr, 32, "%d", curid);
        snprintf(titlestr, 32, "%d / %d", curid, frm_cnt);
        snprintf(prevaddr, 32, "/browse/%s/%d", params[1].c_str(), curid == 1? 1 : curid - 1);
        snprintf(nextaddr, 32, "/browse/%s/%d", params[1].c_str(), curid == frm_cnt? curid : curid + 1);
        body =  "<html>"
                "<head><title>" + params[1] + "</title></head>"
                "<body><center><h2>" + titlestr + "</h2></center>"
                "<center><img src=\"/image/" + params[1] + "/" + curidstr + "\" usemap=\"#img1\""
                "align=\"middle\" width=600></center>"
                "<map name=\"img1\">"
                "<area shape=\"rect\" coords=\"0,0,300,600\" href=\"" + prevaddr + "\" alt=\"Sun\">"
                "<area shape=\"rect\" coords=\"300,0,600,600\" href=\"" + nextaddr + "\" alt=\"Sun\">"
                "</map></body></html>";

        conn->set_header("Content-Type", "text/html");
    } else if (params[0] == "image") {
        //if (!conn->in_threadpool())
        //    return HTTP_SWITCH_THREAD;
        //printf("querying %s: %d\n", params[1].c_str(), atoi(params[2].c_str()));
        if (gvm->Lookup(params[1], atoi(params[2].c_str()), body) < 0)
            return HTTP_404;
        conn->set_header("Content-Type", "image/jpeg");
    } else {
        return HTTP_404;
    }
    
    conn->set_body(body);
    conn->set_header("Server", "Video Browse Server 1.0");
    return HTTP_200;
}


static void print_usage(const char *sexec)
{
    printf("Usage: %s [opts]\n"
           "\t-p --port <port_number> [default 8964]            the server port\n"
           "\t-l --list <filename> [default ./videolist.txt]    the pickle list file\n"
           "\t-c --cache <size(MB(> [default 128]               cache size in MB\n"
          , sexec);
}

} //end of namespace

int main(int argc, char *argv[])
{
    const char *listfile = "./videolist.txt";
    int port = 8965;
    uint64_t cache_size = 1024;

    static struct option long_options[] = {
        {"list",     required_argument, 0,  'l' },
        {"port",     required_argument, 0,  'p' },
        {"cache",     required_argument, 0,  'c' },
        {      0,     0,     0,     0},  
    };

    int long_index = 0;
    int opt;
    while((opt = getopt_long(argc, argv, "c:p:l:", long_options, &long_index)) != -1) {
        switch(opt) {
            case 'l':
                listfile = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'c':
                cache_size = atoi(optarg) * 1048576ull;
                break;
            default:
                print_usage(argv[0]);
                return 1;
                break;
        }
    }

    gvm = new VideoManager(cache_size);
    if (gvm->LoadPath(listfile) < 0)
        return -1;

    boost::asio::io_service io_service;
    http_server::HttpServer http_server(io_service, port, &my_handler);
    io_service.run();

    return 0;
}


