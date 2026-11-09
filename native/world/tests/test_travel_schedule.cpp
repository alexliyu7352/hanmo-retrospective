// 行程纯算法的独立有界wire合同；人工夹具不替代旧GDScript全数组oracle。
#include "hanmo/travel_schedule.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <limits>
using J=nlohmann::json;
namespace {
int checks=0,failures=0;
// 每条断言均计入退出码，失败必须显示真实码或字段。
void check(bool ok,const std::string& label) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<label<<'\n'; } }
// 最小合法作者行程保持目的地重复项及原序，不模拟世界Schema。
J actor(const std::string& id="traveler") {
    return {{"actor_id",id},{"action_id",""},{"alive",true},{"available",true},
        {"location",{{"kind","Settlement"},{"settlement_id","a"},{"place_id","gate"},{"entry_place","gate"}}},
        {"itinerary",{{"index","0"},{"ready_tick","7"}}},{"destinations",J::array({"a","b"})}};
}
// 协议只含候选算法读集，无工件、世界身份或持久运行实例。
J request() { return {{"format","HM-NATIVE-TRAVEL-SCHEDULE/1"},{"scope_token","call:7"},{"tick","7"},
    {"player_actor_id","player"},{"actors",J::array({actor()})},{"parties",J::array()},{"interactions",J::array()}}; }
// 成功与失败均从公共纯函数读取，失败结果不能保留前面已算出的任何提案。
void rejects(const std::string& bytes,const std::string& code,const std::string& path) {
    const auto r=hanmo::travel_schedule_proposals(bytes);
    check(!r.ok && r.error.code==code && r.error.path==path,"拒绝 "+code+" 实得 "+r.error.code+r.error.path);
    check(r.value.proposals.empty() && r.value.scope_token.empty() && r.value.evaluated_count==0,"失败不泄漏部分批次");
}
// 固定数组语义证明当前城只跳一次；玩家、原行序、负安全索引保持旧语言行为。
void proposals() {
    auto q=request(); const auto wire=q.dump(); auto r=hanmo::travel_schedule_proposals(wire);
    check(r.ok,"最小原行程成功 "+r.error.code+r.error.path);
    if(!r.ok)return;
    check(r.value.scope_token=="call:7" && r.value.evaluated_count==1,"同步scope与全部输入行计数");
    check(r.value.proposals.size()==1 && r.value.proposals[0].actor_id=="traveler" && r.value.proposals[0].destination=="b" && r.value.proposals[0].next_index==0,"单次跳当前城再返回原travel目的地");
    check(q.dump()==wire,"输入原字节不变");
    q["actors"][0]["destinations"]=J::array({"a","a"});r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.empty(),"两个重复当前城不提案");
    q["actors"][0]["destinations"]=J::array({"a","a","b"});r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.empty(),"三个目的地禁止循环跳重复城");
    q["actors"][0]["destinations"]=J::array({"b"});r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1 && r.value.proposals[0].next_index==0,"单一其他城提案取模归零");
    q["actors"][0]["destinations"]=J::array({"a"});r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.empty(),"单一当前城不提案");
    q=request();q["actors"]=J::array({actor("z"),actor("player"),actor("a")});r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.evaluated_count==3 && r.value.proposals.size()==2 && r.value.proposals[0].actor_id=="z" && r.value.proposals[1].actor_id=="a","保原Behavior行序而非C++重排");
    q=request();q["actors"][0]["itinerary"]["index"]="-1";r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1 && r.value.proposals[0].destination=="b" && r.value.proposals[0].next_index==0,"Godot安全负数组索引不预跑Schema");
    q=request();q["actors"][0]["destinations"]=J::array({"a","b","c"});q["actors"][0]["itinerary"]["index"]="-2";r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1 && r.value.proposals[0].destination=="b" && r.value.proposals[0].next_index==-1,"负索引next_index保原有符号余数");
    r.value.proposals.clear();check(hanmo::travel_schedule_proposals(wire).value.proposals.size()==1,"返回副本及调用之间无持久状态");
}
// 每个早退都配无法解引用的目的地，证明不提前做后续索引或世界范围验证。
void filters() {
    for(int which=0;which<12;++which) {
        auto q=request();auto& a=q["actors"][0];a["destinations"]=J::array();a["itinerary"]["index"]="9223372036854775807";
        switch(which) {
            case 0:q["player_actor_id"]="traveler";break;
            case 1:a["itinerary"]=nullptr;break;
            case 2:a["action_id"]="busy";break;
            case 3:a["itinerary"]["ready_tick"]="8";break;
            case 4:q["parties"]=J::array({{{"id","party"},{"members",J::array({"traveler"})}}});break;
            case 5:a["alive"]=false;break;
            case 6:a["available"]=false;break;
            case 7:a["location"]={{"kind","OnRoad"}};break;
            case 8:a["location"]["place_id"]="inside";break;
            case 9:q["interactions"]=J::array({{{"actor_id","traveler"},{"provider_actor_id","other"}}});break;
            case 10:q["interactions"]=J::array({{{"actor_id","other"},{"provider_actor_id","traveler"}}});break;
            case 11:q["tick"]="2147483647";a["itinerary"]["ready_tick"]="2147483648";break;
        }
        const auto r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.empty() && r.value.evaluated_count==1,"按原过滤早退 "+std::to_string(which)+" "+r.error.code+r.error.path);
    }
    auto q=request();q["parties"]=J::array({{{"id",""},{"members",J::array({"traveler"})}},{{"id","later"},{"members",J::array({"traveler"})}}});auto r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1,"party首匹配空ID保原迭代短路");
    q=request();q["tick"]="9223372036854775807";q["actors"][0]["itinerary"]["ready_tick"]="9223372036854775807";r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1,"int64最大ready相等仍可出发");
    q["tick"]="-9223372036854775808";q["actors"][0]["itinerary"]["ready_tick"]="-9223372036854775808";r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.size()==1,"完整int64最小值不提前旧Schema拒绝");
    q=request();q["actors"]=J::array();r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.proposals.empty() && r.value.evaluated_count==0,"合法空全批保持成功");
}
// 封闭结构、规范十进制、严格JSON以及后项错误均需全批失败。
void malformed() {
    auto q=request();q["format"]="wrong";rejects(q.dump(),"NativeTravelUnsupportedFormat","/travel_schedule/format");
    q=request();q["unexpected"]=true;rejects(q.dump(),"UnsupportedProjectionField","/travel_schedule/unexpected");
    q=request();q.erase("tick");rejects(q.dump(),"MissingProjectionField","/travel_schedule/tick");
    for(const std::string value:{"01","+1","-0","1.0","9223372036854775808","-9223372036854775809"}) {q=request();q["tick"]=value;rejects(q.dump(),"InvalidIntegerEncoding","/travel_schedule/tick");}
    q=request();q["tick"]=7;rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/tick");
    q=request();q["actors"][0]["alive"]=1;rejects(q.dump(),"NativeTravelInvalidValue","/travel_schedule/actors/0/alive");
    q=request();q["actors"][0]["destinations"]=J::array();rejects(q.dump(),"NativeTravelDestinationIndex","/travel_schedule/actors/0/itinerary/index");
    q=request();q["actors"].push_back(actor("later"));q["actors"][1]["itinerary"]["index"]="2";rejects(q.dump(),"NativeTravelDestinationIndex","/travel_schedule/actors/1/itinerary/index");
    q=request();q["actors"][0]["itinerary"]["index"]="-3";rejects(q.dump(),"NativeTravelDestinationIndex","/travel_schedule/actors/0/itinerary/index");
    q=request();q["actors"].push_back(actor());rejects(q.dump(),"NativeTravelDuplicateActor","/travel_schedule/actors/1/actor_id");
    rejects("{\"tick\":\"1\",\"ti\\u0063k\":\"2\"}","DuplicateJsonKey","/travel_schedule");
    rejects("{", "InvalidJson","/travel_schedule");
    rejects(std::string(65,'[')+std::string(65,']'),"WireDepthLimit","/travel_schedule");
    rejects(std::string(hanmo::wire_limit+1,' '),"WireSizeLimit","/travel_schedule");
    q=request();q["actors"][0]["itinerary"]["index"]="-9223372036854775808";rejects(q.dump(),"NativeTravelDestinationIndex","/travel_schedule/actors/0/itinerary/index");
    q=request();q["actors"][0]["itinerary"]["index"]="9223372036854775807";rejects(q.dump(),"NativeTravelDestinationIndex","/travel_schedule/actors/0/itinerary/index");
    q=request();q["actors"][0]["itinerary"]["index"]=0;rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/actors/0/itinerary/index");
    q=request();q["actors"][0]["itinerary"]["ready_tick"]=7;rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/actors/0/itinerary/ready_tick");
    q=request();q["actors"][0]["location"]["entry_place"]=true;rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/actors/0/location/entry_place");
    q=request();q["actors"][0]["location"]["facts"]=J::object();rejects(q.dump(),"UnsupportedProjectionField","/travel_schedule/actors/0/location/facts");
    q=request();q["actors"][0]["location"]={{"kind","OnRoad"},{"settlement_id","a"}};rejects(q.dump(),"UnsupportedProjectionField","/travel_schedule/actors/0/location/settlement_id");
    q=request();q["actors"][0]["destinations"]=J::array({"a",true});rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/actors/0/destinations/1");
    q=request();q["parties"]=J::array({{{"id","party"},{"members",J::array({true})}}});rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/parties/0/members/0");
    q=request();q["interactions"]=J::array({{{"actor_id","other"},{"provider_actor_id",true}}});rejects(q.dump(),"InvalidProjectionValue","/travel_schedule/interactions/0/provider_actor_id");
    q=request();q["scope_token"]="temporary:第二半日";auto r=hanmo::travel_schedule_proposals(q.dump());check(r.ok && r.value.scope_token=="temporary:第二半日","临时scope原Unicode字节回显");
    check(hanmo::travel_schedule_proposals(request().dump()).ok,"失败后下次合法纯函数仍成功");
}
}
// 仅运行本片纯算法合同，完整世界与Bridge门由中央独立执行。
int main() { try { proposals();filters();malformed(); } catch(const std::exception& e) { check(false,std::string("异常逃逸: ")+e.what()); } std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0; }
