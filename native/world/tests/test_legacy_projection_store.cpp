// 单一store兼容门只核原v1与一次安装；新profile解码用明确独立链接桩，不冒充真实wire验收。
#include "hanmo/legacy_projection.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#ifndef HANMO_TYPED_FIXTURE_DECODER
#error "本测试必须使用独立typed fixture target，不能链接生产decoder替身"
#endif
namespace hanmo::detail {
// 本门不接收新工件，固定失败用于证明候选安装失败仍可重试。
Result<LegacyProjectionInput> decode_legacy_artifact(const std::string&) { return {false,{}, {"FixtureRejected","/fixture"}}; }
// v1不可执行关联查询，本桩若被调用即表明profile首错丢失。
Result<std::vector<ActorCommitmentsQuery>> decode_actor_queries(const std::string&,const std::string&) { return {false,{}, {"FixtureDecoderUnexpected","/fixture"}}; }
}
namespace {
using J=nlohmann::json;
int checks=0,failures=0;
// 每个断言均计入退出码，不将编译或日志存在冒称通过。
void check(bool ok,const std::string& label) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; } }
// fixture显式tag，不把原JSON数字转为领域整数。
J integer(const std::string& value) { return {{"t","int"},{"v",value}}; }
// 字符串原样编码，整数样式字符串也不改变类型。
J text(const std::string& value) { return {{"t","string"},{"v",value}}; }
// 数组保存原序，fixture不包含运行算法。
J array(J value=J::array()) { return {{"t","array"},{"v",value}}; }
// 字典明确带类型，完整产权附加键可与原值比较。
J dict(J value=J::object()) { return {{"t","dict"},{"v",value}}; }
// 旧v1只有库存及引用ID；故意不给新五类实体表。
std::string artifact() {
    J source=J::object();
    for(const auto* key:{"ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id"}) source[key]=text("old");
    source["tick"]=integer("0"); source["revision"]=integer("0");
    const J private_owner=dict({{"kind",text("actor")},{"id",text("a")}});
    const J task_owner=dict({{"kind",text("task")},{"id",text("t")},{"extra",array({integer("1"),text("1")})}});
    J lots=J::array();
    for(const auto& row:std::vector<std::vector<std::string>>{{"lot:z","3",""},{"lot:a","2",""},{"lot:r","10","action:1"}})
        lots.push_back(dict({{"id",text(row[0])},{"item_id",text("grain")},{"quantity",integer(row[1])},{"owner",row[0]=="lot:a"?private_owner:task_owner},{"reserved_by",text(row[2])}}));
    J inventory=dict({{"id",text("bag")},{"owner",private_owner},{"capacity",integer("20")},{"revision",integer("0")},{"lots",array(lots)}});
    J payload=dict({{"source",dict(source)},{"provenance",dict({{"source_commit",text(std::string(40,'a'))},{"schema_sha256",text(std::string(64,'b'))},{"rule_modules",array({dict({{"id",text("builtin")},{"version",text("1")}})})}})},
        {"references",dict({{"actors",array({text("a")})},{"organizations",array()},{"tasks",array({text("t")})},{"actions",array({text("action:1")})}})},
        {"items",array({dict({{"id",text("grain")},{"weight",integer("1")}})})},{"inventories",array({inventory})}});
    const auto raw=payload.dump();
    return J{{"format","HM-NATIVE-INVENTORY-ARTIFACT/1"},{"binding_kind","artifact"},{"payload_sha256",hanmo::sha256(raw)},{"payload_json",raw}}.dump();
}
// 原count请求保完整owner和数组次序；期望值留在断言侧。
std::string queries(const std::string& bytes) {
    auto owner=dict({{"kind",text("task")},{"id",text("t")},{"extra",array({integer("1"),text("1")})}});
    return J{{"format","HM-NATIVE-INVENTORY-QUERIES/1"},{"input_sha256",hanmo::sha256(bytes)},{"queries",J::array({
        J{{"query_id","task"},{"inventory_id","bag"},{"item_id","grain"},{"owners",J::array({owner,owner})},{"reservation",""}},
        J{{"query_id","reserved"},{"inventory_id","bag"},{"item_id","grain"},{"owners",J::array({owner})},{"reservation","action:1"}}
    })}}.dump();
}
}
// 只执行受影响兼容边界；没有完整Actor也应按原v1安装，不补假数据。
int main() {
    hanmo::ProjectionStore store;
    check(!store.audit().installed,"初态空");
    check(store.query_count_batch("{").error.code=="NotInstalled","count状态首错");
    check(store.actor_commitments("{").error.code=="NotInstalled","actor状态首错");
    check(store.install_once("{\"format\":\"HM-NATIVE-LEGACY-ARTIFACT/1\"}").error.code=="FixtureRejected","新decoder原失败");
    check(!store.audit().installed,"失败不半安装");
    const auto bytes=artifact(); const auto installed=store.install_once(bytes);
    check(installed.ok,"原v1仍可安装 "+installed.error.code);
    if(!installed.ok) return 1;
    const auto audit=store.audit();
    check(audit.profile=="HM-NATIVE-INVENTORY-ARTIFACT/1" && audit.install_count==1,"精确旧profile与一次安装");
    check(audit.lot_ids.at("bag")==std::vector<std::string>({"lot:z","lot:a","lot:r"}),"原lot序");
    check(!audit.record_component_sha256.count("actors") && !audit.record_component_sha256.count("wallets"),"v1没有虚构actor/wallet");
    auto parsed=J::parse(bytes); auto payload=J::parse(parsed["payload_json"].get<std::string>());
    check(audit.component_sha256.at("bag")==hanmo::sha256(payload["v"]["inventories"]["v"][0].dump()),"实际组件原v1摘要完全等价");
    const auto got=store.query_count_batch(queries(bytes));
    check(got.ok && got.value.size()==2 && got.value[0].count==3 && got.value[1].count==10,"唯一count复用full owner与预留");
    check(store.actor_commitments("{").error.code=="UnsupportedProjectionProfile" && store.actor_commitments("{").error.path=="/runtime/profile","v1拒新query先于坏JSON");
    check(store.install_once("{").error.code=="AlreadyInstalled","已装先于坏JSON");
    auto query=J::parse(queries(bytes)); query["queries"][0]["inventory_id"]="missing"; query["queries"][1]["owners"]=true;
    check(store.query_count_batch(query.dump()).error.code=="InvalidProjectionValue","整批decode先于前条UnknownInventory");
    query=J::parse(queries(bytes)); query["queries"][0]["inventory_id"]="missing";
    const auto rejected=store.query_count_batch(query.dump());
    check(!rejected.ok && rejected.value.empty() && rejected.error.code=="UnknownInventory","未知库存无部分counts");
    query=J::parse(queries(bytes)); query["queries"][0]["owners"][0]["v"].erase("extra"); query["queries"][0]["owners"][1]["v"].erase("extra");
    check(store.query_count_batch(query.dump()).value[0].count==0,"kind/id相同但owner不完整不匹配");
    auto copy=store.audit(); copy.lot_ids["bag"].clear(); copy.component_sha256.clear();
    check(store.audit().lot_ids==audit.lot_ids && store.audit().component_sha256==audit.component_sha256,"audit修改无别名");
    for(const auto& entry:std::vector<std::pair<std::string,std::string>>{{"{}","MissingProjectionField"},{"[]","InvalidProjectionValue"},{"{\"format\":1,\"binding_kind\":\"artifact\",\"payload_json\":\"{\",\"payload_sha256\":\"bad\"}","UnsupportedProjectionProfile"},{"{\"format\":\"x\",\"format\":\"y\"}","DuplicateJsonKey"}}) {
        hanmo::ProjectionStore empty; const auto result=empty.install_once(entry.first);
        check(!result.ok && result.error.code==entry.second,"路由不抢旧首错 "+entry.second);
        check(!empty.audit().installed && empty.install_once(bytes).ok,"拒绝后仍能安装原v1");
    }
    std::cout<<"NATIVE_STORE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
