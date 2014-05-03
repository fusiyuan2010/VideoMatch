#ifndef _VIDEODB_HPP_
#define _VIDEODB_HPP_
#define _VIDEOSTORE_H_

#include <stdint.h>
#include <atomic>
#include <vector>
#include <utility>
#include <string>
#include <unordered_map>
#include <mutex>

namespace VideoMatch
{




class VideoDB 
{

public:
    class DataItem
    {
        friend class VideoDB;
        /* do not need init, 
         will be set in VideoDB::add_frame_to_index */
        size_t uniq_frm_cnt;
        std::string name_;
        std::vector<uint64_t> frames_;

        /* reference count is to avoid lock for too long time in VideoDB,
           However since Remove() is not implemented in VideoDB,
           current refcount is useless */
        mutable std::atomic<int> ref_;
        bool deleted_;

        void inc_ref() 
        {
            ref_++;
        }

        void dec_ref()
        {
            if (--ref_ == 0 && deleted_) {
                delete this;
            }
        }

        void destroy()
        {
            deleted_ = true;
            if (ref_ == 0) 
                delete this;
        }

    public:
        DataItem(const std::string& name)
            : name_(name), ref_(0), deleted_(false) 
        {
        }

        DataItem(const DataItem& data_item)
            : name_(data_item.name_), 
            frames_(data_item.frames_),ref_(0), deleted_(false)
        {
        }

        DataItem& operator=(const DataItem& data_item) 
        {
            name_ = data_item.name_;
            frames_ = data_item.frames_;
            ref_ = 0;
            deleted_ = false;
            return *this;
        }

        ~DataItem() = default;

        void Push(const uint64_t frame)
        {
            frames_.push_back(frame);
        }
    };


    //////////////////////////////////////////////////////////
private:
    static const int HSIZE_BITS = 20;

    typedef std::vector<std::pair<uint64_t, DataItem*>> KeyBlock;
    std::unordered_map<std::string, DataItem*> db_;

    /* index for frames*/
    std::unordered_map<uint32_t, KeyBlock*> table_;
    mutable std::mutex mutex_;

    std::string db_path_;

    int get_candidates1(const std::vector<uint64_t>& frames, std::vector<DataItem*>& result) const;
    double check_candidate(DataItem *data_item1, const DataItem& data_item2) const;
    uint32_t key_shorten(uint64_t raw) const
    {
        /* 2 Million (21bits) slots*/
        return (raw >> (64 - HSIZE_BITS));
    }

    void add_frames_to_index(DataItem *di);

public:
    VideoDB(const std::string& db_path);
    ~VideoDB();

    int Load();
    int Save();

    int Add(const DataItem& data_item);
    int Query(const std::string& video_name, DataItem& data_item) const ;
    int Query(const DataItem& data_item, std::vector<std::pair<std::string, double>>& result) const;
    int Remove(const std::string& video_name);

    int Count() const;
    int FramesCount() const;
    int FrameTableSize() const;
};


}

#endif

