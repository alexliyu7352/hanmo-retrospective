#include "item_condition_contract.hpp"
#include "item_condition_head.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <utility>
#include <algorithm>
#include <limits>
#include <type_traits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status s){if(!s.ok)throw Failure{s.error};}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
Integer ordinal(const ItemUseSource&s){return std::visit([](const auto&v)->Integer{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,BattleWear>)return v.usage_ordinal;else if constexpr(std::is_same_v<T,InfusionFailureWear>)return v.item_slot_ordinal;else return v.tool_usage_ordinal;},s);}
const StableId& result_id(const ItemUseSource&s){return std::visit([](const auto&v)->const StableId&{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,BattleWear>)return v.result_ref.id;else if constexpr(std::is_same_v<T,GatherToolWear>)return v.gather_receipt_ref.id;else if constexpr(std::is_same_v<T,InfusionFailureWear>)return v.infusion_result_ref.id;else return v.facility_attack_result_id;},s);}
bool same_key(const ItemUseSource&a,const ItemUseSource&b){return a.index()==b.index()&&result_id(a)==result_id(b)&&ordinal(a)==ordinal(b);}
bool same_refs(const std::vector<SourceRef>&a,const std::vector<SourceRef>&b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_source_ref(a[i],b[i]))return false;return true;}
void refs(const std::vector<SourceRef>&v){for(std::size_t i=0;i<v.size();++i){if(!valid_source_ref(v[i]))fail("InvalidItemConditionResourceSource",v[i].id);for(std::size_t j=0;j<i;++j)if(same_source_ref(v[i],v[j]))fail("DuplicateItemConditionResourceSource",v[i].id);}}
void time(const ItemConditionReadContext&x,const WorldMoment&at){take(compare_world_moments(at,at));if(compare(at,take(x.clock.read_current_moment()))>0)fail("FutureItemConditionSource");}
ActualItemUse original_use(const ItemConditionReadContext&x,const ItemUseSource&s,const StableId&instance){
 take(validate_item_use_source(s));auto a=take(std::visit([&](const auto&v)->Outcome<ActualItemUse>{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,BattleWear>)return x.original.read_battle_wear(v,instance);else if constexpr(std::is_same_v<T,GatherToolWear>)return x.original.read_gather_tool_wear(v,instance);else if constexpr(std::is_same_v<T,InfusionFailureWear>)return x.original.read_infusion_failure_wear(v,instance);else return x.original.read_facility_attack_tool_wear(v,instance);},s));
 if(!same_item_use_source(a.source,s)||a.instance_id!=instance||a.version_before<1||a.damage_units<0||a.authorized_damage_limit<0||a.damage_units>a.authorized_damage_limit||!valid_source_ref(a.source_fact_ref)||!valid_source_ref(a.source_activity_ref))fail("ItemUseOriginalSourceMismatch",instance);
 std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,BattleWear>){if(!same_source_ref(a.source_fact_ref,v.result_ref)||!same_source_ref(a.source_activity_ref,v.battle_or_session_ref))fail("ItemUseOriginalSourceMismatch",instance);}else if constexpr(std::is_same_v<T,GatherToolWear>){if(!same_source_ref(a.source_fact_ref,v.gather_receipt_ref))fail("ItemUseOriginalSourceMismatch",instance);}else if constexpr(std::is_same_v<T,InfusionFailureWear>){if(!same_source_ref(a.source_fact_ref,v.infusion_result_ref))fail("ItemUseOriginalSourceMismatch",instance);}else if(a.source_fact_ref.id!=v.facility_attack_result_id||a.source_activity_ref.id!=v.episode_id)fail("ItemUseOriginalSourceMismatch",instance);},s);
 refs(a.resource_effect_refs);time(x,a.occurred_at);return a;
}
ActualItemRepairCompletion original_repair(const ItemConditionReadContext&x,const SourceRef&s,const StableId&instance){
 if(!valid_source_ref(s))fail("ItemRepairOriginalSourceRequired",instance);auto a=take(x.original.read_repair_completed(s,instance));
 if(!same_source_ref(a.source_fact_ref,s)||a.instance_id!=instance||a.version_before<1||!valid_source_ref(a.source_activity_ref)||a.repair_policy_id.empty()||a.max_durability_after<=0||a.durability_after<0||a.durability_after>a.max_durability_after)fail("ItemRepairOriginalSourceMismatch",instance);
 refs(a.resource_effect_refs);time(x,a.occurred_at);return a;
}
// The same original reducer serves the real writer and cold verification.
// Historical reconstruction owns no copied Item identity, source or repair list.
struct ConditionCursor {
 const ItemInstance& original;
 Integer instance_version,durability,max_durability;
 ItemInstanceState state=ItemInstanceState::live;
};
ConditionCursor current_cursor(const ItemInstance&i){return {i,i.instance_version,i.durability,i.max_durability,i.state};}
struct ExpectedConditionResult {
 const StableId& result_id;const StableId& instance_id;
 ItemConditionSourceKind source_kind;
 const ItemUseSource* use_source;
 const SourceRef& source_fact_ref;const SourceRef& source_activity_ref;
 std::optional<Integer> usage_ordinal;Integer effective_tick;
 Integer instance_version_before,instance_version_after,durability_before,durability_after;
 Integer max_durability_before,max_durability_after;
 const std::vector<SourceRef>& resource_effect_refs;
 ItemConditionDisposition disposition=ItemConditionDisposition::no_change;
 std::optional<std::string_view> reason;
};
ExpectedConditionResult expected_use(const ActualItemUse&a,const ConditionCursor&i,const StableId&id){
 ExpectedConditionResult r{id,i.original.id,ItemConditionSourceKind::use_damage,&a.source,a.source_fact_ref,a.source_activity_ref,ordinal(a.source),a.occurred_at.half_day_tick,i.instance_version,i.instance_version,i.durability,a.damage_units>=i.durability?0:i.durability-a.damage_units,i.max_durability,i.max_durability,a.resource_effect_refs};
 const bool changed=r.durability_before!=r.durability_after;if(changed&&i.instance_version==std::numeric_limits<Integer>::max())fail("ItemInstanceVersionOverflow",i.original.id);r.instance_version_after=i.instance_version+(changed?1:0);r.disposition=changed?ItemConditionDisposition::applied:ItemConditionDisposition::no_change;if(!changed)r.reason=a.damage_units==0?"zero_damage":"already_unusable";return r;
}
ExpectedConditionResult expected_repair(const ActualItemRepairCompletion&a,const ConditionCursor&i,const StableId&id){
 if(a.repair_policy_id!=i.original.repair_policy_id)fail("ItemRepairPolicyMismatch",i.original.id);ExpectedConditionResult r{id,i.original.id,ItemConditionSourceKind::repair_completed,nullptr,a.source_fact_ref,a.source_activity_ref,{},a.occurred_at.half_day_tick,i.instance_version,i.instance_version,i.durability,a.durability_after,i.max_durability,a.max_durability_after,a.resource_effect_refs};
 const bool changed=r.durability_before!=r.durability_after||r.max_durability_before!=r.max_durability_after;if(changed&&i.instance_version==std::numeric_limits<Integer>::max())fail("ItemInstanceVersionOverflow",i.original.id);r.instance_version_after=i.instance_version+(changed?1:0);r.disposition=changed?ItemConditionDisposition::applied:ItemConditionDisposition::no_change;if(!changed)r.reason="already_at_repair_target";return r;
}
// Only the actual writer materializes the unchanged durable result. Its fresh
// row/vector allocation remains a separate original producer ownership frontier.
ItemConditionResultView materialize_result(const ExpectedConditionResult&v){
 ItemConditionResultView r;r.result_id=v.result_id;r.instance_id=v.instance_id;r.source_kind=v.source_kind;if(v.use_source)r.use_source=*v.use_source;r.source_fact_ref=v.source_fact_ref;r.source_activity_ref=v.source_activity_ref;r.usage_ordinal=v.usage_ordinal;r.effective_tick=v.effective_tick;r.instance_version_before=v.instance_version_before;r.instance_version_after=v.instance_version_after;r.durability_before=v.durability_before;r.durability_after=v.durability_after;r.max_durability_before=v.max_durability_before;r.max_durability_after=v.max_durability_after;r.resource_effect_refs=v.resource_effect_refs;r.disposition=v.disposition;if(v.reason)r.reason.emplace(*v.reason);return r;
}
bool same_result(const ItemConditionResultView&a,const ExpectedConditionResult&b){return a.result_id==b.result_id&&a.instance_id==b.instance_id&&a.source_kind==b.source_kind&&a.use_source.has_value()==(b.use_source!=nullptr)&&(!a.use_source||same_item_use_source(*a.use_source,*b.use_source))&&same_source_ref(a.source_fact_ref,b.source_fact_ref)&&same_source_ref(a.source_activity_ref,b.source_activity_ref)&&a.usage_ordinal==b.usage_ordinal&&a.effective_tick==b.effective_tick&&a.instance_version_before==b.instance_version_before&&a.instance_version_after==b.instance_version_after&&a.durability_before==b.durability_before&&a.durability_after==b.durability_after&&a.max_durability_before==b.max_durability_before&&a.max_durability_after==b.max_durability_after&&same_refs(a.resource_effect_refs,b.resource_effect_refs)&&a.disposition==b.disposition&&a.reason.has_value()==b.reason.has_value()&&(!a.reason||*a.reason==*b.reason);}
template<class T>void update_values(T&i,const ItemConditionResultView&r){i.instance_version=r.instance_version_after;i.durability=r.durability_after;i.max_durability=r.max_durability_after;}
void update(ItemInstance&i,const ItemConditionResultView&r){update_values(i,r);if(r.source_kind==ItemConditionSourceKind::repair_completed)i.repair_fact_refs.push_back(r.source_fact_ref.id);}
WorldMoment original_result(const ItemConditionReadContext&x,const ItemConditionResultView&r,const ConditionCursor&before){
 take(validate_item_condition_result_shape(r));if(r.instance_version_before!=before.instance_version||r.durability_before!=before.durability||r.max_durability_before!=before.max_durability)fail("ItemConditionHistoryGap",r.result_id);
 if(r.source_kind==ItemConditionSourceKind::use_damage){auto a=original_use(x,*r.use_source,r.instance_id);if(a.version_before!=before.instance_version||!same_result(r,expected_use(a,before,r.result_id)))fail("ItemConditionOriginalResultMismatch",r.result_id);return a.occurred_at;}
 auto a=original_repair(x,r.source_fact_ref,r.instance_id);if(a.version_before!=before.instance_version||!same_result(r,expected_repair(a,before,r.result_id)))fail("ItemConditionOriginalResultMismatch",r.result_id);return a.occurred_at;
}
std::pair<const ItemOutputAdoption*,ItemOutputSourceView> creation_source(const ItemConditionReadContext&x,const ItemInstance&current){
 const ItemOutputAdoption* creation=nullptr;for(const auto&a:x.state.output_adoptions)if(a.instance_id&&*a.instance_id==current.id){if(creation)fail("DuplicateItemCreationSource",current.id);creation=&a;}
 if(!creation)fail("ItemCreationSourceRequired",current.id);auto output=take(x.outputs.read_item_output(creation->key,creation->source_kind));
 if(!output.unique_instance||output.quantity!=1||output.max_durability<=0||output.definition_id!=current.item_definition_id||!same_source_ref(output.key.source_ref,creation->key.source_ref)||output.key.output_ordinal!=creation->key.output_ordinal||output.key.source_schema!=creation->key.source_schema||output.produced_tick<0||output.source_kind!=creation->source_kind||creation->original_quantity!=1||creation->original_lot_id.empty()||!output.quality_score||!output.maker_ref||current.quality_score!=*output.quality_score||current.maker_ref.kind!=output.maker_ref->kind||current.maker_ref.id!=output.maker_ref->id||current.repair_policy_id!=output.repair_policy_id||current.creation_fact_id!=creation->key.source_ref.id)fail("ItemCreationSourceMismatch",current.id);
 return {creation,std::move(output)};
}
void validate_instance(const ItemConditionReadContext&x,const ItemInstance&current){
 const auto creation=creation_source(x,current);const auto&output=creation.second;
 ConditionCursor reconstructed{current,1,output.max_durability,output.max_durability};std::optional<WorldMoment> previous;std::size_t repair_count=0;bool repair_refs_match=true;
 for(std::size_t index=0;index<x.state.condition_results.size();++index){const auto&r=x.state.condition_results[index];if(r.instance_id!=current.id)continue;
 for(std::size_t prior=0;prior<index;++prior){const auto&old=x.state.condition_results[prior];if(old.result_id==r.result_id)fail("DuplicateItemConditionResult",r.result_id);if(old.instance_id!=r.instance_id||old.source_kind!=r.source_kind)continue;if((old.use_source&&r.use_source&&same_key(*old.use_source,*r.use_source))||(!old.use_source&&!r.use_source&&same_source_ref(old.source_fact_ref,r.source_fact_ref)))fail("DuplicateItemConditionUse",r.result_id);}
 const auto at=original_result(x,r,reconstructed);if(at.half_day_tick<output.produced_tick||(previous&&compare(at,*previous)<0))fail("ItemConditionSourceOrderMismatch",r.result_id);previous=at;update_values(reconstructed,r);if(r.source_kind==ItemConditionSourceKind::repair_completed){if(repair_count>=current.repair_fact_refs.size()||current.repair_fact_refs[repair_count]!=r.source_fact_ref.id)repair_refs_match=false;++repair_count;}}
 const ItemTerminationResult* terminal=nullptr;for(const auto&t:x.state.terminations)if(t.instance_id==current.id){if(terminal)fail("DuplicateItemTermination",current.id);terminal=&t;}
 if(terminal){auto original=take(x.outputs.read_item_termination(terminal->cause_ref,terminal->cause_schema,terminal->usage_ordinal,current.id));if(!same_source_ref(original.source_ref,terminal->cause_ref)||original.source_schema!=terminal->cause_schema||original.usage_ordinal!=terminal->usage_ordinal||original.instance_id!=current.id||original.effective_tick!=terminal->effective_tick||terminal->effective_tick>take(x.clock.read_current_moment()).half_day_tick)fail("ItemTerminationSourceMismatch",current.id);if(terminal->instance_version_before!=reconstructed.instance_version||reconstructed.instance_version==std::numeric_limits<Integer>::max()||terminal->instance_version_after!=reconstructed.instance_version+1||(previous&&previous->half_day_tick>terminal->effective_tick))fail("ItemTerminationHistoryGap",current.id);reconstructed.instance_version=terminal->instance_version_after;reconstructed.state=ItemInstanceState::destroyed;}
 if(current.instance_version!=reconstructed.instance_version||current.durability!=reconstructed.durability||current.max_durability!=reconstructed.max_durability||current.state!=reconstructed.state||(!repair_refs_match||repair_count!=current.repair_fact_refs.size()))fail("ItemConditionCurrentStateMismatch",current.id);
}
void stamp(const ItemConditionReadContext&x,const ReadStamp&s,const ReadStampAuthority&a,const DomainReadVersionSource&versions,const StableId&instance,Integer expected,const SourceRef&source){
 if(s.mode()!=ReadMode::committed_current&&s.mode()!=ReadMode::phase_current)fail("ItemConditionCurrentStampRequired",instance);take(a.validate(s,x.common.state().identity,s.mode(),x.common,versions));
 bool found=false;for(const auto&v:s.domains())if(v.domain=="craft"&&v.kind=="item_instance"&&v.id==instance){if(found||v.revision!=static_cast<std::uint64_t>(expected))fail("ItemConditionStampVersionMismatch",instance);found=true;}
 if(!found||std::find(s.source_ids().begin(),s.source_ids().end(),source.id)==s.source_ids().end())fail("ItemConditionSourceStampRequired",instance);
}
ItemInstance& current_instance(CommonStore&c,ItemInstancesState&state,const ItemConditionReadContext&x,const StableId&id,Integer version){
 if(&c!=&x.common||&state!=&x.state)fail("ItemConditionCandidateMismatch",id);auto*i=state.instances.find(id);if(!i||i->state!=ItemInstanceState::live)fail("LiveItemInstanceRequired",id);validate_instance(x,*i);if(version<1||version!=i->instance_version)fail("StaleItemInstance",id);
 std::size_t count=0;for(const auto&row:c.state().inventories.rows())for(const auto&lot:row.second.lots)if(lot.item_instance_id&&*lot.item_instance_id==id){if(lot.quantity!=1||lot.item_id!=i->item_definition_id||lot.lot_properties_ref)fail("UniqueInstanceLotMismatch",id);++count;}if(count!=1)fail("UniqueInstanceLotMismatch",id);return *i;
}
}
namespace {
CheckpointCopyLease reserve_repair_output(const ActualItemRepairCompletionBuildView&v){return CheckpointCopyLease::reserve();}
SourceRef copy_repair_source(const SourceRefBuildView&v){return {StableId(v.domain),StableId(v.kind),StableId(v.id)};}
std::vector<SourceRef> copy_repair_effects(const ActualItemRepairResourceEffectsBuildView&v){std::vector<SourceRef>out;out.reserve(v.values.size());for(const auto&source:v.values)out.push_back(copy_repair_source(source));return out;}
}
ActualItemRepairCompletion::ActualItemRepairCompletion(const ActualItemRepairCompletionBuildView&v):memory_(reserve_repair_output(v)),source_fact_ref(copy_repair_source(v.source_fact_ref)),source_activity_ref(copy_repair_source(v.source_activity_ref)),instance_id(v.instance_id),repair_policy_id(v.repair_policy_id),version_before(v.version_before),durability_after(v.durability_after),max_durability_after(v.max_durability_after),occurred_at(v.occurred_at),resource_effect_refs(copy_repair_effects(v.resource_effect_refs)){}
ActualItemRepairCompletion::ActualItemRepairCompletion(ActualItemRepairCompletion&&v):memory_(v.memory_),source_fact_ref(std::move(v.source_fact_ref)),source_activity_ref(std::move(v.source_activity_ref)),instance_id(std::move(v.instance_id)),repair_policy_id(std::move(v.repair_policy_id)),version_before(v.version_before),durability_after(v.durability_after),max_durability_after(v.max_durability_after),occurred_at(std::move(v.occurred_at)),resource_effect_refs(std::move(v.resource_effect_refs)){v.memory_={};}
bool same_item_use_source(const ItemUseSource&a,const ItemUseSource&b){if(a.index()!=b.index())return false;return std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;const auto&y=std::get<T>(b);if constexpr(std::is_same_v<T,BattleWear>)return same_source_ref(x.battle_or_session_ref,y.battle_or_session_ref)&&same_source_ref(x.result_ref,y.result_ref)&&x.usage_ordinal==y.usage_ordinal;else if constexpr(std::is_same_v<T,GatherToolWear>)return same_source_ref(x.gather_receipt_ref,y.gather_receipt_ref)&&x.tool_usage_ordinal==y.tool_usage_ordinal;else if constexpr(std::is_same_v<T,InfusionFailureWear>)return same_source_ref(x.infusion_result_ref,y.infusion_result_ref)&&x.item_slot_ordinal==y.item_slot_ordinal;else return x.facility_attack_result_id==y.facility_attack_result_id&&x.episode_id==y.episode_id&&x.period_ordinal==y.period_ordinal&&x.tool_usage_ordinal==y.tool_usage_ordinal;},a);}
Status validate_item_use_source(const ItemUseSource&s){try{if(s.valueless_by_exception())fail("InvalidItemUseSource");std::visit([](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,BattleWear>){if(!valid_source_ref(v.battle_or_session_ref)||!valid_source_ref(v.result_ref)||v.usage_ordinal<0)fail("InvalidBattleWear");}else if constexpr(std::is_same_v<T,GatherToolWear>){if(!valid_source_ref(v.gather_receipt_ref)||v.tool_usage_ordinal<0)fail("InvalidGatherToolWear");}else if constexpr(std::is_same_v<T,InfusionFailureWear>){if(!valid_source_ref(v.infusion_result_ref)||v.item_slot_ordinal<0)fail("InvalidInfusionFailureWear");}else if(v.facility_attack_result_id.empty()||v.episode_id.empty()||v.period_ordinal<0||v.tool_usage_ordinal<0)fail("InvalidFacilityAttackToolWear");},s);return common_success();}catch(const Failure&e){return {false,{},e.error};}}
Status validate_item_condition_result_shape(const ItemConditionResultView&r){try{
 if(r.result_id.empty()||r.instance_id.empty()||!valid_source_ref(r.source_fact_ref)||!valid_source_ref(r.source_activity_ref)||r.effective_tick<0||r.instance_version_before<1||r.instance_version_after<1||r.max_durability_before<=0||r.max_durability_after<=0||r.durability_before<0||r.durability_after<0||r.durability_before>r.max_durability_before||r.durability_after>r.max_durability_after||(r.reason&&r.reason->empty()))fail("InvalidItemConditionResult",r.result_id);
 if(r.source_kind==ItemConditionSourceKind::use_damage){if(!r.use_source||!r.usage_ordinal||*r.usage_ordinal<0)fail("ItemUseSourceRequired",r.result_id);take(validate_item_use_source(*r.use_source));if(*r.usage_ordinal!=ordinal(*r.use_source)||r.durability_after>r.durability_before||r.max_durability_after!=r.max_durability_before)fail("InvalidItemUseConditionResult",r.result_id);}else if(r.source_kind!=ItemConditionSourceKind::repair_completed||r.use_source||r.usage_ordinal)fail("InvalidItemConditionSourceKind",r.result_id);
 const bool changed=r.durability_before!=r.durability_after||r.max_durability_before!=r.max_durability_after;if(r.disposition==ItemConditionDisposition::applied){if(!changed||r.instance_version_before==std::numeric_limits<Integer>::max()||r.instance_version_after!=r.instance_version_before+1)fail("InvalidItemConditionVersion",r.result_id);}else if(r.disposition!=ItemConditionDisposition::no_change||changed||r.instance_version_after!=r.instance_version_before)fail("InvalidItemConditionVersion",r.result_id);refs(r.resource_effect_refs);return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Outcome<const ItemConditionResultView*> apply_item_use_damage_in_candidate(CommonStore&c,ItemInstancesState&state,const ItemConditionReadContext&x,const ItemUseRequest&r,ReadStampAuthority&authority,const DomainReadVersionSource&versions){try{
 if(&c!=&x.common||&state!=&x.state||(x.selected_version!=2&&x.selected_version!=3))fail("ItemConditionCandidateMismatch",r.instance_id);auto actual=original_use(x,r.source,r.instance_id);
 for(const auto&old:state.condition_results)if(old.instance_id==r.instance_id&&old.source_kind==ItemConditionSourceKind::use_damage&&old.use_source&&same_key(*old.use_source,r.source)){if(!same_item_use_source(*old.use_source,r.source)||old.instance_version_before!=r.expected_instance_version)fail("ConflictingItemUseReplay",r.instance_id);const auto*i=state.instances.find(r.instance_id);if(!i)fail("ItemInstanceMissing",r.instance_id);validate_instance(x,*i);return {true,&old,{}};}
 auto&i=current_instance(c,state,x,r.instance_id,r.expected_instance_version);if(actual.version_before!=i.instance_version||compare(actual.occurred_at,take(x.clock.read_current_moment())))fail("ItemUseActualBoundaryMismatch",i.id);stamp(x,r.source_stamp,authority,versions,i.id,i.instance_version,actual.source_fact_ref);
 ReadStampWriteBoundary write(authority);const auto id=take(c.next_id("item_condition"));const auto before=current_cursor(i);auto result=materialize_result(expected_use(actual,before,id));update(i,result);state.condition_results.push_back(std::move(result));return {true,&state.condition_results.back(),{}};
 }catch(const Failure&e){return {false,nullptr,e.error};}}
Outcome<const ItemConditionResultView*> apply_item_repair_completed_in_candidate(CommonStore&c,ItemInstancesState&state,const ItemConditionReadContext&x,const ItemRepairCompletionRequest&r,ReadStampAuthority&authority,const DomainReadVersionSource&versions){try{
 if(&c!=&x.common||&state!=&x.state||(x.selected_version!=2&&x.selected_version!=3))fail("ItemConditionCandidateMismatch",r.instance_id);auto actual=original_repair(x,r.actual_repair_result,r.instance_id);
 for(const auto&old:state.condition_results)if(old.instance_id==r.instance_id&&old.source_kind==ItemConditionSourceKind::repair_completed&&same_source_ref(old.source_fact_ref,r.actual_repair_result)){if(old.instance_version_before!=r.expected_instance_version)fail("ConflictingItemRepairReplay",r.instance_id);const auto*i=state.instances.find(r.instance_id);if(!i)fail("ItemInstanceMissing",r.instance_id);validate_instance(x,*i);return {true,&old,{}};}
 auto&i=current_instance(c,state,x,r.instance_id,r.expected_instance_version);if(actual.version_before!=i.instance_version||compare(actual.occurred_at,take(x.clock.read_current_moment())))fail("ItemRepairActualBoundaryMismatch",i.id);stamp(x,r.source_stamp,authority,versions,i.id,i.instance_version,actual.source_fact_ref);
 ReadStampWriteBoundary write(authority);const auto id=take(c.next_id("item_condition"));const auto before=current_cursor(i);auto result=materialize_result(expected_repair(actual,before,id));update(i,result);state.condition_results.push_back(std::move(result));return {true,&state.condition_results.back(),{}};
 }catch(const Failure&e){return {false,nullptr,e.error};}}
Status validate_item_condition_history(const ItemConditionReadContext&x){try{
 if((x.selected_version!=2&&x.selected_version!=3))fail("ItemConditionVersionRequired");
 for(std::size_t i=0;i<x.state.condition_results.size();++i){const auto&a=x.state.condition_results[i];take(validate_item_condition_result_shape(a));if(!x.state.instances.find(a.instance_id))fail("ItemConditionInstanceMissing",a.result_id);for(std::size_t j=0;j<i;++j){const auto&b=x.state.condition_results[j];if(a.result_id==b.result_id)fail("DuplicateItemConditionResult",a.result_id);if(a.instance_id!=b.instance_id||a.source_kind!=b.source_kind)continue;if((a.use_source&&b.use_source&&same_key(*a.use_source,*b.use_source))||(!a.use_source&&!b.use_source&&same_source_ref(a.source_fact_ref,b.source_fact_ref)))fail("DuplicateItemConditionUse",a.result_id);}}
 for(const auto&row:x.state.instances.rows())validate_instance(x,row.second);
 auto expected=take(x.original.enumerate_item_condition_occurrences());if(expected.size()!=x.state.condition_results.size())fail("ItemConditionOriginalCoverageMismatch");
 for(std::size_t i=0;i<expected.size();++i){const auto&occurrence=expected[i];std::size_t matched=0;for(const auto&result:x.state.condition_results){const bool match=std::visit([&](const auto&entry){using T=std::decay_t<decltype(entry)>;if(entry.instance_id!=result.instance_id)return false;if constexpr(std::is_same_v<T,ItemUseOccurrence>)return result.source_kind==ItemConditionSourceKind::use_damage&&result.use_source&&same_item_use_source(entry.source,*result.use_source);else return result.source_kind==ItemConditionSourceKind::repair_completed&&same_source_ref(entry.actual_repair_result,result.source_fact_ref);},occurrence);if(match)++matched;}if(matched!=1)fail("ItemConditionOriginalCoverageMismatch");
  for(std::size_t j=0;j<i;++j){if(expected[j].index()!=occurrence.index())continue;const bool duplicate=std::visit([&](const auto&entry){using T=std::decay_t<decltype(entry)>;const auto&prior=std::get<T>(expected[j]);if(entry.instance_id!=prior.instance_id)return false;if constexpr(std::is_same_v<T,ItemUseOccurrence>)return same_key(entry.source,prior.source);else return same_source_ref(entry.actual_repair_result,prior.actual_repair_result);},occurrence);if(duplicate)fail("DuplicateOriginalItemConditionOccurrence");}
 }
 return common_success();
 }catch(const Failure&e){return {false,{},e.error};}}
Outcome<const ItemConditionResultView*> read_item_condition_result(const ItemConditionReadContext&x,const StableId&id){try{if((x.selected_version!=2&&x.selected_version!=3))fail("ItemConditionVersionRequired");const ItemConditionResultView*out=nullptr;for(const auto&r:x.state.condition_results)if(r.result_id==id){if(out)fail("DuplicateItemConditionResult",id);out=&r;}if(!out)fail("ItemConditionResultMissing",id);const auto*i=x.state.instances.find(out->instance_id);if(!i)fail("ItemConditionInstanceMissing",id);validate_instance(x,*i);return {true,out,{}};}catch(const Failure&e){return {false,nullptr,e.error};}}
Outcome<WorldMoment> read_item_condition_result_moment(const ItemConditionReadContext&x,const StableId&id){try{const auto*r=take(read_item_condition_result(x,id));if(r->use_source)return {true,original_use(x,*r->use_source,r->instance_id).occurred_at,{}};return {true,original_repair(x,r->source_fact_ref,r->instance_id).occurred_at,{}};}catch(const Failure&e){return {false,{},e.error};}}
Outcome<std::uint64_t> read_item_instance_domain_version(const ItemInstancesState&state,const DomainReadKey&key){if(key.domain!="craft"||key.kind!="item_instance")return {false,0,common_failure("UnsupportedItemInstanceDomainKey",key.id).error};const auto*i=state.instances.find(key.id);if(!i||i->id!=key.id||i->instance_version<1)return {false,0,common_failure("ItemInstanceSourceRequired",key.id).error};return {true,static_cast<std::uint64_t>(i->instance_version),{}};}

SourceRef item_condition_result_source(const StableId&id){return {"craft","item_condition_result",id};}
Outcome<const ItemConditionResultView*> read_item_condition_source(const ItemConditionReadContext&x,const SourceRef&source,Integer schema){if(schema!=1||source.domain!="craft"||source.kind!="item_condition_result"||source.id.empty())return {false,nullptr,common_failure("UnsupportedItemConditionSource",source.id).error};return read_item_condition_result(x,source.id);}

Status visit_item_condition_source_pins(const ItemConditionReadContext&x,const SourceRef&source,Integer schema,ItemConditionSourcePinSink&sink){try{
 const auto&row=*take(read_item_condition_source(x,source,schema));
 take(sink.item_instance("instance_id",row.instance_id));
 // Their schema belongs to the actual original owner. In particular external
 // resource effects are not automatically Common/schema1.
 take(sink.source("source_fact_ref",0,row.source_fact_ref,{}));
 take(sink.source("source_activity_ref",0,row.source_activity_ref,{}));
 for(std::size_t n=0;n<row.resource_effect_refs.size();++n)take(sink.source("resource_effect_refs",n,row.resource_effect_refs[n],{}));
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}
Status visit_item_instance_condition_pins(const ItemConditionReadContext&x,const StableId&id,ItemConditionSourcePinSink&sink){try{
 if(x.selected_version!=2&&x.selected_version!=3)fail("ItemConditionVersionRequired",id);
 const auto*item=x.state.instances.find(id);if(!item||item->id!=id)fail("ItemInstanceMissing",id);
 validate_instance(x,*item);
 const auto creation=creation_source(x,*item);
 take(sink.source("creation_fact_id",0,creation.first->key.source_ref,creation.first->key.source_schema));
 std::size_t ordinal=0;
 for(const auto&row:x.state.condition_results)if(row.instance_id==id)take(sink.condition_result("condition_results",ordinal++,row.result_id));
 for(const auto&row:x.state.terminations)if(row.instance_id==id)take(sink.item_termination("termination",row.id));
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}
Status visit_item_termination_source_pins(const ItemConditionReadContext&x,const StableId&id,ItemConditionSourcePinSink&sink){try{
 if(x.selected_version!=2&&x.selected_version!=3)fail("ItemConditionVersionRequired",id);
 const auto*row=take(read_item_original_termination(x.common,x.state,x.outputs,id,&x.clock));
 const auto*item=x.state.instances.find(row->instance_id);if(!item||item->id!=row->instance_id)fail("ItemInstanceMissing",row->instance_id);
 validate_instance(x,*item);
 take(sink.item_instance("instance_id",row->instance_id));
 take(sink.source("cause_ref",0,row->cause_ref,row->cause_schema));
 return common_success();
}catch(const Failure&e){return {false,{},e.error};}}

namespace {
CheckpointCopyLease reserve_head(const ItemConditionHeadBuildView&v){return CheckpointCopyLease::reserve();}
}
static_assert(std::is_nothrow_move_constructible_v<ItemConditionHead>);
static_assert(std::is_nothrow_swappable_v<ItemConditionHead>);
static_assert(std::is_nothrow_move_constructible_v<CheckpointCopyLease>);
static_assert(std::is_nothrow_swappable_v<CheckpointCopyLease>);
OwnedItemConditionHead::OwnedItemConditionHead(const ItemConditionHeadBuildView&v):memory_(reserve_head(v)),value_{StableId(v.instance_id),v.instance_version,v.last_condition_result_ref?std::optional<StableId>{std::in_place,*v.last_condition_result_ref}:std::nullopt}{}
OwnedItemConditionHead::OwnedItemConditionHead(OwnedItemConditionHead&&v)noexcept:memory_(std::move(v.memory_)),value_(std::move(v.value_)){}
OwnedItemConditionHead&OwnedItemConditionHead::operator=(OwnedItemConditionHead&&v)noexcept{if(this!=&v){OwnedItemConditionHead fresh(std::move(v));swap(fresh);}return *this;}
void OwnedItemConditionHead::swap(OwnedItemConditionHead&v)noexcept{if(this==&v)return;using std::swap;swap(value_,v.value_);swap(memory_,v.memory_);}
Outcome<OwnedItemConditionHead> read_current_item_condition_head(const ItemConditionReadContext&x,const StableId&id){try{
 if(x.selected_version!=2&&x.selected_version!=3)fail("ItemConditionVersionRequired",id);const auto*item=x.state.instances.find(id);if(!item||item->id!=id||item->state!=ItemInstanceState::live)fail("LiveItemInstanceRequired",id);validate_instance(x,*item);
 std::optional<std::string_view> last;for(const auto&r:x.state.condition_results)if(r.instance_id==id)last=r.result_id;return {true,OwnedItemConditionHead({item->id,item->instance_version,last}),{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<ItemConditionAtHead> read_item_condition_at_head(const ItemConditionReadContext&x,const ItemConditionHead&head){try{
 if((x.selected_version!=2&&x.selected_version!=3)||head.instance_id.empty()||head.instance_version<1)fail("ItemConditionHeadRequired",head.instance_id);const auto*item=x.state.instances.find(head.instance_id);if(!item||item->id!=head.instance_id)fail("ItemInstanceMissing",head.instance_id);const auto creation=creation_source(x,*item);
 ConditionCursor before{*item,1,creation.second.max_durability,creation.second.max_durability};bool found=!head.last_condition_result_ref;std::optional<WorldMoment> previous;
 if(head.last_condition_result_ref){for(std::size_t i=0;i<x.state.condition_results.size();++i){const auto&r=x.state.condition_results[i];if(r.instance_id!=head.instance_id)continue;for(std::size_t j=0;j<i;++j){const auto&prior=x.state.condition_results[j];if(prior.result_id==r.result_id)fail("DuplicateItemConditionResult",r.result_id);if(prior.instance_id==r.instance_id&&prior.source_kind==r.source_kind&&((prior.use_source&&r.use_source&&same_key(*prior.use_source,*r.use_source))||(!prior.use_source&&!r.use_source&&same_source_ref(prior.source_fact_ref,r.source_fact_ref))))fail("DuplicateItemConditionUse",r.result_id);}
  const auto at=original_result(x,r,before);if(at.half_day_tick<creation.second.produced_tick||(previous&&compare(at,*previous)<0))fail("ItemConditionSourceOrderMismatch",r.result_id);previous=at;update_values(before,r);if(r.result_id==*head.last_condition_result_ref){found=true;break;}
 }}
 if(!found||before.instance_version!=head.instance_version)fail("ItemConditionHeadPrefixMismatch",head.instance_id);return {true,{item,creation.first,before.instance_version,before.durability,before.max_durability},{}};
}catch(const Failure&e){return {false,{},e.error};}}

Outcome<ItemConditionAtHead> read_item_condition_before_common_source(const ItemConditionReadContext&x,const ItemConditionHead&head,const SourceRef&boundary,const WorldMoment&at){try{
 auto order=[&](const SourceRef&r){const auto*f=x.common.state().facts.find(r.id);if(!f||f->id!=r.id||f->domain!=r.domain||f->kind!=r.kind)fail("ItemConditionActualBoundaryRequired",r.id);const auto&rows=x.common.state().facts.order();auto found=std::find(rows.begin(),rows.end(),r.id);if(found==rows.end())fail("ItemConditionActualBoundaryRequired",r.id);return static_cast<std::size_t>(found-rows.begin());};
 time(x,at);const auto cut=order(boundary);
 if((x.selected_version!=2&&x.selected_version!=3)||head.instance_id.empty()||head.instance_version<1)fail("ItemConditionHeadRequired",head.instance_id);
 // Establish the exact original prefix before resolving it. A forged adopted
 // head must never send this historical read through a later repair result
 // back into its own acceptance. Later effects remain the full owner
 // validator's responsibility.
 const auto*item=x.state.instances.find(head.instance_id);if(!item||item->id!=head.instance_id)fail("ItemInstanceMissing",head.instance_id);
 const ItemOutputAdoption*creation=nullptr;for(const auto&row:x.state.output_adoptions)if(row.instance_id&&*row.instance_id==head.instance_id){if(creation)fail("DuplicateItemCreationSource",head.instance_id);creation=&row;}
 if(!creation||order(creation->key.source_ref)>=cut)fail("ItemCreationBeforeAdoptionRequired",head.instance_id);
 const auto created=order(creation->key.source_ref);
 Integer expected_version=1;const StableId*expected_last=nullptr;bool later=false;
 for(const auto&r:x.state.condition_results){if(r.instance_id!=head.instance_id)continue;const bool before=order(r.source_fact_ref)<cut;if(!before){later=true;continue;}if(later||order(r.source_fact_ref)<=created)fail("ItemConditionAdoptionPrefixOrderMismatch",r.result_id);take(validate_item_condition_result_shape(r));
  expected_version=r.instance_version_after;expected_last=&r.result_id;
 }
 if(expected_version!=head.instance_version||bool(expected_last)!=head.last_condition_result_ref.has_value()||(expected_last&&*expected_last!=*head.last_condition_result_ref))fail("ItemConditionAdoptionHeadMismatch",head.instance_id);
 auto value=take(read_item_condition_at_head(x,head));
 for(const auto&r:x.state.condition_results){if(r.instance_id!=head.instance_id||order(r.source_fact_ref)>=cut)continue;
  const auto occurred=r.source_kind==ItemConditionSourceKind::use_damage?original_use(x,*r.use_source,r.instance_id).occurred_at:original_repair(x,r.source_fact_ref,r.instance_id).occurred_at;
  if(compare(occurred,at)>0)fail("ItemConditionSourceAfterAdoption",r.result_id);
 }
 for(const auto&terminal:x.state.terminations)if(terminal.instance_id==head.instance_id&&order(terminal.cause_ref)<cut)fail("ItemTerminatedBeforeAdoption",head.instance_id);
 return {true,std::move(value),{}};
}catch(const Failure&e){return {false,{},e.error};}}

}
