#include <RequestProcessor.hpp>
#include <TimeCounter.hpp>
#include <VideoDB.hpp>

#include <json/json.h>


namespace VideoMatch
{

VideoDB* RequestProcessor::vdb_ = nullptr;


void RequestProcessor::SetVideoDB(VideoDB *db) 
{
    vdb_ = db;
}


/*
    Json format: 
Request:

    type: string "[add | query_duplicate | query_video | remove]"
    name: string  (when add | query_video | remove)
    frames: array of UInt64 (when add | query_duplicate)
    
Replay:
    
    code: int 0 or -1;
    msg: string, if code not equal 0

*/

void RequestProcessor::Process(const std::string& request, std::string& reply)
{
    //Parse request , Json
    Json::Reader reader;
    Json::StyledWriter writer;
    Json::Value v;
    Json::Value rv;

    auto bad_rpl = [&](const std::string& msg) {
        rv["code"] = (Json::Value::Int)(-1);
        rv["msg"] = msg;
        reply = writer.write(rv);
    };
    
    if (!reader.parse(request.c_str(), v, false)) {
        bad_rpl("Parse failed");
        return;
    }

    if (!v.isMember("type") || !v["type"].isString()) {
        bad_rpl("No 'type' field");
        return;
    }

    const std::string req_type = v["type"].asString();
    if (req_type == "add") {
        if (!v.isMember("name") || !v["name"].isString()) {
            bad_rpl("No 'name' field");
            return;
        }
        if (!v.isMember("frames") || !v["frames"].isArray()) {
            bad_rpl("No 'frames' field");
            return;
        }
        DataItem data_item(v["name"].asString());
        Json::Value& frames = v["frames"];
        int fc = frames.size();
        for(int i = 0; i < fc; i++) {
            data_item.Push(frames[i].asUInt64());
        }
        rv["code"] = vdb_->Add(data_item);
        reply = writer.write(rv);

    } else if (req_type == "query_duplicate") {
         if (!v.isMember("frames") || !v["frames"].isArray()) {
            bad_rpl("No 'frames' field");
            return;
        }
        DataItem data_item("QUERY"); // name actually not required
        Json::Value& frames = v["frames"];
        int fc = frames.size();
        for(int i = 0; i < fc; i++) {
            data_item.Push(frames[i].asUInt64());
        }
        std::vector<std::pair<std::string, double>> result;
        rv["code"] = vdb_->Query(data_item, result);
        for(size_t i = 0; i < result.size(); i++) {
            Json::Value item;
            item["name"] = result[i].first;
            item["score"] = result[i].second;
            rv["result"][(int)i] = item;
        }
        reply = writer.write(rv);
    //} else if (req_type == "query_video") {
    //} else if (req_type == "remove") {
    } else {
        bad_rpl("Bad 'type' field");
        return;
    }
    /* */
    return;
}

void RequestProcessor::SaveDB()
{
    vdb_->Save();
}

void RequestProcessor::Info(std::string& reply)
{
    Json::StyledWriter writer;
    Json::Value rv;

    rv["video_count"] = (Json::Value::Int)(vdb_->Count());
    rv["frames_count"] = (Json::Value::Int)(vdb_->FramesCount());
    rv["frame_table_size"] = (Json::Value::Int)(vdb_->FrameTableSize());
    reply = writer.write(rv);
}

void RequestProcessor::Query(const std::string& key, std::string& reply)
{
    Json::StyledWriter writer;
    Json::Value rv;
    DataItem data_item(""); // name not required
    std::vector<std::pair<std::string, double>> result;

    TimeCounter tc;

    if (vdb_->Query(key, data_item) < 0) {
        rv["code"] = (Json::Value::Int)(-1);
        rv["msg"] = "Key not found";
    } else {
        rv["code"] = vdb_->Query(data_item, result);
        for(size_t i = 0; i < result.size(); i++) {
            Json::Value item;
            item["name"] = result[i].first;
            item["score"] = result[i].second;
            rv["result"][(int)i] = item;
        }
        rv["time_ms"] = (Json::Value::Int)tc.GetTimeMilliS();
    }
    reply = writer.write(rv);
}


}


