// 手写军政wire边界测试；原GDScript oracle及真实身份来源由另一测试入口独立验证。
#include "hanmo/military_schedule.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using J=nlohmann::json;
using V=hanmo::detail::Value;
namespace {
int checks=0,failures=0;
// 失败保明确标签与非零退出，不将空数组误认为未实施代码的成功。
void check(bool ok,const std::string& label) { ++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';} }
// 角色上下文保持本人地点、当前办理人和岗位三个实际字符串。
J context(const std::string& place="gate",const std::string& role="role") { return {{"place_id",place},{"role_id",role},{"provider_actor_id","provider"}}; }
// 行记录只含纯算法消费值，默认静态本人首次受任。
J actor(const std::string& id="npc") {
    return {{"actor_id",id},{"alive",true},{"available",true},{"action_id",""},{"combat_occupied",false},{"is_static",true},
        {"location",{{"kind","Settlement"},{"place_id","gate"},{"settlement_id","a"}}},
        {"plan",{{"office_id","office"},{"order_id","order"},{"unit_ids",J::array({"u2","u1"})},{"food_quantity","7"},{"money","9"}}},
        {"office",{{"enabled",false},{"holder_actor_id",""},{"appointment_id",""},{"garrison_place_id","camp"},{"appointing_context",context()}}},
        {"order",{{"issuer_context",context()},{"objective_kind","occupy"}}},{"campaigns",J::array()}};
}
// 领域helper正常无结果以显式空ID/false/空roles保留，不构建通用缺值系统。
J campaign(const std::string& status="accepted",const std::string& id="campaign") {
    return {{"id",id},{"actor_id","npc"},{"template_id","order"},{"status",status},{"army_id","army"},{"office_id","office"},
        {"office_enabled",true},{"office_holder_actor_id","npc"},{"objective_result_nonempty",false},
        {"pending_siege",{{"id",""},{"phase",""},{"player_siege_id",""}}},
        {"return_choice",{{"ok",false},{"garrison_id",""},{"place_id",""}}},{"report_roles",J::array()}};
}
// 有向passages顺序决定BFS第一条等长路径，目录只包含当前城和目标place。
J request() {
    return {{"format","HM-NATIVE-MILITARY-SCHEDULE/1"},{"scope_token","call:3"},{"player_actor_id","player"},{"actors",J::array({actor()})},
        {"places",J::array({{{"id","gate"},{"settlement_id","a"},{"passages",J::array({"right","left"})}},{{"id","right"},{"settlement_id","a"},{"passages",J::array({"gate","camp"})}},{{"id","left"},{"settlement_id","a"},{"passages",J::array({"camp"})}},{{"id","camp"},{"settlement_id","a"},{"passages",J::array({"gate"})}},{{"id","foreign"},{"settlement_id","b"},{"passages",J::array()}}})},
        {"settlements",J::array({{{"id","a"},{"entry_place","gate"}},{{"id","b"},{"entry_place","foreign"}}})}};
}
// 从同库真实返回读取提案，不把输入另算一遍作为oracle。
void expects(const J& q,const std::string& kind,const std::string& key="",const V& expected={}) {
    const auto r=hanmo::military_schedule_proposals(q.dump());check(r.ok,"调用成功 "+r.error.code+r.error.path);if(!r.ok)return;
    check(r.value.evaluated_count==static_cast<std::int64_t>(q["actors"].size()) && r.value.scope_token==q["scope_token"],"全行计数与同步scope");
    check(r.value.proposals.size()==(kind.empty()?0u:1u),"提案数量 "+kind);if(kind.empty() || r.value.proposals.empty())return;
    const auto& row=r.value.proposals[0];check(row.kind==kind && row.actor_id=="npc","原kind与永久本人 "+kind);
    if(!key.empty())check(row.params.count(key) && row.params.at(key)==expected,"参数原值 "+kind+"/"+key);
}
// 正常分支、旧优先级以及真实命令参数均由固定预期约束。
void branches() {
    auto q=request();expects(q,"office_accept","office_id",V{std::string("office")});
    q["actors"][0]["is_static"]=false;expects(q,"");q=request();q["actors"][0]["office"]["appointment_id"]="history";expects(q,"");
    q=request();q["actors"][0]["office"]["enabled"]=true;q["actors"][0]["office"]["holder_actor_id"]="other";expects(q,"");
    q["actors"][0]["office"]["holder_actor_id"]="npc";expects(q,"campaign_accept","template_id",V{std::string("order")});
    q["actors"][0]["campaigns"]=J::array({campaign()});expects(q,"move_local","place_id",V{std::string("right")});
    q["actors"][0]["location"]["place_id"]="camp";expects(q,"army_form","unit_ids",V{V::Array{V{std::string("u2")},V{std::string("u1")}}});
    q["actors"][0]["campaigns"][0]["status"]="mustered";expects(q,"move_local","place_id",V{std::string("gate")});
    q["actors"][0]["location"]["place_id"]="gate";expects(q,"campaign_depart","campaign_id",V{std::string("campaign")});
    q["actors"][0]["campaigns"][0]["status"]="operating";expects(q,"siege_start");
    q["actors"][0]["order"]["objective_kind"]="deliver_supply";expects(q,"campaign_deliver");
    q["actors"][0]["campaigns"][0]["objective_result_nonempty"]=true;expects(q,"campaign_return");
    q["actors"][0]["campaigns"][0]["objective_result_nonempty"]=false;q["actors"][0]["order"]["objective_kind"]="unknown";expects(q,"");
    q["actors"][0]["campaigns"][0]["office_enabled"]=false;expects(q,"campaign_return");
    q["actors"][0]["campaigns"][0]["army_id"]="";expects(q,"campaign_cancel");
    q["actors"][0]["campaigns"][0]["status"]="returning";expects(q,"");
    q["actors"][0]["campaigns"][0]["army_id"]="army";q["actors"][0]["campaigns"][0]["return_choice"]={{"ok",true},{"garrison_id","camp-id"},{"place_id","gate"}};expects(q,"army_return","garrison_id",V{std::string("camp-id")});
    q["actors"][0]["campaigns"][0]["status"]="awaiting_report";expects(q,"");
    q["actors"][0]["campaigns"][0]["report_roles"]=J::array({{{"role_id","far"},{"settlement_id","b"},{"context",context("foreign","far")}},{{"role_id","near"},{"settlement_id","a"},{"context",context("gate","near")}}});
    expects(q,"campaign_report","context",V{V::Dict{{"place_id",V{std::string("gate")}},{"role_id",V{std::string("near")}},{"provider_actor_id",V{std::string("provider")}}}});
    q["actors"][0]["campaigns"][0]["pending_siege"]={{"id","siege"},{"phase","planning"},{"player_siege_id",""}};expects(q,"");
    q["actors"][0]["campaigns"][0]["pending_siege"]["phase"]="result";expects(q,"siege_close","siege_id",V{std::string("siege")});
    q["actors"][0]["campaigns"][0]["pending_siege"]["player_siege_id"]="siege";expects(q,"");
    q["actors"][0]["campaigns"][0]["status"]="closed";q["actors"][0]["campaigns"][0]["pending_siege"]["player_siege_id"]="";expects(q,"");
}
// 过滤、最后匹配、一次移动及BFS先后不让适配器先计算结果。
void order_and_movement() {
    for(int which=0;which<5;++which) {auto q=request();auto& a=q["actors"][0];if(which==0)q["player_actor_id"]="npc";if(which==1)a["alive"]=false;if(which==2)a["available"]=false;if(which==3)a["action_id"]="busy";if(which==4)a["combat_occupied"]=true;expects(q,"");}
    auto q=request();q["actors"][0]["campaigns"]=J::array({campaign("operating"),campaign("closed","last")});expects(q,"");
    q["actors"][0]["campaigns"][1]["template_id"]="other";q["actors"][0]["office"]["enabled"]=true;q["actors"][0]["office"]["holder_actor_id"]="npc";expects(q,"siege_start");
    q=request();q["actors"][0]["office"]["appointing_context"]=context("camp");expects(q,"move_local","place_id",V{std::string("right")});
    q["places"][0]["passages"]=J::array({"left","right"});expects(q,"move_local","place_id",V{std::string("left")});
    q["places"][0]["passages"]=J::array({"gate"});expects(q,"");
    q=request();q["actors"][0]["location"]["place_id"]="camp";q["actors"][0]["office"]["appointing_context"]=context("foreign");expects(q,"move_local","place_id",V{std::string("gate")});
    q["actors"][0]["location"]["place_id"]="gate";expects(q,"travel","destination",V{std::string("b")});
    q["actors"][0]["location"]={{"kind","Wilderness"},{"place_id",""},{"settlement_id",""}};expects(q,"travel","destination",V{std::string("b")});
    q["actors"][0]["location"]["kind"]="OnRoad";expects(q,"");
    q["actors"][0]["location"]["place_id"]="foreign";expects(q,"office_accept");
    q["actors"][0]["office"]["appointing_context"]=context("missing");expects(q,"");
    q=request();q["actors"]=J::array({actor("z"),actor("npc")});auto r=hanmo::military_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==2 && r.value.proposals[0].actor_id=="z" && r.value.proposals[1].actor_id=="npc","不重排原Behavior数组");
}
// 后项协议损坏仍返回空结果，严格JSON与规范int64沿既有公共解析门。
void rejects(const std::string& bytes,const std::string& code) {const auto r=hanmo::military_schedule_proposals(bytes);check(!r.ok && r.error.code==code,"失败码 "+code+" 实得"+r.error.code+r.error.path);check(r.value.proposals.empty() && r.value.scope_token.empty() && r.value.evaluated_count==0,"失败不返回半份结果");}
void malformed() {
    auto q=request();q["actors"].push_back(actor("second"));q["actors"][1]["plan"]["money"]="01";rejects(q.dump(),"InvalidIntegerEncoding");
    q=request();q["actors"][0]["plan"]["money"]=9;rejects(q.dump(),"InvalidProjectionValue");
    q=request();q["format"]="wrong";rejects(q.dump(),"NativeMilitaryUnsupportedFormat");
    q=request();q["actors"].push_back(actor());rejects(q.dump(),"NativeMilitaryDuplicateActor");
    rejects("{\"x\":1,\"x\":2}","DuplicateJsonKey");rejects("{","InvalidJson");rejects(std::string(65,'[')+std::string(65,']'),"WireDepthLimit");rejects(std::string(hanmo::wire_limit+1,' '),"WireSizeLimit");
    q=request();q["actors"][0]["office"]["enabled"]=true;q["actors"][0]["office"]["holder_actor_id"]="npc";q["actors"][0]["location"]["place_id"]="camp";q["actors"][0]["campaigns"]=J::array({campaign()});q["actors"][0]["plan"]["money"]="9223372036854775807";q["actors"][0]["plan"]["food_quantity"]="-9223372036854775808";expects(q,"army_form","money",V{std::int64_t(9223372036854775807LL)});
}
}
// 只执行本片纯算法，不代替真实Godot来源与正式候选提交验证。
int main(){try{branches();order_and_movement();malformed();}catch(const std::exception& e){check(false,std::string("异常逃逸 ")+e.what());}std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}
