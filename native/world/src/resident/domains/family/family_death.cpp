// S4只登记实际生命终点；军事损失由S2决定，遗产与接续须后续独立明确程序。
#include "family_death.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
// 守卫不借死后同意或任何命令白名单；未来接续入口须另行取得在世者的合法决定。
Status guard_family_death_player_command(const CommonStore& common,const FamilyDeathPolicy& policy,const StableId& actor_id){
    if(!policy.enabled||actor_id!=common.state().metadata.player_actor_id)return common_success();
    const auto* actor=common.actor(actor_id);if(!actor)return common_failure("UnknownActor",actor_id);
    return actor->alive?common_success():common_failure("PlayerDeathContinuationRequired",actor_id);
}
namespace {
// 永久引用必须是已分配的本域ID，禁止未来ID或等值非规范数字。
bool allocated(const StableId& id,Integer next){constexpr const char* prefix="family_death:";if(id.rfind(prefix,0)!=0)return false;const auto text=id.substr(13);Integer n=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&n>0&&n<next&&std::to_string(n)==text;}
// 事实源必须在本记录之前实际写入，tick相同也不能颠倒原因和结果。
bool precedes(const CommonStore& common,const StableId& cause,const StableId& record){const auto& ids=common.state().facts.order();const auto first=std::find(ids.begin(),ids.end(),cause),last=std::find(ids.begin(),ids.end(),record);return first!=ids.end()&&last!=ids.end()&&first<last;}
}
// 当前死亡状态只是必要条件；精确战损来源、时刻和世界绑定才提供可追溯依据。
Status validate_family_death_record(const CommonStore& common,const FamilyDeathRecord& death,const FamilyDeathOriginRead& read){
    if(death.precise)return common_failure("FamilyLifeProfileMismatch");if(!read)return common_failure("FamilyDeathSourceUnavailable");const auto* actor=common.state().actors.find(death.actor_id);if(!actor)return common_failure("UnknownActor",death.actor_id);
    if(actor->alive||actor->available||actor->hp!=0)return common_failure("FamilyDeathActorStillLiving",death.actor_id);
    if(death.kind!="family_death"||death.cause_kind!="military_casualty"||death.actor_id.empty()||death.actor_id.size()>256||death.battle_id.empty()||death.battle_id.size()>256)return common_failure("InvalidFamilyDeathRecord");
    if(death.death_tick<0||death.death_tick>death.recorded_tick||death.recorded_tick>common.tick()||(actor->created_tick&&death.death_tick<*actor->created_tick)||(actor->birth_tick&&death.death_tick<*actor->birth_tick))return common_failure("InvalidFamilyDeathTime",death.actor_id);
    if(death.world_id!=common.state().identity.world_id||std::all_of(death.world_id.begin(),death.world_id.end(),[](auto byte){return byte==0;})||death.scenario_id!=common.state().metadata.scenario_id||death.scenario_id.empty())return common_failure("FamilyDeathWorldMismatch");
    const auto* source=common.state().facts.find(death.cause_fact_id);if(!source||source->domain!="military"||source->kind!="military_casualties")return common_failure("FamilyDeathCauseMissing",death.cause_fact_id);
    auto actual=read(death.actor_id);if(!actual.ok)return {false,{},actual.error};if(!actual.value)return common_failure("FamilyDeathSourceUnavailable",death.actor_id);const auto& proof=*actual.value;
    if(proof.actor_id!=death.actor_id||proof.casualty_fact_id!=death.cause_fact_id||proof.battle_id!=death.battle_id||proof.tick!=death.death_tick)return common_failure("FamilyDeathSourceMismatch",death.actor_id);
    return common_success();
}
// 不维护第二张actor索引，返回的借用只在同一不可变候选生命周期内有效。
const FamilyDeathRecord* family_death_for_actor(const FamilyDeathState& state,const StableId& actor){for(const auto& row:state.deaths.rows())if(row.second.actor_id==actor)return &row.second;return nullptr;}
// 只核已登记事件，不因为作者初态alive=false就擅自补上死亡原因或遗产资格。
Status validate_family_deaths(const CommonStore& common,const FamilyDeathState& state,const FamilyDeathOriginRead& read,bool enabled){
    if(!enabled&&!state.deaths.empty())return common_failure("FamilyDeathCapabilityRequired");std::set<StableId> actors;
    for(const auto& row:state.deaths.rows()){
        const auto* header=common.state().facts.find(row.first);if(!allocated(row.first,common.state().next_instance_id)||!header||header->domain!="family_life"||header->kind!="family_death")return common_failure("InvalidFamilyDeathFact",row.first);
        auto valid=validate_family_death_record(common,row.second,read);if(!valid.ok)return valid;if(!actors.insert(row.second.actor_id).second)return common_failure("DuplicateFamilyDeathActor",row.second.actor_id);if(!precedes(common,row.second.cause_fact_id,row.first))return common_failure("FamilyDeathBeforeCause",row.first);
    }
    for(const auto& row:common.state().facts.rows())if(row.second.domain=="family_life"&&row.second.kind=="family_death"){
        if(!enabled)return common_failure("FamilyDeathCapabilityRequired",row.first);if(!state.deaths.find(row.first))return common_failure("MissingFamilyDeathRecord",row.first);
    }
    return common_success();
}
// 稳定发布时要求所有可证死亡都已有记录；阶段批次的中间候选仍用基本账本校验。
Status validate_family_death_coverage(const CommonStore& common,const FamilyDeathState& state,const FamilyDeathOriginRead& read,bool enabled){
    auto valid=validate_family_deaths(common,state,read,enabled);if(!valid.ok||!enabled)return valid;if(!read)return common_failure("FamilyDeathSourceUnavailable");std::set<StableId> recorded;for(const auto& row:state.deaths.rows())recorded.insert(row.second.actor_id);
    for(const auto& row:common.state().actors.rows())if(!row.second.alive&&!recorded.count(row.first)){auto source=read(row.first);if(!source.ok)return {false,{},source.error};if(source.value)return common_failure("MissingFamilyDeathRecord",row.first);}
    return common_success();
}
namespace {
// 只在调用者唯一保存点内追加候选；Common不支持嵌套保存点，本函数不自行提交。
Outcome<StableId> append_death_candidate(CommonStore& common,FamilyDeathState& state,const FamilyDeathOriginRead& read,const StableId& actor){
    auto valid=validate_family_deaths(common,state,read,true);if(!valid.ok)return {false,{},valid.error};if(!read)return {false,{},common_failure("FamilyDeathSourceUnavailable").error};
    auto source=read(actor);if(!source.ok)return {false,{},source.error};if(!source.value)return {false,{},common_failure("FamilyDeathSourceUnavailable",actor).error};
    FamilyDeathRecord record;record.actor_id=actor;record.world_id=common.state().identity.world_id;record.scenario_id=common.state().metadata.scenario_id;record.cause_fact_id=source.value->casualty_fact_id;record.battle_id=source.value->battle_id;record.death_tick=source.value->tick;record.recorded_tick=common.tick();valid=validate_family_death_record(common,record,read);if(!valid.ok)return {false,{},valid.error};
    for(const auto& row:state.deaths.rows())if(row.second.actor_id==actor)return {true,row.first,{}};
    auto id=common.next_id("family_death");if(!id.ok)return {false,{},id.error};if(!state.deaths.insert(id.value,std::move(record)))return {false,{},common_failure("DuplicateFamilyDeathFact",id.value).error};valid=common.append_fact({id.value,"family_death","family_life"});if(!valid.ok)return {false,{},valid.error};valid=validate_family_deaths(common,state,read,true);if(!valid.ok)return {false,{},valid.error};return {true,id.value,{}};
}
}
// 同一轮命名伤亡共享外保存点；第二个人来源坏也不能留下第一个人的半份批次。
Outcome<std::vector<StableId>> record_family_deaths(CommonStore& common,FamilyDeathState& state,const FamilyDeathOriginRead& read,const std::vector<StableId>& actors){
    if(actors.size()>4096)return {false,{},common_failure("FamilyDeathBatchLimit").error};std::set<StableId> unique;for(const auto& actor:actors)if(actor.empty()||!unique.insert(actor).second)return {false,{},common_failure("InvalidFamilyDeathBatch",actor).error};
    auto valid=validate_family_deaths(common,state,read,true);if(!valid.ok)return {false,{},valid.error};if(actors.empty())return {true,{}, {}};
    auto point=common.begin_savepoint();if(!point.ok)return {false,{},point.error};auto next=state;std::vector<StableId> ids;ids.reserve(actors.size());
    for(const auto& actor:actors){auto result=append_death_candidate(common,next,read,actor);if(!result.ok){auto rolled=common.rollback_savepoint(point.value);return {false,{},rolled.ok?result.error:rolled.error};}ids.push_back(std::move(result.value));}
    valid=common.commit_savepoint(point.value);if(!valid.ok)return {false,{},valid.error};state=std::move(next);return {true,std::move(ids),{}};
}
// 单人入口复用同一原子批次，不建立第二层保存点或另写提交路径。
Outcome<StableId> record_family_death(CommonStore& common,FamilyDeathState& state,const FamilyDeathOriginRead& read,const StableId& actor){auto result=record_family_deaths(common,state,read,{actor});if(!result.ok)return {false,{},result.error};return {true,std::move(result.value.front()),{}};}
// clock_commit后的唯一阶段入口；先取得整批真实原因，再统一写入，不能因一个人缺来源猜出死因。
Outcome<std::vector<StableId>> record_verified_family_deaths(CommonStore& common,FamilyDeathState& state,const FamilyDeathOriginRead& read){
    if(!read)return {false,{},common_failure("FamilyDeathSourceUnavailable").error};std::set<StableId> recorded;for(const auto& row:state.deaths.rows())recorded.insert(row.second.actor_id);std::vector<StableId> pending;
    for(const auto& row:common.state().actors.rows()){
        if(row.second.alive||recorded.count(row.first))continue;auto origin=read(row.first);if(!origin.ok)return {false,{},origin.error};if(origin.value)pending.push_back(row.first);
    }
    return record_family_deaths(common,state,read,pending);
}

}
