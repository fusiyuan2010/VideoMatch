#include <VideoDB.hpp>
#include <Log.hpp>
#include <algorithm>
#include <utility>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>



namespace VideoMatch
{


VideoDB::VideoDB(const std::string& db_path)
    : db_path_(db_path)
{
}


VideoDB::~VideoDB() 
{
}

int VideoDB::get_candidates1(const std::vector<uint64_t>& frames, std::vector<DataItem*>& result) const
{
    //std::lock_guard<std::mutex> lock(mutex_);
    mutex_.lock();

    for(const auto k : frames) {
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
    std::sort(result.begin(), result.end());
    auto it = std::unique(result.begin(), result.end());
    result.resize(std::distance(result.begin(), it));
    return (int)result.size();
}

double VideoDB::check_candidate(DataItem *data_item1, const DataItem& data_item2) const
{
    return 0;
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
    char *s;
    off_t fsize;
    int db_size, kb_num;
    
    snprintf(fn, 255, "%s/videomatch_db.bin", db_path_.c_str());
    fd = open(fn, O_RDONLY);
    if (fd < 0) 
        goto READLOG;
    fsize = lseek(fd, 0, SEEK_END);
    s = (char*)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    db_size = *(int*)s; s += sizeof(int);
    kb_num = *(int*)s; s += sizeof(int);
    for(int i = 0; i < db_size; i++) {
        /* video name */
        int vnlen = *(int*)s; s += sizeof(int);
        std::string vn(s);  s += vnlen + 1;
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
            std::string vn(s);  s += vnlen + 1;
            auto it = db_.find(vn);
            if (it == db_.end()) {
                LOG_ERROR("Load DB error, no video named [ %s ] found", vn.c_str());
                continue;
            }
            DataItem *di = it->second;
            kb->push_back(std::make_pair(hash, di));
        }
        table_.insert(make_pair(kbhash, kb));
    }
    
    munmap(s, fsize);
    close(fd);
    LOG_INFO("Load from db file done");

READLOG:
    snprintf(fn, 255, "%s/videomatch_log.bin", db_path_.c_str());
    fd = open(fn, O_RDONLY);
    if (fd < 0)
        goto RET;
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
    int fd = open(fn, O_WRONLY);
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

    *(int *)s = (int)(db_.size()); s += sizeof(int);
    *(int *)s = (int)(table_.size()); s += sizeof(int);

    /* write items in db_ */
    for(const auto &d : db_) {
        d.second->inc_ref();
        check_flush(sizeof(int) + d.first.size() + 1
                + sizeof(int) + d.second->frames_.size() * sizeof(uint64_t));
        *(int *)s = (int)(d.first.size()); s += sizeof(int);
        memcpy(s, d.first.c_str(), d.first.size());
        *s++ = '\0';
        *(int *)s = (int)(d.second->frames_.size()); s += sizeof(int);
        for(const uint64_t f : d.second->frames_) {
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
            memcpy(s, i.second->name_.c_str(), i.second->name_.size());
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = db_.find(data_item.name_);
    if (it != db_.end()) {
        LOG_INFO("Video %s exists", data_item.name_.c_str());
        return -1;
    }

    DataItem *di = new DataItem(data_item);
    db_.insert(std::make_pair(di->name_, di));
    for(const auto &k : di->frames_) {
        KeyBlock *kb;
        uint32_t k2 = key_shorten(k);
        auto it = table_.find(k2);
        if (it == table_.end()) {
            kb = new KeyBlock;
            table_.insert(std::make_pair(k2, kb));
        } else
            kb = it->second;
        
        kb->push_back(std::make_pair(k, di));
    }
    LOG_INFO("Video %s added, %d frames, KeyBlock size: %d",
            data_item.name_.c_str(), data_item.frames_.size(), table_.size());
    return 0;
}

int VideoDB::Query(const std::string& video_name, DataItem& data_item) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = db_.find(video_name);
    if (it == db_.end())
        return -1;
    data_item = *(it->second);
    return 0;
}

int VideoDB::Query(const DataItem& data_item, std::vector<std::pair<std::string, double>>& result) const
{
    static const double threshold = 0;
    std::vector<DataItem *> candidates;
    int cand_num = get_candidates1(data_item.frames_, candidates);
    LOG_DEBUG("level1 candidate num: %d", cand_num);
    for(auto i : candidates) {
        double score = check_candidate(i, data_item);
        LOG_DEBUG("Checked candidate %s, score %f", i->name_.c_str(), score);
        if (score > threshold)
            result.push_back(std::make_pair(i->name_, score));
        i->dec_ref();
    }
    return (int)result.size();
}

int VideoDB::Remove(const std::string& video_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
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


