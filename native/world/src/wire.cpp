// 旧v1库存profile保持原字段、先后和错误；通用wire函数只委托唯一实现。
#include "wire.hpp"
#include "typed_wire.hpp"
#include "inventory_core.hpp"
#include <picosha2.h>
#include <limits>
namespace hanmo {
std::string sha256(const std::string& bytes) { return picosha2::hash256_hex_string(bytes); }
namespace detail {
// 全部内容先形成值拥有的候选，任何失败均不产生可安装的部分清单。
Result<Input> decode_artifact(const std::string& bytes) {
    try {
        const auto envelope=parse(bytes,"/artifact");
        if(!envelope.is_object()) fail("InvalidProjectionValue","/artifact");
        fields(envelope,{"format","binding_kind","payload_sha256","payload_json"},{},"/artifact");
        if(envelope.at("format")!="HM-NATIVE-INVENTORY-ARTIFACT/1" || envelope.at("binding_kind")!="artifact") fail("UnsupportedProjectionProfile","/artifact");
        const auto raw=json_string(envelope.at("payload_json"),"/artifact/payload_json");
        const auto digest=json_string(envelope.at("payload_sha256"),"/artifact/payload_sha256");
        if(!hex(digest,64) || digest!=sha256(raw)) fail("PayloadDigestMismatch","/artifact/payload_sha256");
        const auto typed=value(parse(raw,"/payload"),"/payload"); const auto& payload=*require_value<Dict>(typed,"/payload");
        fields(payload,{"source","provenance","references","items","inventories"},{},"/payload"); provenance(payload);
        const auto& refs=*require_value<Dict>(payload.at("references"),"/payload/references"); fields(refs,{"actors","organizations","tasks","actions"},{},"/payload/references");
        std::map<std::string,std::set<std::string>> references{{"actor",ids(refs.at("actors"),"/payload/references/actors")},{"organization",ids(refs.at("organizations"),"/payload/references/organizations")},{"task",ids(refs.at("tasks"),"/payload/references/tasks")}};
        const auto actions=ids(refs.at("actions"),"/payload/references/actions");
        std::map<std::string,std::int64_t> items;
        for(const auto& row:*require_value<Array>(payload.at("items"),"/payload/items")) {
            const auto& item=*require_value<Dict>(row,"/payload/items"); fields(item,{"id","weight"},{},"/payload/items");
            const auto& id=*require_value<std::string>(item.at("id"),"/payload/items/id");
            if(id.empty() || !items.emplace(id,whole(item.at("weight"),1,10000,"/payload/items/weight")).second) fail("DuplicateItemId","/payload/items");
        }
        Input result; result.input_sha256=sha256(bytes); result.payload_sha256=digest;
        result.inventories=decode_inventories(payload.at("inventories"),references,actions,items,false);
        return {true,std::move(result),{}};
    } catch(const Failure& failure) { return {false,{},failure.error}; }
      catch(const std::bad_alloc&) { return {false,{}, {"ProjectionResourceLimit","/artifact"}}; }
}

// 查询载荷闭合且绑定整个artifact原字节；不将不存在的匹配值视为权限错误。
Result<std::vector<Query>> decode_queries(const std::string& bytes,const std::string& input_sha256) {
    try {
        const auto root=parse(bytes,"/queries");
        if(!root.is_object()) fail("InvalidProjectionValue","/queries");
        fields(root,{"format","input_sha256","queries"},{},"/queries");
        if(root.at("format")!="HM-NATIVE-INVENTORY-QUERIES/1") fail("UnsupportedProjectionProfile","/queries/format");
        if(json_string(root.at("input_sha256"),"/queries/input_sha256")!=input_sha256) fail("SourceMismatch","/queries/input_sha256");
        if(!root.at("queries").is_array()) fail("InvalidProjectionValue","/queries/queries");
        std::vector<Query> result; std::set<std::string> seen;
        for(std::size_t i=0;i<root.at("queries").size();++i) {
            const auto path=at("/queries/queries",std::to_string(i)); const auto& row=root.at("queries")[i];
            if(!row.is_object()) fail("InvalidProjectionValue",path);
            fields(row,{"query_id","inventory_id","item_id","owners","reservation"},{},path); Query query;
            query.id=json_string(row.at("query_id"),at(path,"query_id"));
            if(query.id.empty() || !seen.insert(query.id).second) fail("DuplicateQueryId",at(path,"query_id"));
            query.inventory_id=json_string(row.at("inventory_id"),at(path,"inventory_id")); query.item_id=json_string(row.at("item_id"),at(path,"item_id")); query.reservation=json_string(row.at("reservation"),at(path,"reservation"));
            if(!row.at("owners").is_array()) fail("InvalidProjectionValue",at(path,"owners"));
            for(std::size_t n=0;n<row.at("owners").size();++n) {
                const auto op=at(at(path,"owners"),std::to_string(n)); auto o=value(row.at("owners")[n],op); owner(o,op); query.owners.push_back(std::move(o));
            }
            result.push_back(std::move(query));
        }
        return {true,std::move(result),{}};
    } catch(const Failure& failure) { return {false,{},failure.error}; }
      catch(const std::bad_alloc&) { return {false,{}, {"ProjectionResourceLimit","/queries"}}; }
}

} // 私有类型解码边界。
} // 原生只读组件命名空间。
