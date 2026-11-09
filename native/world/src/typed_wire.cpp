// 原v1严格解析帮助函数机械迁位；JSON物理深度和TypedValue递归限制分别保留。
#include "typed_wire.hpp"
#include <charconv>
#include <limits>
namespace hanmo::detail {
// 字段错误只通过有界结果返回，不修补输入。
[[noreturn]] void fail(std::string code,std::string path) { throw Failure{{std::move(code),std::move(path)}}; }
// JSON指针转义避免额外字段名称改变审计路径含义。
std::string at(const std::string& path,const std::string& key) {
    std::string escaped;
    for(char c:key) { if(c=='~') escaped+="~0"; else if(c=='/') escaped+="~1"; else escaped+=c; }
    return path+"/"+escaped;
}
// 每个文档独立限深，回调观察解码后的键以拒绝包括转义形式的重复键。
Json parse(const std::string& bytes,const std::string& path) {
    if(bytes.size()>wire_limit) fail("WireSizeLimit",path);
    std::vector<std::set<std::string>> keys;
    auto callback=[&](int,Json::parse_event_t event,Json& parsed) {
        if(event==Json::parse_event_t::object_start || event==Json::parse_event_t::array_start) {
            if(keys.size()>=wire_depth_limit) fail("WireDepthLimit",path);
            keys.emplace_back();
        } else if(event==Json::parse_event_t::key) {
            if(keys.empty() || !keys.back().insert(parsed.get<std::string>()).second) fail("DuplicateJsonKey",path);
        } else if(event==Json::parse_event_t::object_end || event==Json::parse_event_t::array_end) keys.pop_back();
        return true;
    };
    try { return Json::parse(bytes,callback,true,false); }
    catch(const Json::exception&) { fail("InvalidJson",path); }
}
// 闭合对象缺字段或额外字段明确失败；TypedValue扩展字典不使用此限制。
// 先核类型后读取，避免JSON库隐式将bool或float取为整数。
std::string json_string(const Json& value,const std::string& path) {
    if(!value.is_string()) fail("InvalidProjectionValue",path);
    return value.get<std::string>();
}
// int64规范十进制严格往返，最小负数也不经取反溢出。
std::int64_t decimal(const std::string& text,const std::string& path) {
    std::int64_t result=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || std::to_string(result)!=text) fail("InvalidIntegerEncoding",path);
    return result;
}
// 任意被保留扩展值只能来自已冻结tag集合，字符串key不转数字。
Value value(const Json& row,const std::string& path,std::size_t depth) {
    if(depth>wire_depth_limit) fail("WireDepthLimit",path);
    if(!row.is_object() || !row.contains("t")) fail("InvalidProjectionValue",path);
    const auto tag=json_string(row.at("t"),at(path,"t"));
    if(tag=="null") { fields(row,{"t"},{},path); return {}; }
    fields(row,{"t","v"},{},path); const auto& v=row.at("v");
    if(tag=="bool") { if(!v.is_boolean()) fail("InvalidProjectionValue",path); return Value{v.get<bool>()}; }
    if(tag=="int") return Value{decimal(json_string(v,at(path,"v")),path)};
    if(tag=="string") return Value{json_string(v,at(path,"v"))};
    if(tag=="array") {
        if(!v.is_array()) fail("InvalidProjectionValue",path);
        Array result; result.reserve(v.size());
        for(std::size_t i=0;i<v.size();++i) result.push_back(value(v[i],at(path,std::to_string(i)),depth+1));
        return Value{std::move(result)};
    }
    if(tag=="dict") {
        if(!v.is_object()) fail("InvalidProjectionValue",path);
        Dict result;
        for(auto it=v.begin();it!=v.end();++it) result.emplace(it.key(),value(it.value(),at(path,it.key()),depth+1));
        return Value{std::move(result)};
    }
    fail("UnsupportedProjectionValue",path);
}
// 原类型必须匹配，不给缺失／错误值提供可用默认值。
// 领域数值保持旧有效域，承载宽度不构成规则放宽。
std::int64_t whole(const Value& v,std::int64_t minimum,std::int64_t maximum,const std::string& path) {
    const auto result=*require_value<std::int64_t>(v,path);
    if(result<minimum || result>maximum) fail("ProjectionRange",path);
    return result;
}
// SHA格式只识别摘要文本，不替代实际字节核验。
bool hex(const std::string& text,std::size_t length) { return text.size()==length && std::all_of(text.begin(),text.end(),[](char c){ return (c>='0'&&c<='9')||(c>='a'&&c<='f'); }); }
// 引用表保精确字符串域，重复ID不能覆盖先前项。
std::set<std::string> ids(const Value& source,const std::string& path) {
    std::set<std::string> result;
    for(const auto& row:*require_value<Array>(source,path)) {
        const auto& id=*require_value<std::string>(row,path);
        if(id.empty() || !result.insert(id).second) fail("DuplicateReferenceId",path);
    }
    return result;
}
// 查询owner可不在源世界里，此时原算法无匹配；工件owner则必须有真实引用。
void owner(const Value& input,const std::string& path,const std::map<std::string,std::set<std::string>>* references) {
    const auto& obj=*require_value<Dict>(input,path);
    if(!obj.count("kind") || !obj.count("id")) fail("InvalidOwner",path);
    const auto& kind=*require_value<std::string>(obj.at("kind"),at(path,"kind"));
    const auto& id=*require_value<std::string>(obj.at("id"),at(path,"id"));
    if(kind!="actor" && kind!="organization" && kind!="task") fail("InvalidOwner",path);
    if(references && !references->at(kind).count(id)) fail("DanglingOwner",path);
}
// 序号／构建来源只按本profile核形状，不宣称认证来源或替代旧Schema。
void provenance(const Dict& payload) {
    const auto& source=*require_value<Dict>(payload.at("source"),"/payload/source");
    fields(source,{"ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"},{},"/payload/source");
    for(const auto* key:{"ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id"}) (void)*require_value<std::string>(source.at(key),at("/payload/source",key));
    for(const auto* key:{"tick","revision"}) whole(source.at(key),0,2147483647,at("/payload/source",key));
    const auto& proof=*require_value<Dict>(payload.at("provenance"),"/payload/provenance");
    fields(proof,{"source_commit","schema_sha256","rule_modules"},{"pack_set_digest"},"/payload/provenance");
    if(!hex(*require_value<std::string>(proof.at("source_commit"),"/payload/provenance/source_commit"),40) || !hex(*require_value<std::string>(proof.at("schema_sha256"),"/payload/provenance/schema_sha256"),64)) fail("InvalidProvenance","/payload/provenance");
    if(proof.count("pack_set_digest") && !hex(*require_value<std::string>(proof.at("pack_set_digest"),"/payload/provenance/pack_set_digest"),64)) fail("InvalidProvenance","/payload/provenance/pack_set_digest");
    std::set<std::string> seen;
    for(const auto& row:*require_value<Array>(proof.at("rule_modules"),"/payload/provenance/rule_modules")) {
        const auto& module=*require_value<Dict>(row,"/payload/provenance/rule_modules"); fields(module,{"id","version"},{},"/payload/provenance/rule_modules");
        const auto& id=*require_value<std::string>(module.at("id"),"/payload/provenance/rule_modules/id");
        if(id.empty() || !seen.insert(id).second || require_value<std::string>(module.at("version"),"/payload/provenance/rule_modules/version")->empty()) fail("InvalidProvenance","/payload/provenance/rule_modules");
    }
    if(seen.empty()) fail("InvalidProvenance","/payload/provenance/rule_modules");
}
// 正权重与数量只做检查过的乘加，先拒绝溢出再比较容量。
void add_weight(std::int64_t& total,std::int64_t quantity,std::int64_t weight,const std::string& path) {
    const auto maximum=std::numeric_limits<std::int64_t>::max();
    if(quantity>maximum/weight || total>maximum-quantity*weight) fail("ProjectionOverflow",path);
    total+=quantity*weight;
}
// 为审计重建tag树，所有类型和数组顺序来自实际组件而非缓存原输入。
Json encode_value(const Value& value) {
    return std::visit([](const auto& stored)->Json {
        using T=std::decay_t<decltype(stored)>;
        if constexpr(std::is_same_v<T,std::monostate>) return {{"t","null"}};
        else if constexpr(std::is_same_v<T,bool>) return {{"t","bool"},{"v",stored}};
        else if constexpr(std::is_same_v<T,std::int64_t>) return {{"t","int"},{"v",std::to_string(stored)}};
        else if constexpr(std::is_same_v<T,std::string>) return {{"t","string"},{"v",stored}};
        else if constexpr(std::is_same_v<T,Array>) {
            Json result=Json::array(); for(const auto& item:stored) result.push_back(encode_value(item));
            return {{"t","array"},{"v",std::move(result)}};
        } else {
            Json result=Json::object(); for(const auto& [key,item]:stored) result[key]=encode_value(item);
            return {{"t","dict"},{"v",std::move(result)}};
        }
    },value.data);
}

} // 值拥有的只读wire接缝。
