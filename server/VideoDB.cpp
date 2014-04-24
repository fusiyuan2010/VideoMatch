#include <VideoDB.hpp>
#include <Log.hpp>
#include <algorithm>
#include <utility>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;


class TimeCounter
{
public:
    TimeCounter() {reset();}
    ~TimeCounter() {}
    void reset() { gettimeofday(&_start, NULL); }
    long GetTimeMilliS() {return get_interval()/1000;}
    long GetTimeMicroS() {return get_interval();}
    long GetTimeS() {return get_interval()/1000000;}
private:
    long get_interval()
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        return (long)(now.tv_sec - _start.tv_sec)*1000000
            + now.tv_usec - _start.tv_usec;
    }
    struct timeval _start;
};


namespace VideoMatch
{

static int bit1_table[256];
static pthread_once_t bit1_table_inited = PTHREAD_ONCE_INIT;
static const uint64_t EMPTY_FRAME = 0;

static void make_bit1_table(void)
{
    for(unsigned int i = 0; i < 255; i++) {
        int count = 0;
        unsigned int k = i;
        for(int j = 0; j < 8; j++) {
            if (k & 0x1) 
                count++;
            k >>= 1;
        }
        bit1_table[i] = count;
    }
}

static inline int bit1count(uint64_t i) 
{
    unsigned char *c = (unsigned char *)&i;
    int ret = 0;
    for(int i = 0; i < 8; i++) 
        ret += bit1_table[c[i]];
    return ret;
}

VideoDB::VideoDB(const string& db_path)
    : db_path_(db_path)
{
    (void) pthread_once(&bit1_table_inited, make_bit1_table);
}


VideoDB::~VideoDB() 
{
}

int VideoDB::get_candidates1(const vector<uint64_t>& frames, vector<DataItem*>& result) const
{
    //lock_guard<mutex> lock(mutex_);
    mutex_.lock();

    for(const auto k : frames) {
        if (k == EMPTY_FRAME)
            continue;
        uint32_t k2 = key_shorten(k);
        auto it = table_.find(k2);
        if (it == table_.end())
            continue;

        KeyBlock *kb = it->second;
        if (kb == nullptr)
            continue;

        for(auto &i : *kb) 
            if (i.first == k) {
                result.push_back(i.second);
                i.second->inc_ref();
            }
    }
    mutex_.unlock();

    /* merge the result  */
    sort(result.begin(), result.end());
    auto it = unique(result.begin(), result.end());
    result.resize(distance(result.begin(), it));
    return (int)result.size();
}

/* 
   Check candidate algorithm:

   CMF denotes Completely Match Frame, means the frames that have the same hash value.
   MDF denotes Minimum Difference Frame, 
   means the corresponding frame that has the min xor1 bits in the other item.

    1. Check CMF index ranges, 
       measure its coverage among the whole range of base item and comparing item.

    2. (Or maybe only if 1's result not good enough) Check MDF,
       measure average min diff bits count (check only some samples, like 50 at most),
       and the order of MDF in the other video.


*/
/* find MDF from a range of frames in base */
static inline int diff_bits(uint64_t key1, uint64_t key2)
{
    return bit1count(key1 ^ key2);
}

static int min_diff_bits(uint64_t key, const vector<uint64_t>& base, int start, int end, int& pos)
{
    int md = 64;
    for(int i = start; i < end; i++) {
        uint64_t diff = key ^ base[i];
        int d = bit1count(diff);
        if (d < md) {
            md = d;
            pos = i;
        }
    }
    return md;
}

/* not interchangeable, the result depends on the arguments' order,
   but it does not matters much */
double VideoDB::check_candidate(DataItem *data_item1, const DataItem& data_item2) const
{
    /* if no CMF found, decide how many frames to skip to check MDF, 
       since finding MDF is O(n), it's not good to check every frame for MDF */
    static const int SKIP_SPLIT_PARTS = 32;
    static const int CHECK_BITS = 8;
    static const int STOP_CHECK_BITS = 16;
    static const int GOOD_BITS = 4;

    unordered_multimap<uint64_t, int> base_frames;
    const auto& cf = data_item2.frames_;
    const auto& bf = data_item1->frames_;

    LOG_DEBUG("Comparing Current vs %s", data_item1->name_.c_str());
    TimeCounter tc;

    /* state of each frame :
       255 - no match
       0 - 63 diff bits;
       
    */
    vector<uint8_t> bmark(bf.size(), (uint8_t)255); 
    vector<uint8_t> cmark(cf.size(), (uint8_t)255); 
    
    for(size_t i = 0; i < bf.size(); i++) 
        base_frames.insert(make_pair(bf[i], (int)i));


    int skip_itvl = cf.size() / SKIP_SPLIT_PARTS; // 0 also works
    auto check_range = [&](int cpos, int bpos) {
        /* when found a CMF or MDF, 
           check ahead and backward to see how much frames matched around here */
        for(int j = 1; cpos + j < (int)cf.size() && bpos + j < (int)bf.size(); j++) {
            int d = diff_bits(cf[cpos + j], bf[bpos + j]);
            if (d > STOP_CHECK_BITS)
                break;
            if (d < cmark[cpos + j])
                cmark[cpos + j] = d;
            if (d < bmark[bpos + j])
                bmark[bpos + j] = d;
        }

        for(int j = 1; cpos - j >= 0 && bpos - j >= 0; j++) {
            int d = diff_bits(cf[cpos - j], bf[bpos - j]);
            if (d > STOP_CHECK_BITS)
                break;
            if (d < cmark[cpos - j])
                cmark[cpos - j] = d;
            if (d < bmark[bpos - j])
                bmark[bpos - j] = d;
        }
    };

    int skipped = 0;
    for(size_t i = 0; i < cf.size(); i++) {
        if (cmark[i] < GOOD_BITS)
            continue;

        if (base_frames.count(cf[i]) > 0) {
            /* CMF found */
            cmark[i] = 0;
            auto range = base_frames.equal_range(cf[i]);
            for(auto it = range.first; it != range.second; it++) {
                if (bmark[i] < GOOD_BITS) {
                    bmark[it->second] = 0;
                    continue;
                }
                bmark[it->second] = 0;
                check_range(i, it->second);
            }
            continue;
        }

        if (++skipped < skip_itvl) 
            continue;
        skipped = 0;

        int mdf_pos = 0;  
        int diff =  min_diff_bits(cf[i], bf, 0, bf.size(), mdf_pos);
        if (diff > CHECK_BITS)
            continue;
        cmark[i] = bmark[mdf_pos] = diff;
        check_range(i, mdf_pos);
    }

    /* get score from overlapped frames, based on their ranges and avg diff bits */
    /* CUT_RATIO means the begining and the end of video does not count */
    static const double CUT_RATIO = 0.00;
    int total_diff_bits = 0, cnt = 0;
    double score1, score2;
    for(size_t i = cf.size() * CUT_RATIO * 100 / 100 ; i < cf.size() * (1 - CUT_RATIO) * 100 / 100; i++) {
        if (cmark[i] != 255) {
            total_diff_bits += cmark[i];
            cnt++;
        }
    }

    score1 = (((double)16 - total_diff_bits / (cnt + 1)) / 16) * (cnt / (cf.size() * (1 - 2 * CUT_RATIO) + 1));

    total_diff_bits = 0; cnt = 0;
    for(size_t i = bf.size() * CUT_RATIO * 100 / 100 ; i < bf.size() * (1 - CUT_RATIO) * 100 / 100; i++) {
        if (bmark[i] != 255) {
            total_diff_bits += bmark[i];
            cnt++;
        }
    }
    score2 = (((double)16 - total_diff_bits / (cnt + 1)) / 16) * (cnt / (bf.size() * (1 - 2 * CUT_RATIO) + 1));

    LOG_DEBUG("Time consumed %ld us, score1 : %f, score2: %f", tc.GetTimeMicroS(), score1, score2);
    return score1 * score2;
}

/* File format:
   int video_num;
   int keyblocknum;

   [int vnamelen, char vname[vnamelen+1], 
   int framecount, uint64_t framehash] * video_num,
   
   [uint32_t keyblockhash,
    int keyblocksize, 
    [uint64_t hash, int vkeylen, char vkeyval[vkeylen+1]] * keyblocksize] * table_size

*/
int VideoDB::Load()
{
    char fn[255];
    int fd;
    char *s, *sbase;
    off_t fsize;
    int db_size, kb_num;
    lock_guard<mutex> lock(mutex_);
    
    snprintf(fn, 255, "%s/videomatch_db.bin", db_path_.c_str());
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
        LOG_INFO("No db file found: %s", fn);
        goto READLOG;
    }
    fsize = lseek(fd, 0, SEEK_END);
    s = sbase = (char*)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    db_size = *(int*)s; s += sizeof(int);
    kb_num = *(int*)s; s += sizeof(int);
    for(int i = 0; i < db_size; i++) {
        /* video name */
        int vnlen = *(int*)s; s += sizeof(int);
        string vn(s);  s += vnlen + 1;
        /* create video object and read frame hashes */
        DataItem *di = new DataItem(vn);
        int fc = *(int*)s; s += sizeof(int);
        for(int j = 0; j < fc; j++) {
            di->Push(*(uint64_t*)s);
            s += sizeof(uint64_t);
        }
        db_.insert(make_pair(vn, di));
    }

    for(int i = 0; i < kb_num; i++) {
        KeyBlock *kb = new KeyBlock();
        uint32_t kbhash = *(uint32_t*)s; s += sizeof(uint32_t);
        int kb_size = *(int*)s; s += sizeof(int);
        for(int j = 0; j < kb_size; j++) {
            uint64_t hash = *(uint64_t*)s; s += sizeof(uint64_t);
            int vnlen = *(int*)s; s += sizeof(int);
            string vn(s);  s += vnlen + 1;
            auto it = db_.find(vn);
            if (it == db_.end()) {
                LOG_ERROR("Load DB error, no video named [ %s ] found", vn.c_str());
                continue;
            }
            DataItem *di = it->second;
            kb->push_back(make_pair(hash, di));
        }
        table_.insert(make_pair(kbhash, kb));
    }
    
    munmap(sbase, fsize);
    close(fd);
    LOG_INFO("Load from db file done");

READLOG:
    snprintf(fn, 255, "%s/videomatch_log.bin", db_path_.c_str());
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
        LOG_INFO("No log file found: %s", fn);
        goto RET;
    }
    fsize = lseek(fd, 0, SEEK_END);
    s = (char*)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);

    munmap(s, fsize);
    close(fd);

    LOG_INFO("Load from log file done");
RET:
    return 0;
}

int VideoDB::Save()
{
    static const int BS = 8192;
    char block[BS];
    char *s = block;
    char fn[255], fn2[255];
    snprintf(fn, 255, "%s/tmp_videomatch_db.bin", db_path_.c_str());
    snprintf(fn2, 255, "%s/videomatch_db.bin", db_path_.c_str());
    int fd = open(fn, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        LOG_ERROR("Save DB, %s open failed, [%s]",
                fn, strerror(errno));
        return -1;
    }

    /* block write boundary check */
    auto check_flush = [&](int i){
        if (BS - (s - block) < i) {
            int size = s - block;
            int ret = write(fd, block, size);
            if (ret != size) {
                LOG_ERROR("Exception: write failed ret %d while writing %d",
                        ret, size);
            }
            s = block;
        }
    };

    lock_guard<mutex> lock(mutex_);

    *(int *)s = (int)(db_.size()); s += sizeof(int);
    *(int *)s = (int)(table_.size()); s += sizeof(int);

    /* write items in db_ */
    for(const auto &d : db_) {
        d.second->inc_ref();
        check_flush(sizeof(int) + d.first.size() + 1
                + sizeof(int));
        *(int *)s = (int)(d.first.size()); s += sizeof(int);
        memcpy(s, d.first.c_str(), d.first.size()); s += d.first.size();
        *s++ = '\0';
        *(int *)s = (int)(d.second->frames_.size()); s += sizeof(int);
        for(const uint64_t f : d.second->frames_) {
            check_flush(sizeof(uint64_t));
            *(uint64_t*)s = f; s += sizeof(uint64_t);
        }
        d.second->dec_ref();
    }

    /* write items in table_ */
    for(const auto &d : table_) {
        check_flush(sizeof(uint32_t) + sizeof(int));
        *(uint32_t*)s = d.first; s += sizeof(uint32_t);
        *(int*)s = (int)(d.second->size()); s += sizeof(int);
        /* write items in a keyblock */
        for(const auto &i : *(d.second)) {
            check_flush(sizeof(uint64_t) + sizeof(int) + 
                    i.second->name_.size() + 1);
            *(uint64_t*)s = i.first; s += sizeof(uint64_t);
            *(int *)s = (int)(i.second->name_.size()); s += sizeof(int);
            memcpy(s, i.second->name_.c_str(), i.second->name_.size()); s += i.second->name_.size();
            *s++ = '\0';
        }
    }
    
    check_flush(BS + 1);
    fdatasync(fd);
    close(fd);
    rename(fn, fn2);

    /* write succeed, clear log */
    snprintf(fn, 255, "%s/videomatch_log.bin", db_path_.c_str());
    unlink(fn);
    return 0;
}

int VideoDB::Add(const DataItem& data_item)
{
    lock_guard<mutex> lock(mutex_);
    auto it = db_.find(data_item.name_);
    if (it != db_.end()) {
        LOG_INFO("Video %s exists", data_item.name_.c_str());
        return -1;
    }

    DataItem *di = new DataItem(data_item);
    db_.insert(make_pair(di->name_, di));
    for(const auto &k : di->frames_) {
        if (k == EMPTY_FRAME)
            continue;
        KeyBlock *kb;
        uint32_t k2 = key_shorten(k);
        auto it = table_.find(k2);
        if (it == table_.end()) {
            kb = new KeyBlock;
            table_.insert(make_pair(k2, kb));
        } else
            kb = it->second;
        
        kb->push_back(make_pair(k, di));
    }
    LOG_INFO("Video %s added, %d frames, KeyBlock size: %d",
            data_item.name_.c_str(), data_item.frames_.size(), table_.size());
    return 0;
}

int VideoDB::Query(const string& video_name, DataItem& data_item) const
{
    lock_guard<mutex> lock(mutex_);
    auto it = db_.find(video_name);
    if (it == db_.end())
        return -1;
    data_item = *(it->second);
    return 0;
}

int VideoDB::Query(const DataItem& data_item, vector<pair<string, double>>& result) const
{
    static const double threshold = 0.09;
    vector<DataItem *> candidates;
    TimeCounter tc;

    int cand_num = get_candidates1(data_item.frames_, candidates);
    LOG_DEBUG("level1 candidate num: %d", cand_num);
    for(auto i : candidates) {
        double score = check_candidate(i, data_item);
        LOG_DEBUG("Checked candidate %s, score %f", i->name_.c_str(), score);
        if (score > threshold)
            result.push_back(make_pair(i->name_, score));
        i->dec_ref();
    }
    sort(result.begin(), result.end(), 
            [](const pair<string, double>& p1, const pair<string, double>& p2) {
                return p1.second > p2.second;
            });
    LOG_DEBUG("Query time: %ld ms(%d cand, %d result)", tc.GetTimeMilliS(), cand_num, (int)result.size());
    return (int)result.size();
}

int VideoDB::Remove(const string& video_name)
{
    lock_guard<mutex> lock(mutex_);
    /*
    auto it = db_.find(video_name);
    if (it == db_.end())
        return -1;
    DataItem* di = it->second;
    for(const auto& f : di->frames) {
    }

    di.destroy();
    */
    return 0;
}


}


