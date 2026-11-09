// 闭合组合/预算/取消调度窄门；这里的叶fixture只测搬运，真实领域等价另由C测试。
#include "resident/views/player_frame_seed.hpp"
#include "resident_bridge_live_jobs.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>
using namespace hanmo::resident;
using V=BoundaryValue;using D=V::Dict;
// 最小叶fixture的标签/顺序均故意保留，用compare_boundary检查无再编码或截断。
V object(std::initializer_list<std::pair<std::string,V>> rows){return V{D{rows}};}
const StableId& text(const V& value){return *value.as<StableId>();}
struct Fixture {
    bool encounter=false,npc=false;StableId phase="scene",speaker="",failed_query="",huge_query="";bool negative_query=false;
    std::vector<StableId> queries,kinds,characters;
    V error=object({{"ok",V{false}},{"code",V{StableId{"OriginalLeafError"}}},{"path",V{StableId{"ordered/path"}}},{"command",V{StableId{"original"}}}});
    // 固定查询值只服务组合测试，不模拟游戏规则或权限。
    Outcome<V> operator()(LiveOperationKind kind,const V& payload){
        if(kind==LiveOperationKind::preview){auto name=text(*payload.find("kind"));kinds.push_back(name);return {true,error,{}};}
        assert(kind==LiveOperationKind::query);auto name=text(*payload.find("query_id"));queries.push_back(name);const auto& args=*payload.find("args");
        if(name==failed_query){if(negative_query)return {true,error,{}};return {false,{},{{"ExactDomainError","x"},ErrorKind::rule,"query","c","leaf","id"}};}
        if(name==huge_query)return {true,object({{"huge",V{StableId(player_frame_seed_byte_limit,'x')}}}),{}};
        if(name=="command_context")return {true,object({{"ok",V{true}},{"actor_id",V{StableId{"self"}}}}),{}};
        if(name=="interaction_view"){
            assert(*args.find("cursor")->as<Integer>()==0);auto result=object({{"phase",V{phase}}});
            if(!speaker.empty())result.as<D>()->emplace_back("speaker",V{speaker});
            if(phase!="scene")result.as<D>()->emplace_back("context",object({{"provider_actor_id",V{StableId{"context-person"}}}}));
            return {true,std::move(result),{}};
        }
        if(name=="journey_encounter")return {true,object({{"travel",object({{"active",V{true}}})},{"encounter",encounter?object({{"encounter_id",V{StableId{"enc:1"}}},{"provider_actor_id",V{StableId{"peer"}}}}):V{D{}}}}),{}};
        if(name=="character_input"){characters.push_back(text(*args.find("actor_id")));return {true,object({{"ok",V{true}},{"actor",*args.find("actor_id")}}),{}};}
        if(name=="journey_route"||name=="journey_crossings"||name=="journey_companions"){
            assert(*args.find("cursor")->as<Integer>()==0);return {true,object({{"cursor",V{Integer{0}}},{"total_rows",V{Integer{65}}},{"next_cursor",V{Integer{64}}},{"rows",V{V::Array{}}}}),{}};
        }
        return {true,object({{"z_first",V{Integer{7}}},{"float",V{1.0}},{"bool",V{true}},{"a_last",V{StableId{name}}}}),{}};
    }
};
// 只为队列调度构造已完成值，不声明任何半日推进。
Outcome<LiveOperationResult> completed(const WorldIdentity& source,V value){return {true,{source,source,std::move(value)},{}};}
int main(){
    Fixture f;auto result=query_player_frame_seed([&](auto k,const auto& p){return f(k,p);},false);assert(result.ok&&*result.value.find("ok")->as<bool>());
    assert(f.queries.size()==15&&f.kinds.size()==7&&f.characters==std::vector<StableId>{"self"});
    auto* journey=result.value.find("journey");assert(journey->as<D>()->size()==8);assert(journey->find("journey_route")->find("next_cursor"));
    auto expected=object({{"z_first",V{Integer{7}}},{"float",V{1.0}},{"bool",V{true}},{"a_last",V{StableId{"journey_travel"}}}});assert(compare_boundary(*journey->find("journey_travel"),expected).exact());
    for(const auto& quote:*result.value.find("quotes")->as<V::Array>())assert(compare_boundary(*quote.find("result"),f.error).exact());
    assert(text(result.value.find("quotes")->as<V::Array>()->front().as<D>()->front().second)=="advance");
    Fixture encounter;encounter.encounter=true;encounter.phase="say";encounter.speaker="speaker";
    auto r=query_player_frame_seed([&](auto k,const auto& p){return encounter(k,p);},true);assert(r.ok);assert(encounter.characters==std::vector<StableId>({"self","peer","speaker"}));assert(encounter.kinds.size()==11&&encounter.kinds[9]=="ask_news"&&encounter.kinds[10]=="end_interaction");
    for(const char* id:{"journey_companions","journey_logistics","journey_travel"})assert(!r.value.find("journey")->find(id)&&std::find(encounter.queries.begin(),encounter.queries.end(),id)==encounter.queries.end());
    const auto& talk=(*r.value.find("quotes")->as<V::Array>())[7];assert(text(*talk.find("params")->find("encounter_id"))=="enc:1");
    Fixture duplicate;duplicate.encounter=true;duplicate.phase="say";duplicate.speaker="peer";assert(query_player_frame_seed([&](auto k,const auto& p){return duplicate(k,p);},false).ok);assert(duplicate.characters.size()==2&&std::find(duplicate.kinds.begin(),duplicate.kinds.end(),"ask_news")==duplicate.kinds.end());
    Fixture fallback;fallback.phase="action";assert(query_player_frame_seed([&](auto k,const auto& p){return fallback(k,p);},false).ok);assert(fallback.characters.back()=="context-person"&&fallback.kinds[7]=="dialogue_confirm");
    Fixture bad;bad.failed_query="screen_context";auto fail=query_player_frame_seed([&](auto k,const auto& p){return bad(k,p);},false);assert(!fail.ok&&fail.error.original.code=="ExactDomainError"&&fail.error.source_id=="id"&&bad.queries.size()==3&&bad.kinds.empty());
    bad.queries.clear();bad.negative_query=true;fail=query_player_frame_seed([&](auto k,const auto& p){return bad(k,p);},false);assert(fail.ok&&compare_boundary(fail.value,bad.error).exact()&&bad.queries.size()==3&&!fail.value.find("views"));
    Fixture huge;huge.huge_query="action_context";fail=query_player_frame_seed([&](auto k,const auto& p){return huge(k,p);},false);assert(!fail.ok&&fail.error.original.code=="PlayerFrameSeedLimit"&&huge.queries.size()==2&&huge.kinds.empty());
    // batch只是原query槽：执行中的固定读完成后优先取消，两个回执都保留，无grant被生成。
    WorldIdentity source;source.execution_epoch=1;ResidentLiveJobs jobs;jobs.bind(source);
    auto run=V{D{{"run_id",V{StableId{"run:batch"}}}}};assert(jobs.enqueue({LiveOperationKind::begin_run,source,run},1).ok);auto begin=jobs.take();assert(begin);jobs.complete(1,completed(source,object({{"ok",V{true}},{"stop",V{StableId{"awaiting_permit"}}}})),source);assert(jobs.poll(source,1).ok);
    assert(jobs.enqueue({LiveOperationKind::query,source,object({{"query_id",V{StableId{"player_frame_seed"}}},{"args",V{D{}}}})},2).ok);auto batch=jobs.take();assert(batch&&batch->operation.kind==LiveOperationKind::query);
    assert(jobs.enqueue({LiveOperationKind::cancel_run,source,run},3).ok);assert(!jobs.take());jobs.complete(2,completed(source,result.value),source);auto cancel=jobs.take();assert(cancel&&cancel->operation_id==3);jobs.complete(3,completed(source,object({{"ok",V{true}},{"stop",V{StableId{"cancelled"}}}})),source);assert(jobs.poll(source,2).value.ready&&jobs.poll(source,3).value.ready&&!jobs.has_work());
    std::cout<<"player frame seed composition/budget/cancel checks passed; actual domain parity tested separately\n";
}
