// 严格JSON与TaggedValue共用接缝；只机械复用旧边界，不形成通用查询平台。
#pragma once
#include "hanmo/projection_types.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>
#include <type_traits>
namespace hanmo::detail {
using Json=nlohmann::json;
using Dict=Value::Dict;
using Array=Value::Array;
struct Failure { Error error; };
[[noreturn]] void fail(std::string code,std::string path);
std::string at(const std::string& path,const std::string& key);
Json parse(const std::string& bytes,const std::string& path);
template<class Object> void fields(const Object& obj,std::initializer_list<const char*> required,std::initializer_list<const char*> optional,const std::string& path) {
    for(const char* key:required) if(obj.find(key)==obj.end()) fail("MissingProjectionField",at(path,key));
    for(auto it=obj.begin();it!=obj.end();++it) {
        std::string key;
        if constexpr(std::is_same_v<Object,Json>) key=it.key(); else key=it->first;
        auto is_key=[&](const char* name) { return key==name; };
        if(std::find_if(required.begin(),required.end(),is_key)==required.end() && std::find_if(optional.begin(),optional.end(),is_key)==optional.end()) fail("UnsupportedProjectionField",at(path,key));
    }
}
std::string json_string(const Json& value,const std::string& path);
std::int64_t decimal(const std::string& text,const std::string& path);
Value value(const Json& row,const std::string& path,std::size_t depth=1);
template<class T> const T* require_value(const Value& v,const std::string& path) {
    const T* result=std::get_if<T>(&v.data);
    if(!result) fail("InvalidProjectionValue",path);
    return result;
}
std::int64_t whole(const Value& v,std::int64_t minimum,std::int64_t maximum,const std::string& path);
bool hex(const std::string& text,std::size_t length);
std::set<std::string> ids(const Value& source,const std::string& path);
void owner(const Value& input,const std::string& path,const std::map<std::string,std::set<std::string>>* references=nullptr);
void provenance(const Dict& payload);
void add_weight(std::int64_t& total,std::int64_t quantity,std::int64_t weight,const std::string& path);
Json encode_value(const Value& value);
} // 原profile继续使用这些同一函数。
