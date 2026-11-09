// 独立原生合同门：坏wire、完整产权、批次顺序和一次安装，未运行不能标通过。
#include "hanmo/inventory_projection.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>

using J=nlohmann::json;
namespace {
int checks=0,failures=0;
// 所有失败参与退出码，避免只有日志而发行仍成功。
void check(bool value,const std::string& label) { ++checks; if(!value) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; } }
// 测试wire明确编码类型，不把JSON数字用作领域整数。
J integer(const std::string& value) { return {{"t","int"},{"v",value}}; }
// String显式保留原类型，不借内容看似数字改变编码。
J text(const std::string& value) { return {{"t","string"},{"v",value}}; }
// 数组fixture保调用方给定顺序。
J array(J value=J::array()) { return {{"t","array"},{"v",std::move(value)}}; }
// 字典fixture完整编码所有键，原语义不依赖键插入序。
J dict(J value=J::object()) { return {{"t","dict"},{"v",std::move(value)}}; }
// 产权最小值仍是完整TypedValue字典，可继续附加原字段。
J owner(const std::string& kind,const std::string& id) { return dict({{"kind",text(kind)},{"id",text(id)}}); }
// 最小纯合同输入不冒称旧Kernel玩法；实际采购另由GDScript独立oracle证明。
J payload() {
    J source=J::object();
    for(const auto* key:{"ruleset_id","content_id","content_version","engine_build","rng_backend","scenario_id"}) source[key]=text("fixture");
    source["content_digest"]=text(std::string(64,'a')); source["tick"]=integer("0"); source["revision"]=integer("0");
    J private_owner=owner("actor","a"); J task_owner=owner("task","t");
    J lots=J::array();
    lots.push_back(dict({{"id",text("lot:z")},{"item_id",text("grain")},{"quantity",integer("3")},{"owner",task_owner},{"reserved_by",text("")}}));
    lots.push_back(dict({{"id",text("lot:a")},{"item_id",text("grain")},{"quantity",integer("2")},{"owner",private_owner},{"reserved_by",text("")}}));
    lots.push_back(dict({{"id",text("lot:r")},{"item_id",text("grain")},{"quantity",integer("10")},{"owner",task_owner},{"reserved_by",text("action:1")}}));
    J modules=array({dict({{"id",text("fixture")},{"version",text("1")}})});
    J proof=dict({{"source_commit",text(std::string(40,'b'))},{"schema_sha256",text(std::string(64,'c'))},{"rule_modules",modules}});
    return dict({{"source",dict(source)},
        {"provenance",proof},
        {"references",dict({{"actors",array({text("a")})},{"organizations",array({text("o")})},{"tasks",array({text("t")})},{"actions",array({text("action:1")})}})},
        {"items",array({dict({{"id",text("grain")},{"weight",integer("1")}})})},
        {"inventories",array({dict({{"id",text("base::bag:a")},{"owner",private_owner},{"capacity",integer("20")},{"revision",integer("0")},{"lots",array(lots)}})})}});
}
// 摘要覆盖精确payload字符串，不让fixture默认重排修正被测输入。
std::string artifact(const J& body) { const auto raw=body.dump(); return J({{"format","HM-NATIVE-INVENTORY-ARTIFACT/1"},{"binding_kind","artifact"},{"payload_sha256",hanmo::sha256(raw)},{"payload_json",raw}}).dump(); }
// 查询预期值不在输入内；oracle只存在断言侧。
std::string queries(const std::string& bytes,J rows=J::array()) {
    if(rows.empty()) rows={{{"query_id","private"},{"inventory_id","base::bag:a"},{"item_id","grain"},{"owners",J::array({owner("actor","a"),owner("actor","a")})},{"reservation",""}},
        {{"query_id","task"},{"inventory_id","base::bag:a"},{"item_id","grain"},{"owners",J::array({owner("task","t")})},{"reservation",""}},
        {{"query_id","reserved"},{"inventory_id","base::bag:a"},{"item_id","grain"},{"owners",J::array({owner("task","t")})},{"reservation","action:1"}}};
    return J({{"format","HM-NATIVE-INVENTORY-QUERIES/1"},{"input_sha256",hanmo::sha256(bytes)},{"queries",rows}}).dump();
}
// 每个拒绝都须留下空runtime，随后可安装合法输入；不将失败算作一次成功安装。
void rejected(const std::string& bytes,const std::string& code) {
    hanmo::InventoryProjection runtime;
    auto result=runtime.install_once(bytes);
    check(!result.ok && result.error.code==code,"拒绝 "+code+" 实际="+result.error.code);
    check(!runtime.audit().installed && runtime.audit().install_count==0,"失败无部分安装");
    check(runtime.install_once(artifact(payload())).ok,"失败后仍可正常安装一次");
}
// 真实Flecs读组件，保原批次顺序及来源字节，不返回任何可改句柄。
void readonly_runtime() {
    auto bytes=artifact(payload()); hanmo::InventoryProjection runtime;
    check(!runtime.query_batch(queries(bytes)).ok,"未安装不能查询");
    const auto installed=runtime.install_once(bytes);
    check(installed.ok,"完整合同安装 "+installed.error.code+installed.error.path);
    if(!installed.ok) return;
    const auto before=runtime.audit();
    check(before.install_count==1 && before.lot_ids.at("base::bag:a")==std::vector<std::string>({"lot:z","lot:a","lot:r"}),"Flecs原批次顺序不按ID排序");
    const auto query=queries(bytes); const auto got=runtime.query_batch(query);
    check(got.ok && got.value.size()==3 && got.value[0].count==2 && got.value[1].count==3 && got.value[2].count==10,"同一原count含重复owner只计一次");
    const auto repeated=runtime.query_batch(query);
    check(repeated.ok && repeated.value[0].count==2 && runtime.audit().lot_ids==before.lot_ids && runtime.audit().input_sha256==before.input_sha256,"重复查询不动Flecs组件");
    check(runtime.install_once(bytes).error.code=="AlreadyInstalled","同源第二次也拒绝");
    bytes[0]='x'; check(runtime.query_batch(query).ok,"调用方修改输入缓冲不影响安装值");
    auto bad=J::parse(query); bad["queries"][1]["inventory_id"]="missing";
    auto failed=runtime.query_batch(bad.dump());
    check(!failed.ok && failed.value.empty() && failed.error.code=="UnknownInventory","整批坏中间项不返回首项成功");
    bad=J::parse(query); bad["input_sha256"]=std::string(64,'0'); check(runtime.query_batch(bad.dump()).error.code=="SourceMismatch","异源查询拒绝");
    bad=J::parse(query); bad["queries"][1]["query_id"]="private"; check(runtime.query_batch(bad.dump()).error.code=="DuplicateQueryId","重复query ID拒绝");
    bad=J::parse(query); bad["queries"][0]["item_id"]="missing"; bad["queries"][1]["owners"][0]=owner("task","absent"); bad["queries"][2]["reservation"]="absent";
    auto empty=runtime.query_batch(bad.dump()); check(empty.ok && empty.value[0].count==0 && empty.value[1].count==0 && empty.value[2].count==0,"没有匹配沿原算法为0");
}
// 完整owner包含任意已支持嵌套键，不能缩成kind/id匹配。
void rich_owner() {
    auto body=payload(); auto& lots=body["v"]["inventories"]["v"][0]["v"]["lots"]["v"];
    lots[1]["v"]["owner"]["v"]["note"]=dict({{"order",array({text("second"),text("first")})},{"integer",integer("1")},{"string",text("1")}});
    lots[1]["v"]["extra"]=dict({{"keep",array({integer("-9223372036854775808"),integer("9223372036854775807")})}});
    const auto bytes=artifact(body); hanmo::InventoryProjection runtime; const auto installed=runtime.install_once(bytes);
    check(installed.ok,"owner和lot扩展及int64承载 "+installed.error.code+installed.error.path);
    if(!installed.ok) return;
    check(runtime.audit().component_sha256.at("base::bag:a")==hanmo::sha256(body["v"]["inventories"]["v"][0].dump()),"实际Flecs完整组件摘要保留额外lot／owner值与类型");
    auto query=J::parse(queries(bytes));
    auto plain=runtime.query_batch(query.dump()); check(plain.ok && plain.value[0].count==0,"删owner附加键不匹配");
    query["queries"][0]["owners"]=J::array({lots[1]["v"]["owner"]});
    auto rich=runtime.query_batch(query.dump()); check(rich.ok && rich.value[0].count==2,"完整owner仍匹配2");
    query["queries"][0]["owners"][0]["v"]["note"]["v"]["integer"]=text("1");
    auto typed=runtime.query_batch(query.dump()); check(typed.ok && typed.value[0].count==0,"整数和String不混同");
}
// wire负例覆盖重复键、规范整数、原范围、深度、来源摘要与引用。
void malformed_wire() {
    rejected("{\"format\":\"a\",\"format\":\"b\"}","DuplicateJsonKey");
    auto raw=J::parse(artifact(payload())); raw["payload_sha256"]=std::string(64,'0'); rejected(raw.dump(),"PayloadDigestMismatch");
    raw=J::parse(artifact(payload())); raw["extra"]=true; rejected(raw.dump(),"UnsupportedProjectionField");
    raw=J::parse(artifact(payload())); raw["payload_json"]="{\"t\":\"null\",\"t\":\"dict\"}"; raw["payload_sha256"]=hanmo::sha256(raw["payload_json"].get<std::string>()); rejected(raw.dump(),"DuplicateJsonKey");
    for(const std::string value:{"01","-0","+1"," 1","1.0","9223372036854775808","-9223372036854775809"}) {
        auto body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]=integer(value); rejected(artifact(body),"InvalidIntegerEncoding");
    }
    for(const std::string value:{"0","-1","2147483648"}) {
        auto body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]=integer(value); rejected(artifact(body),"ProjectionRange");
    }
    auto body=payload(); body["v"]["inventories"]["v"][0]["v"]["owner"]["v"]["id"]=text("missing"); rejected(artifact(body),"DanglingOwner");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["lots"]["v"][1]["v"]["reserved_by"]=text("missing"); rejected(artifact(body),"DanglingReservation");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["lots"]["v"][1]["v"]["id"]=text("lot:z"); rejected(artifact(body),"DuplicateLotId");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]=integer("14"); rejected(artifact(body),"OverCapacity");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]={{"t","float"},{"v",1.0}}; rejected(artifact(body),"UnsupportedProjectionValue");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]={{"t","bool"},{"v",true}}; rejected(artifact(body),"InvalidProjectionValue");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]=text("20"); rejected(artifact(body),"InvalidProjectionValue");
    body=payload(); body["v"]["inventories"]["v"][0]["v"]["capacity"]={{"t","int"},{"v",20}}; rejected(artifact(body),"InvalidProjectionValue");
    std::string nested; for(int i=0;i<65;++i) nested+='['; for(int i=0;i<65;++i) nested+=']'; rejected(nested,"WireDepthLimit");
    nested.clear(); for(int i=0;i<64;++i) nested+='['; for(int i=0;i<64;++i) nested+=']'; rejected(nested,"InvalidProjectionValue");
    rejected(std::string(hanmo::wire_limit+1,' '),"WireSizeLimit");
}
} // 本地测试辅助不进入产品API。

// 只运行本片自有断言；缺依赖时该可执行文件根本尚未生成。
int main() {
    readonly_runtime(); rich_owner(); malformed_wire();
    std::cout<<"NATIVE_INVENTORY_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';
    return failures?1:0;
}
