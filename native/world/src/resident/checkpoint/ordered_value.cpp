// 唯一初始化/诊断JSON边界保完整标签与Dictionary原序，不把JSON装入常驻规则世界。
#include "ordered_value.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <set>
#include <unordered_map>

namespace hanmo::resident {
const BoundaryValue* BoundaryValue::find(std::string_view key) const {
    const auto* dict=std::get_if<Dict>(&data);
    if(dict) for(const auto& item:*dict) if(item.first==key) return &item.second;
    return nullptr;
}
BoundaryValue* BoundaryValue::find(std::string_view key) {
    return const_cast<BoundaryValue*>(static_cast<const BoundaryValue*>(this)->find(key));
}
namespace {
using Json=nlohmann::ordered_json;
// SAX直接构建唯一边界值，重复键不能被JSON库静默覆盖。
struct Parser : nlohmann::json_sax<Json> {
    struct Frame { BoundaryValue value; std::string key; std::set<std::string> keys; };
    std::vector<Frame> stack;
    BoundaryValue root;
    std::string error;
    std::size_t nodes=0;
    bool has_root=false;
    // 每个值只安装一次；父frame按原输入顺序附加。
    bool append(BoundaryValue value) {
        if(++nodes>2000000u) return bad("BoundaryNodeLimit");
        if(stack.empty()) { if(has_root) return bad("InvalidBoundaryJson"); root=std::move(value); has_root=true; }
        else if(auto* array=std::get_if<BoundaryValue::Array>(&stack.back().value.data)) array->push_back(std::move(value));
        else std::get<BoundaryValue::Dict>(stack.back().value.data).emplace_back(stack.back().key,std::move(value));
        return true;
    }
    bool bad(const std::string& code) { if(error.empty()) error=code; return false; }
    bool null() override { return append({std::monostate{}}); }
    bool boolean(bool value) override { return append({value}); }
    bool number_integer(number_integer_t value) override { return append({static_cast<Integer>(value)}); }
    bool number_unsigned(number_unsigned_t value) override { return append(BoundaryValue{static_cast<std::uint64_t>(value)}); }
    bool number_float(number_float_t value,const string_t& token) override { if(token.find_first_of(".eE")==std::string::npos) return bad("BoundaryIntegerOutOfRange"); if(!std::isfinite(value)) return bad("NonFiniteBoundaryValue"); return append({static_cast<double>(value)}); }
    bool string(string_t& value) override { return append({std::move(value)}); }
    bool binary(binary_t&) override { return bad("UnsupportedBoundaryBinary"); }
    bool start_object(std::size_t) override { if(stack.size()>=64) return bad("BoundaryDepthLimit"); stack.push_back({{BoundaryValue::Dict{}},{},{}}); return true; }
    bool key(string_t& value) override { if(!stack.back().keys.insert(value).second) return bad("DuplicateBoundaryKey"); stack.back().key=std::move(value); return true; }
    bool end_object() override { auto value=std::move(stack.back().value); stack.pop_back(); return append(std::move(value)); }
    bool start_array(std::size_t) override { if(stack.size()>=64) return bad("BoundaryDepthLimit"); stack.push_back({{BoundaryValue::Array{}},{},{}}); return true; }
    bool end_array() override { auto value=std::move(stack.back().value); stack.pop_back(); return append(std::move(value)); }
    bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&) override { return bad("InvalidBoundaryJson"); }
};
// 输出仅允许同样有界的类型，不让非有限值悄悄被序列化为null。
Json to_json(const BoundaryValue& value,std::size_t depth) {
    if(depth>64) throw std::length_error("BoundaryDepthLimit");
    return std::visit([&](const auto& item)->Json {
        using T=std::decay_t<decltype(item)>;
        if constexpr(std::is_same_v<T,std::monostate>) return nullptr;
        else if constexpr(std::is_same_v<T,BoundaryValue::Array>) { Json result=Json::array(); for(const auto& x:item) result.push_back(to_json(x,depth+1)); return result; }
        else if constexpr(std::is_same_v<T,BoundaryValue::Dict>) { Json result=Json::object(); for(const auto& x:item) { if(result.contains(x.first)) throw std::invalid_argument("DuplicateBoundaryKey"); result[x.first]=to_json(x.second,depth+1); } return result; }
        else { if constexpr(std::is_same_v<T,double>) if(!std::isfinite(item)) throw std::invalid_argument("NonFiniteBoundaryValue"); return item; }
    },value.data);
}
// 路径转义遵循JSON pointer，键中斜线不会产生含糊的错误位置。
std::string child_path(const std::string& path,const std::string& key) {
    std::string result=path+"/"; for(char c:key) { if(c=='~') result+="~0"; else if(c=='/') result+="~1"; else result+=c; } return result;
}
// 记录首个差异但继续检查另一维度；字典序不能掩盖后面真实值分歧。
void value_failure(BoundaryComparison& result,const std::string& path,const std::string& reason) {
    result.values_equal=false; if(!result.value_difference) result.value_difference=BoundaryDifference{path,reason};
}
void compare(const BoundaryValue& a,const BoundaryValue& b,const std::string& path,BoundaryComparison& result) {
    if(a.data.index()!=b.data.index()) { value_failure(result,path,"type"); return; }
    if(const auto* left=std::get_if<BoundaryValue::Dict>(&a.data)) {
        const auto& right=std::get<BoundaryValue::Dict>(b.data);
        if(left->size()!=right.size()) value_failure(result,path,"dictionary_size");
        bool same_order=left->size()==right.size(); std::unordered_map<std::string,const BoundaryValue*> lookup;
        for(const auto& item:right) if(!lookup.emplace(item.first,&item.second).second) value_failure(result,path,"duplicate_key");
        std::set<std::string> left_keys;
        for(std::size_t i=0;i<left->size();++i) {
            const auto& item=(*left)[i]; if(!left_keys.insert(item.first).second) value_failure(result,path,"duplicate_key");
            if(i>=right.size()||item.first!=right[i].first) same_order=false;
            const auto found=lookup.find(item.first);
            if(found==lookup.end()) value_failure(result,child_path(path,item.first),"missing_key");
            else compare(item.second,*found->second,child_path(path,item.first),result);
        }
        if(!same_order) { result.dictionary_order_equal=false; if(!result.order_difference) result.order_difference=BoundaryDifference{path,"dictionary_order"}; }
    } else if(const auto* left=std::get_if<BoundaryValue::Array>(&a.data)) {
        const auto& right=std::get<BoundaryValue::Array>(b.data);
        if(left->size()!=right.size()) value_failure(result,path,"array_size");
        for(std::size_t i=0;i<std::min(left->size(),right.size());++i) compare((*left)[i],right[i],child_path(path,std::to_string(i)),result);
    } else {
        const bool equal=std::visit([&](const auto& item) {
            using T=std::decay_t<decltype(item)>;
            if constexpr(std::is_same_v<T,BoundaryValue::Array>||std::is_same_v<T,BoundaryValue::Dict>) return false;
            else if constexpr(std::is_same_v<T,double>) { const auto other=std::get<double>(b.data); return std::memcmp(&item,&other,sizeof(double))==0; }
            else return item==std::get<T>(b.data);
        },a.data);
        if(!equal) value_failure(result,path,"value");
    }
}
// 任何解析失败不返回部分root。
template<class T> Outcome<T> fail(const std::string& code) { return {false,{},{{code,"boundary"},ErrorKind::rule,{},{},{},{}}}; }
}
Outcome<BoundaryValue> parse_boundary_json(const std::string& bytes) {
    if(bytes.size()>64u*1024u*1024u) return fail<BoundaryValue>("BoundaryByteLimit");
    try { Parser parser; if(!Json::sax_parse(bytes,&parser)||!parser.has_root) return fail<BoundaryValue>(parser.error.empty()?"InvalidBoundaryJson":parser.error); return {true,std::move(parser.root),{}}; }
    catch(const std::bad_alloc&) { return fail<BoundaryValue>("BoundaryResourceLimit"); }
    catch(const std::exception&) { return fail<BoundaryValue>("InvalidBoundaryJson"); }
}
Outcome<std::string> render_boundary_json(const BoundaryValue& value) {
    try { auto bytes=to_json(value,0).dump(); if(bytes.size()>64u*1024u*1024u) return fail<std::string>("BoundaryByteLimit"); return {true,std::move(bytes),{}}; }
    catch(const std::bad_alloc&) { return fail<std::string>("BoundaryResourceLimit"); }
    catch(const std::exception&) { return fail<std::string>("InvalidBoundaryValue"); }
}
Outcome<std::vector<StableId>> capture_field_order(const BoundaryValue& dictionary) {
    const auto* dict=dictionary.as<BoundaryValue::Dict>(); if(!dict) return fail<std::vector<StableId>>("ExpectedBoundaryDictionary");
    std::vector<StableId> keys; keys.reserve(dict->size()); std::set<StableId> seen;
    for(const auto& item:*dict) {if(!seen.insert(item.first).second) return fail<std::vector<StableId>>("DuplicateBoundaryKey"); keys.push_back(item.first);}
    return {true,std::move(keys),{}};
}
Status apply_field_order(BoundaryValue& dictionary,const std::vector<StableId>& keys) {
    auto* dict=dictionary.as<BoundaryValue::Dict>(); if(!dict) return fail<Done>("ExpectedBoundaryDictionary");
    if(keys.empty()) return {true,{},{}};
    if(keys.size()!=dict->size()) return fail<Done>("BoundaryFieldOrderMismatch");
    std::unordered_map<StableId,std::size_t> positions;
    for(std::size_t i=0;i<dict->size();++i) if(!positions.emplace((*dict)[i].first,i).second) return fail<Done>("DuplicateBoundaryKey");
    std::set<StableId> seen;
    for(const auto& key:keys) if(!positions.count(key)||!seen.insert(key).second) return fail<Done>("BoundaryFieldOrderMismatch");
    // 全部验证后才move，失败不会让调用者的完整边界对象残缺。
    BoundaryValue::Dict reordered; reordered.reserve(keys.size());
    for(const auto& key:keys) reordered.push_back(std::move((*dict)[positions.at(key)]));
    dictionary.data=std::move(reordered); return {true,{},{}};
}
BoundaryComparison compare_boundary(const BoundaryValue& a,const BoundaryValue& b) { BoundaryComparison result; compare(a,b,"",result); return result; }
} // 完整对照在规则计时之外执行。
