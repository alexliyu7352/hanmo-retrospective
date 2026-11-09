#include "hanmo/resident/checkpoint_memory.hpp"
// 生命周期只组合原领域结果，不另设时钟、账户、NPC身份或第二份可写世界。
#include "actor_life_eligibility.hpp"
#include "family_birth_world.hpp"
#include "birth_care_cached_revision.hpp"
#include "family_care_completion_shape.hpp"
#include "child_care_relief_world.hpp"
#include "family_birth_lifecycle.hpp"
#include "family_child_maturation.hpp"
#include "actor_age_birth_read.hpp"
#include "actor_death_read.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "born_character_creation.hpp"
#include "resident/kernel/native_family_birth_state.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/domains/interaction/interaction_phases.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
// 成年、存活和具名军事处境分别读取；历史死亡之后不抹除更早的有效本人决定。
Status alive_before(const WorldBundle& w,const StableId& actor,Integer tick,const StableId& fact){
    if(!fact.empty()){auto alive=read_actor_alive_before_fact(w.common,w.family_life,actor,tick,fact);if(!alive.ok)return {false,{},alive.error};return alive.value?common_success():common_failure("BirthPersonAlreadyDead",actor);}
    auto death=read_actor_death(w.common,w.family_life,actor);if(!death.ok)return {false,{},death.error};return death.value?common_failure("BirthPersonAlreadyDead",actor):common_success();
}
Status adult_before(const WorldBundle& w,const StableId& actor,Integer tick,const StableId& fact){
    auto age=fact.empty()?read_actor_age(w.common,*w.catalog->common,w.family_birth,actor):read_actor_age_before_fact(w.common,*w.catalog->common,w.family_birth,actor,tick,fact);
    if(!age.ok)return {false,{},age.error};if(!age.value.adult)return common_failure("BirthArrangementRequiresAdults",actor);
    auto live=alive_before(w,actor,tick,fact);return live.ok?w.family_legacy_playable_read()(actor,tick,fact):live;
}
// 到期未在原照护地点或本人正在忙时继续等待，不能远程生成婴儿或吞掉内部错误。
Outcome<bool> at_birth_place(const WorldBundle& w,const StableId& actor,const StableId& place){const auto* p=w.common.actor(actor);if(!p)return {false,{},common_failure("BirthPersonSourceRequired",actor).error};if(!p->alive||!p->available||!p->action_id.empty())return {true,false,{}};auto location=w.common.resolved_location(actor);if(!location.ok)return {false,{},location.error};const auto* at=std::get_if<SettlementLocation>(&location.value);return {true,at&&at->place_id==place,{}};}
// 本批新命令没有业务重放回执；同tick别人的旧事实也不能替本次新分配来源。
Status allocated_in_result(const CommonStore& common,const StableId& result_id,const StableId& source_id,const StableId& prefix){
    auto ordinal=[](const StableId& id,const StableId& head)->Integer{if(id.rfind(head+":",0)!=0)return 0;const auto digits=id.substr(head.size()+1);Integer value=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&std::to_string(value)==digits?value:0;};
    const HistoryRecord* previous=nullptr;const HistoryRecord* selected=nullptr;const HistoryRecord* before=nullptr;
    for(const auto& history:common.state().history){if(history.result_id==result_id){if(selected)return common_failure("BirthCachedHistoryMismatch");selected=&history;before=previous;}previous=&history;}
    const auto upper=ordinal(result_id,"result"),lower=before?ordinal(before->result_id,"result"):0,source=ordinal(source_id,prefix);
    return selected&&upper>lower&&upper<common.state().next_instance_id&&(!before||lower>0)&&source>lower&&source<upper?common_success():common_failure("BirthCachedAllocationMismatch",source_id);
}
// 可复用原FamilyEffect，但每项都绑定原照护人的永久结束事实，不把孩子称为继承人。
ActionCompletion completion(const WorldBundle& w,const StableId& caregiver,const ChildCareEffect& effect){ActionCompletion result;result.completed=true;result.success=effect.success;if(!effect.code.empty())result.code=effect.code;FamilyEffect family;family.success=effect.success;family.actor_id=caregiver;family.fact_id=effect.fact_id;family.player_actor_id=w.common.state().metadata.player_actor_id;if(!effect.code.empty())family.code=effect.code;result.family=std::move(family);return result;}
// 保存的advance条目只读其原准确工单与结束事实；不以当前行动不存在代替完成证明。
Status validate_completion(const WorldBundle& w,const StableId& action,Integer tick,const ActionCompletion& result){
    const auto* start=w.family_birth.care_starts.find(action);if(!start)return common_failure("ChildCareStartRequired",action);
    auto read=read_child_care_completion(w.common,w.family_birth,start->caregiver_actor_id,action);if(!read.ok)return {false,{},read.error};if(!read.value)return common_failure("ChildCareCompletionSourceRequired",action);const auto& source=*read.value;
    if(source.completed_tick!=tick)return common_failure("ChildCareCachedCompletionMismatch",action);
    auto player=w.family_bequest_player_read()(tick,source.fact_id);if(!player.ok)return {false,{},player.error};
    return validate_family_care_completion_shape(result,source.caregiver_actor_id,player.value,source.fact_id,source.success,source.code);
}
}
bool family_birth_command_kind(const StableId& k){return k=="family_birth_receive"||k=="family_birth_reply"||k=="family_birth_withdraw"||k=="family_child_care"||k=="family_child_care_cancel";}
Status validate_birth_fact_header(const FamilyBirthState& s,const FactRecord& h){if(h.domain!="family_birth")return common_failure("BirthFactDomainMismatch",h.id);Integer count=0;
    if(const auto* p=s.proposals.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.replies.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.closures.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.births.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.care_starts.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.care.find(h.id))count+=h.kind==p->kind;if(const auto* p=s.maturations.find(h.id))count+=h.kind==p->kind;
    if(h.kind=="actor_creation")for(const auto& p:s.births.rows())count+=p.second.actor_id==h.id;
    return count==1?common_success():common_failure("BirthFactPayloadMismatch",h.id);
}
BirthArrangementAccess WorldBundle::family_birth_access()const{
    BirthArrangementAccess out;out.lineage=catalog->family_lineage.get();out.life=&family_life;out.player_before=family_bequest_player_read();
    out.decision_maker_before=[this](const StableId& actor,Integer tick,const StableId& fact){return adult_before(*this,actor,tick,fact);};
    out.care_recipient_alive_before=[this](const StableId& actor,Integer tick,const StableId& fact){return alive_before(*this,actor,tick,fact);};
    out.maturation_unoccupied_before=family_legacy_playable_read();
    out.care_relief_profile_enabled=family_care_relief_enabled();if(out.care_relief_profile_enabled)out.original_care_not_replaced_before=[this](const StableId& child,Integer tick,const StableId& fact){return validate_original_care_not_replaced(common,child_care_relief,child,tick,fact);};
    out.eligible_pair_before=[this](const StableId& first,const StableId& second,Integer tick,const StableId& fact){for(const auto* actor:{&first,&second}){auto valid=adult_before(*this,*actor,tick,fact);if(!valid.ok)return valid;}auto parents=read_birth_adoptive_parents(common,family,fact);if(!parents.ok)return Status{false,{},parents.error};return validate_birth_known_family_pair(common,*catalog->common,*catalog->family_lineage,family_birth,parents.value,first,second);};
    return out;
}
Status validate_family_birth_identity_world(const WorldBundle& w){
    const bool enabled=w.family_birth_enabled();Integer birth=0,lineage=0;for(const auto& m:w.catalog->identity.module_versions){if(m.first=="family_birth"){++birth;if(m.second!="1"&&m.second!="4")return common_failure("UnsupportedFamilyBirthVersion");}if(m.first=="family_lineage"){++lineage;if(m.second!="1")return common_failure("UnsupportedFamilyLineageVersion");}}
    if(enabled!=w.family_birth_seed_complete||enabled!=bool(w.catalog->family_lineage)||enabled!=w.catalog->common->family_birth_identity_enabled||birth!=(enabled?1:0)||lineage!=(enabled?1:0)||(enabled&&(!w.family_life_enabled()||!w.catalog->family||!w.catalog->family->enabled||!w.catalog->common->extended_skills)))return common_failure("FamilyBirthProfileMismatch");
    if(!enabled)return common_success();auto valid=validate_family_lineage_catalog(w.common,*w.catalog->common,*w.catalog->family_lineage);if(!valid.ok)return valid;if(w.catalog->family_birth->selected_version==4){auto owner=w.complete_exact_time_bindings();if(!owner.ok)return {false,{},std::move(owner.error)};if(!owner.value||!owner.value->reads().family_original_birth)return common_failure("Birth4OriginalSourcesRequired",{},ErrorKind::unsupported_capability);const auto& original=*owner.value->reads().family_original_birth;valid=validate_native_family_birth_bindings(w,original);if(!valid.ok)return valid;for(const auto& row:w.family_birth.births.rows()){const auto* actor=w.common.actor(row.second.actor_id);if(!actor)return common_failure("BiologicalActorSourceRequired");valid=validate_born_character_identity_and_placement(original,*w.actor_foundation,*w.catalog->actor_foundation,*original.access.placement,{row.first});if(!valid.ok)return valid;}return common_success();}
    auto projection=read_birth_lineage_projection(w.common,*w.catalog->common,*w.catalog->family_lineage,w.family_birth);if(!projection.ok)return {false,{},projection.error};for(const auto& row:w.family_birth.births.rows()){const auto* actor=w.common.state().actors.find(row.second.actor_id);if(!actor)return common_failure("BiologicalActorSourceRequired");valid=validate_biological_actor_identity(w.common,*w.catalog->common,*w.catalog->family,*w.catalog->family_lineage,w.family_birth,*actor);if(!valid.ok)return valid;}return common_success();
}
Status validate_family_birth_world(const WorldBundle& w){
    std::unique_ptr<CompleteExactWorldBindingOwner> birth_owner;std::optional<BirthArrangementAccess> legacy_access;const BirthArrangementAccess* access=nullptr;
    if(w.family_birth_enabled()&&w.catalog->family_birth->selected_version==4){auto acquired=w.complete_exact_time_bindings();if(!acquired.ok)return {false,{},std::move(acquired.error)};birth_owner=std::move(acquired.value);if(!birth_owner||!birth_owner->reads().family_original_birth)return common_failure("Birth4OriginalSourcesRequired",{},ErrorKind::unsupported_capability);const auto& original=*birth_owner->reads().family_original_birth;auto valid=validate_native_family_birth_bindings(w,original);if(!valid.ok)return valid;access=&original.access;}else{legacy_access.emplace(w.family_birth_access());access=&*legacy_access;}
    auto history=validate_birth_arrangements(w.common,*w.catalog->interaction,*w.catalog->family,w.catalog->family_birth?*w.catalog->family_birth:BirthArrangementCatalog{},w.family_birth,*access);if(!history.ok)return {false,{},history.error};
    for(const auto& h:w.common.state().history)if(((h.kind.rfind("family_birth_",0)==0||h.kind.rfind("family_child_",0)==0)&&!child_care_relief_command_kind(h.kind))&&(!w.family_birth_enabled()||!family_birth_command_kind(h.kind)))return common_failure("BirthProfileRequired");
    std::set<StableId> cached_sources;
    for(const auto& row:w.common.state().results.rows())if(row.second){const auto& result=*row.second;if(((result.kind.rfind("family_birth_",0)==0||result.kind.rfind("family_child_",0)==0)&&!child_care_relief_command_kind(result.kind))&&(!w.family_birth_enabled()||!family_birth_command_kind(result.kind)))return common_failure("BirthProfileRequired");const auto* effect=result.details?std::get_if<FamilyEffect>(&result.details->data):nullptr;const bool typed=effect&&effect->fact_id&&effect->fact_id->rfind("family_birth:",0)==0;const bool kind=family_birth_command_kind(result.kind);
        if(kind||typed){if(!w.family_birth_enabled()||!kind||!result.details)return common_failure("InvalidBirthCommandReceipt",row.first);const HistoryRecord* h=nullptr;for(const auto& old:w.common.state().history)if(old.result_id==result.result_id){if(h)return common_failure("BirthCachedHistoryMismatch");h=&old;}if(!h||h->kind!=result.kind||h->tick!=result.tick)return common_failure("BirthCachedHistoryMismatch");
            if(result.kind=="family_child_care"){const auto* started=std::get_if<ActionStarted>(&result.details->data);const auto* source=started?w.family_birth.care_starts.find(started->action_id):nullptr;if(!source||source->caregiver_actor_id!=h->actor_id||source->tick!=result.tick)return common_failure("ChildCareCachedStartMismatch");if(!cached_sources.insert(source->action_id).second)return common_failure("DuplicateBirthCachedSource",source->action_id);auto allocation=allocated_in_result(w.common,result.result_id,source->action_id,"action");if(!allocation.ok)return allocation;allocation=birth_care_cache::birth_revision(w.common,w.family_birth,result,source->action_id);if(!allocation.ok)return allocation;}
            else{if(!effect||!typed)return common_failure("InvalidBirthCommandReceipt");auto valid=validate_family_birth_command_receipt(w.family_birth,result.kind,h->actor_id,result.tick,*effect);if(!valid.ok)return valid;if(!cached_sources.insert(*effect->fact_id).second)return common_failure("DuplicateBirthCachedSource",*effect->fact_id);valid=allocated_in_result(w.common,result.result_id,*effect->fact_id,"family_birth");if(!valid.ok)return valid;valid=birth_care_cache::birth_revision(w.common,w.family_birth,result,*effect->fact_id);if(!valid.ok)return valid;}}
        if(result.details)if(const auto* advance=std::get_if<AdvanceDetails>(&result.details->data))for(const auto& item:advance->completed_actions.rows()){const auto* source=w.family_birth.care_starts.find(item.first);const bool family_source=item.second.family&&item.second.family->fact_id&&item.second.family->fact_id->rfind("family_birth:",0)==0;if(!source&&!family_source)continue;if(!w.family_birth_enabled()||!source)return common_failure("InvalidChildCareCompletion");auto valid=validate_completion(w,item.first,result.tick,item.second);if(!valid.ok)return valid;if(!cached_sources.insert(*item.second.family->fact_id).second)return common_failure("DuplicateBirthCachedSource",*item.second.family->fact_id);valid=allocated_in_result(w.common,result.result_id,*item.second.family->fact_id,"family_birth");if(!valid.ok)return valid;valid=birth_care_cache::birth_revision(w.common,w.family_birth,result,*item.second.family->fact_id);if(!valid.ok)return valid;}
    }return common_success();
}
Status release_completed_child_care(WorldBundle& w,const StableId& action,const ActionCompletion& outcome){
    const auto* original=w.common.action(action);if(!original||original->kind!="family_child_care"||!w.family_birth.care_starts.find(action))return common_failure("ChildCareOriginalActionRequired",action);
    auto released=release_action(w,action);if(!released.ok)return released;auto interaction=w.services.interaction_access();return resume_completed_dialogues(w.common,w.interaction,*w.catalog->interaction,interaction,{{action,checkpoint_make_shared<const ActionCompletion>(outcome)}});
}
Status finish_family_birth_world(WorldBundle& w,AdvanceDetails& result){
    if(!w.family_birth_enabled())return common_success();auto access=w.family_birth_access();const auto& catalog=*w.catalog->family_birth;
    auto ids=w.common.action_order();for(const auto& id:ids){const auto* pending=w.common.action(id);if(!pending||pending->kind!="family_child_care")continue;const auto action=*pending;const auto* start=w.family_birth.care_starts.find(id);if(!start)return common_failure("ChildCareStartRequired",id);const auto* caregiver=w.common.actor(start->caregiver_actor_id);const auto* child=w.common.actor(start->child_actor_id);if(!caregiver||!child)return common_failure("ChildCarePersonUnavailable");const bool death=!caregiver->alive||!child->alive;if(action.remaining_ticks!=0&&!death)continue;
        auto ended=death?interrupt_child_care_action_in_candidate(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access,action):finish_child_care_action_in_candidate(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access,action);if(!ended.ok)return {false,{},ended.error};auto done=completion(w,action.actor_id,ended.value);if(!result.completed_actions.insert(id,done))return common_failure("DuplicateChildCareCompletion",id);auto released=release_completed_child_care(w,id,done);if(!released.ok)return released;
    }
    auto history=validate_birth_arrangements(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access);if(!history.ok)return {false,{},history.error};const auto accepted_ids=w.family_birth.replies.order();
    for(const auto& id:accepted_ids){const auto* reply=w.family_birth.replies.find(id);if(!reply->confirmation||history.value.closed_acceptances.count(id)||history.value.born_acceptances.count(id))continue;const auto source=*reply->confirmation;bool deceased=false;for(const auto* actor:{&source.speaker_actor_id,&source.listener_actor_id}){auto death=read_actor_death(w.common,w.family_life,*actor);if(!death.ok)return {false,{},death.error};deceased=deceased||bool(death.value);}
        if(deceased){auto closed=close_unborn_arrangement_after_death_in_candidate(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access,id);if(!closed.ok)return {false,{},closed.error};continue;}
        if(reply->due_tick>w.common.tick())continue;auto first=at_birth_place(w,source.speaker_actor_id,source.terms.place_id),second=at_birth_place(w,source.listener_actor_id,source.terms.place_id);if(!first.ok)return {false,{},first.error};if(!second.ok)return {false,{},second.error};if(!first.value||!second.value)continue;
        // 原接受不免除今天的占用或近亲边界。纯读不准入继续等待，内部来源错误仍终止整个候选。
        auto eligible=access.eligible_pair_before(source.speaker_actor_id,source.listener_actor_id,w.common.tick(),{});if(!eligible.ok){if(eligible.error.kind==ErrorKind::rule)continue;return eligible;}
        auto born=fulfill_birth_arrangement_in_candidate(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,w.interaction,w.social,access,id);if(!born.ok)return {false,{},born.error};
    }
    const auto born_ids=w.family_birth.births.order();for(const auto& id:born_ids){const auto* born=w.family_birth.births.find(id);const auto* actor=w.common.actor(born->actor_id);if(!actor||!actor->alive||w.common.tick()-born->tick<18*720)continue;bool mature=false;for(const auto& old:w.family_birth.maturations.rows())mature=mature||old.second.actor_id==born->actor_id;if(mature)continue;auto registered=record_child_maturation_in_candidate(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access,born->actor_id);if(!registered.ok)return {false,{},registered.error};}
    // 原本人已接受的有限照护条款允许NPC继续履行；不替NPC签新生育同意，也不替玩家点击。
    history=validate_birth_arrangements(w.common,*w.catalog->interaction,*w.catalog->family,catalog,w.family_birth,access);if(!history.ok)return {false,{},history.error};for(const auto& id:accepted_ids){const auto* reply=w.family_birth.replies.find(id);if(!reply->confirmation||!history.value.born_acceptances.count(id)||history.value.closed_acceptances.count(id)||history.value.pending_care_actions.find(id))continue;const auto caregiver=reply->confirmation->terms.caregiver_actor_id;if(caregiver==w.common.state().metadata.player_actor_id)continue;FamilyBirthCommand care;care.kind="family_child_care";care.accepted_id=id;bool applied=false;auto r=try_npc_command(w,caregiver,{care.kind,care},applied);if(!r.ok)return r;}
    return common_success();
}
}
