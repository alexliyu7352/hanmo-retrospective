#include "hanmo/resident/checkpoint_memory.hpp"
// 生命周期只采用真实同意、原工单和已登记死亡；取消不改变原出生预约。
#include "actor_life_eligibility.hpp"
#include "child_care_relief_world.hpp"
#include "birth_care_cached_revision.hpp"
#include "family_care_completion_shape.hpp"
#include "child_care_relief_cache.hpp"
#include "actor_age_birth_read.hpp"
#include "actor_death_read.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/domains/interaction/interaction_phases.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 历史决定不借后来死亡抹除；实际占用由原Military/Naval纯读，不能凭当前可见位置猜空闲。
Status eligible(const WorldBundle& w,const StableId& actor,Integer tick,const StableId& fact){
    auto age=fact.empty()?read_actor_age(w.common,*w.catalog->common,w.family_birth,actor):read_actor_age_before_fact(w.common,*w.catalog->common,w.family_birth,actor,tick,fact);
    if(!age.ok)return {false,{},age.error};if(!age.value.adult)return common_failure("ChildCareReliefAdultRequired",actor);
    if(!fact.empty()){auto alive=read_actor_alive_before_fact(w.common,w.family_life,actor,tick,fact);if(!alive.ok)return {false,{},alive.error};if(!alive.value)return common_failure("ChildCareReliefParticipantDead",actor);}
    else {auto death=read_actor_death(w.common,w.family_life,actor);if(!death.ok)return {false,{},death.error};if(death.value)return common_failure("ChildCareReliefParticipantDead",actor);}
    return w.family_legacy_playable_read()(actor,tick,fact);
}
ActionCompletion completion(const WorldBundle& w,const StableId& provider,const ChildCareReliefEffect& effect){ActionCompletion out;out.completed=true;out.success=effect.success;if(!effect.code.empty())out.code=effect.code;FamilyEffect family;family.success=effect.success;family.actor_id=provider;family.fact_id=effect.fact_id;family.player_actor_id=w.common.state().metadata.player_actor_id;if(!effect.code.empty())family.code=effect.code;out.family=std::move(family);return out;}
// 完整领域史之后只读精确原Action终态，不用动作消失推断，也不接受其他回执混载。
Status check_completion(const WorldBundle& w,const StableId& action,Integer tick,const ActionCompletion& result){
    const auto* started=w.child_care_relief.starts.find(action);if(!started)return common_failure("ChildCareReliefStartRequired",action);
    auto read=read_child_care_relief_completion(w.common,w.child_care_relief,started->provider_actor_id,action);if(!read.ok)return {false,{},read.error};if(!read.value)return common_failure("ChildCareReliefCompletionSourceRequired",action);const auto& source=*read.value;
    if(source.completed_tick!=tick)return common_failure("ChildCareReliefCachedCompletionMismatch",action);
    auto player=w.family_bequest_player_read()(tick,source.fact_id);if(!player.ok)return {false,{},player.error};
    return validate_family_care_completion_shape(result,source.provider_actor_id,player.value,source.fact_id,source.success,source.code);
}
}
bool child_care_relief_command_kind(const StableId& kind){return kind=="family_care_relief_accept"||kind=="family_care_relief_withdraw"||kind=="family_child_care_relief"||kind=="family_care_relief_cancel";}
Status validate_child_care_relief_fact_header(const ChildCareReliefState& state,const FactRecord& head){
    if(head.domain!="family_care_relief")return common_failure("ChildCareReliefFactDomainMismatch",head.id);Integer count=0;
    if(const auto* p=state.agreements.find(head.id))count+=head.kind==p->kind;if(const auto* p=state.starts.find(head.id))count+=head.kind==p->kind;if(const auto* p=state.care.find(head.id))count+=head.kind==p->kind;if(const auto* p=state.closures.find(head.id))count+=head.kind==p->kind;
    return count==1?common_success():common_failure("ChildCareReliefFactPayloadMismatch",head.id);
}
ChildCareReliefAccess WorldBundle::family_care_relief_access()const{return {family_bequest_player_read(),[this](const StableId& actor,Integer tick,const StableId& fact){return eligible(*this,actor,tick,fact);}};}
Status validate_child_care_relief_world(const WorldBundle& w){
    const bool enabled=w.family_care_relief_enabled();Integer selected=0;for(const auto& module:w.catalog->identity.module_versions)if(module.first=="family_care_relief"){++selected;if(module.second!="1")return common_failure("UnsupportedChildCareReliefVersion");}
    if(enabled!=w.child_care_relief_seed_complete||selected!=(enabled?1:0)||(enabled&&(!w.family_birth_enabled()||!w.family_life_enabled())))return common_failure("ChildCareReliefProfileMismatch");
    std::set<StableId> cached_sources;
    auto history=validate_child_care_relief(w.common,*w.catalog->interaction,enabled?*w.catalog->family_care_relief:ChildCareReliefCatalog{},w.family_birth,w.family_life,w.child_care_relief,w.family_care_relief_access());if(!history.ok)return {false,{},history.error};
    auto prefix=[](const StableId& kind){return kind.rfind("family_care_relief_",0)==0||kind=="family_child_care_relief";};
    for(const auto& old:w.common.state().history)if(prefix(old.kind)&&(!enabled||!child_care_relief_command_kind(old.kind)))return common_failure("ChildCareReliefProfileRequired");
    for(const auto& row:w.common.state().results.rows())if(row.second){const auto& result=*row.second;if(prefix(result.kind)&&(!enabled||!child_care_relief_command_kind(result.kind)))return common_failure("ChildCareReliefProfileRequired");const auto* effect=result.details?std::get_if<FamilyEffect>(&result.details->data):nullptr;const bool typed=effect&&effect->fact_id&&effect->fact_id->rfind("family_care_relief:",0)==0;const bool kind=child_care_relief_command_kind(result.kind);
        if(kind||typed){if(!enabled||!kind||!result.details)return common_failure("InvalidChildCareReliefCommandReceipt",row.first);const HistoryRecord* h=nullptr;for(const auto& old:w.common.state().history)if(old.result_id==result.result_id){if(h)return common_failure("CareReliefCachedHistoryMismatch");h=&old;}if(!h||h->kind!=result.kind||h->tick!=result.tick)return common_failure("CareReliefCachedHistoryMismatch");
            if(result.kind=="family_child_care_relief"){const auto* started=std::get_if<ActionStarted>(&result.details->data);const auto* source=started?w.child_care_relief.starts.find(started->action_id):nullptr;if(!source||source->provider_actor_id!=h->actor_id||source->tick!=result.tick)return common_failure("ChildCareReliefCachedStartMismatch");if(!cached_sources.insert(source->action_id).second)return common_failure("DuplicateChildCareReliefCachedSource",source->action_id);auto valid=care_relief_detail::validate_cached_allocation(w.common,w.child_care_relief,result.result_id,source->action_id);if(!valid.ok)return valid;valid=birth_care_cache::relief_revision(w.common,w.child_care_relief,result,source->action_id);if(!valid.ok)return valid;}
            else{if(!effect||!typed)return common_failure("InvalidChildCareReliefCommandReceipt");auto valid=validate_child_care_relief_command_receipt(w.child_care_relief,result.kind,h->actor_id,result.tick,*effect);if(!valid.ok)return valid;if(!cached_sources.insert(*effect->fact_id).second)return common_failure("DuplicateChildCareReliefCachedSource",*effect->fact_id);valid=care_relief_detail::validate_cached_allocation(w.common,w.child_care_relief,result.result_id,*effect->fact_id);if(!valid.ok)return valid;valid=birth_care_cache::relief_revision(w.common,w.child_care_relief,result,*effect->fact_id);if(!valid.ok)return valid;}}
        if(result.details)if(const auto* advance=std::get_if<AdvanceDetails>(&result.details->data))for(const auto& item:advance->completed_actions.rows()){const auto* source=w.child_care_relief.starts.find(item.first);const bool typed_source=item.second.family&&item.second.family->fact_id&&item.second.family->fact_id->rfind("family_care_relief:",0)==0;if(!source&&!typed_source)continue;if(!enabled||!source)return common_failure("InvalidChildCareReliefCompletion");auto valid=check_completion(w,item.first,result.tick,item.second);if(!valid.ok)return valid;if(!cached_sources.insert(*item.second.family->fact_id).second)return common_failure("DuplicateChildCareReliefCachedSource",*item.second.family->fact_id);valid=care_relief_detail::validate_cached_allocation(w.common,w.child_care_relief,result.result_id,*item.second.family->fact_id);if(!valid.ok)return valid;valid=birth_care_cache::relief_revision(w.common,w.child_care_relief,result,*item.second.family->fact_id);if(!valid.ok)return valid;}
    }return common_success();
}
Status release_completed_child_care_relief(WorldBundle& w,const StableId& action,const ActionCompletion& outcome){
    const auto* original=w.common.action(action);if(!original||original->kind!="family_child_care_relief"||!w.child_care_relief.starts.find(action))return common_failure("ChildCareReliefOriginalActionRequired",action);auto released=release_action(w,action);if(!released.ok)return released;auto contact=w.services.interaction_access();return resume_completed_dialogues(w.common,w.interaction,*w.catalog->interaction,contact,{{action,checkpoint_make_shared<const ActionCompletion>(outcome)}});
}
Status finish_child_care_relief_world(WorldBundle& w,AdvanceDetails& result){
    if(!w.family_care_relief_enabled())return common_success();auto access=w.family_care_relief_access();const auto& catalog=*w.catalog->family_care_relief;const auto action_ids=w.common.action_order();
    for(const auto& id:action_ids){const auto* pending=w.common.action(id);if(!pending||pending->kind!="family_child_care_relief")continue;const auto action=*pending;const auto* started=w.child_care_relief.starts.find(id);if(!started)return common_failure("ChildCareReliefStartRequired",id);const auto* provider=w.common.actor(started->provider_actor_id);const auto* child=w.common.actor(started->child_actor_id);if(!provider||!child)return common_failure("ChildCareReliefParticipantUnavailable");const bool death=!provider->alive||!child->alive;if(action.remaining_ticks!=0&&!death)continue;
        auto ended=death?interrupt_child_care_relief_action_in_candidate(w.common,*w.catalog->interaction,catalog,w.family_birth,w.family_life,w.child_care_relief,access,action):finish_child_care_relief_action_in_candidate(w.common,*w.catalog->interaction,catalog,w.family_birth,w.family_life,w.child_care_relief,access,action);if(!ended.ok)return {false,{},ended.error};auto done=completion(w,action.actor_id,ended.value);if(!result.completed_actions.insert(id,done))return common_failure("DuplicateChildCareReliefCompletion",id);auto released=release_completed_child_care_relief(w,id,done);if(!released.ok)return released;
    }
    auto history=validate_child_care_relief(w.common,*w.catalog->interaction,catalog,w.family_birth,w.family_life,w.child_care_relief,access);if(!history.ok)return {false,{},history.error};const auto agreements=w.child_care_relief.agreements.order();
    for(const auto& id:agreements){if(history.value.closed_agreements.count(id)||history.value.pending_by_agreement.find(id))continue;const auto source=w.child_care_relief.agreements.find(id)->source;auto provider=read_actor_death(w.common,w.family_life,source.provider_actor_id),child=read_actor_death(w.common,w.family_life,source.child_actor_id);if(!provider.ok)return {false,{},provider.error};if(!child.ok)return {false,{},child.error};if(provider.value||child.value){auto closed=close_child_care_relief_after_death_in_candidate(w.common,*w.catalog->interaction,catalog,w.family_birth,w.family_life,w.child_care_relief,access,id);if(!closed.ok)return {false,{},closed.error};continue;}
        // 本人已明确承诺的有限工时由NPC正常业务命令继续，不代新父母或提供者签约。
        if(source.provider_actor_id==w.common.state().metadata.player_actor_id)continue;ChildCareReliefCommand care;care.kind="family_child_care_relief";care.accepted_id=id;bool applied=false;auto next=try_npc_command(w,source.provider_actor_id,{care.kind,care},applied);if(!next.ok)return next;
    }return common_success();
}
}
