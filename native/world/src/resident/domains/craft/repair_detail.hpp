#pragma once
#include "repair_contract.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
#include "resident/storage/selected_resource_execution.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::repair_detail {
struct Failure{RuleError error;};
[[noreturn]]inline void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){auto e=common_failure(code,id).error;e.kind=kind;throw Failure{std::move(e)};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
inline int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
inline Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("RepairQuantityOverflow");return a+b;}
inline bool owner(const Owner&a,const Owner&b){return !a.extra&&!b.extra&&a.kind==b.kind&&a.id==b.id;}
inline bool same_head(const ItemConditionHead&a,const ItemConditionHead&b){return a.instance_id==b.instance_id&&a.instance_version==b.instance_version&&a.last_condition_result_ref==b.last_condition_result_ref;}
inline bool properties(const std::optional<LotPropertiesRef>&a,const std::optional<LotPropertiesRef>&b){return a.has_value()==b.has_value()&&(!a||a->properties_id==b->properties_id);}
inline WorldMoment now(const RepairReadContext&x){auto at=take(x.clock.read_current_moment());take(compare_world_moments(at,at));if(at.half_day_tick<x.common.tick())fail("RepairClockBeforeCommon");return at;}
inline void context(const RepairReadContext&x){if(&x.conditions.common!=&x.common||&x.conditions.state!=&x.items||&x.conditions.clock!=&x.clock||x.conditions.selected_version!=3||!x.original.bound_to(x.common,x.items))fail("RepairReadContextMismatch");}
inline void writable(CommonStore&c,ItemInstancesState&i,ActionElapsedStore&e,const RepairWriteContext&x){context(x.read);if(!x.original_economy||!x.read.commercial_sources)fail("RepairCommercialWriteProviderRequired",{},ErrorKind::unsupported_capability);if(&x.read.commercial_sources->economy!=x.original_economy)fail("RepairCommercialWriteCandidateMismatch");if(!x.read.occupancy||!x.facility_occupancy_owner)fail("RepairOccupancyProviderRequired",{},ErrorKind::unsupported_capability);if(&c!=&x.read.common||&i!=&x.read.items||&e!=&x.read.elapsed||!x.effects.bound_to(c,i)||compare(take(x.stamps.read_current_moment()),now(x.read)))fail("RepairCandidateContextMismatch");}
inline std::size_t header(const CommonStore&c,const SourceRef&ref){const auto*h=c.state().facts.find(ref.id);if(!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)fail("RepairOriginalHeaderRequired",ref.id);const auto&order=c.state().facts.order();auto it=std::find(order.begin(),order.end(),ref.id);if(it==order.end())fail("RepairOriginalHeaderRequired",ref.id);return static_cast<std::size_t>(it-order.begin());}
inline void refs(const std::vector<MomentOwnerSource>&values,const StableId&id){for(std::size_t i=0;i<values.size();++i){if(values[i].source_schema<1||!valid_source_ref(values[i].source_ref))fail("RepairActualSourceRequired",id);for(std::size_t j=0;j<i;++j)if(values[j].source_schema==values[i].source_schema&&same_source_ref(values[j].source_ref,values[i].source_ref))fail("DuplicateRepairActualSource",id);}}
inline const LotView& lot(const CommonStore&c,const CargoSelection&selection){const auto*inv=c.inventory(selection.inventory_id);const LotView*out=nullptr;if(inv)for(const auto&l:inv->lots)if(l.id==selection.lot_id){if(out)fail("DuplicateRepairLot",selection.lot_id);out=&l;}if(!out||selection.quantity<1||selection.quantity>out->quantity)fail("RepairActualLotRequired",selection.lot_id);return *out;}
inline const ResourceOperationExecutionDraft& apply(CommonStore&c,const ResourceOperation&op,const StableId&inventory){if(!c.resource_execution_capture_identity())fail("RepairResourceCaptureRequired");const auto before=c.resource_execution_drafts().size();ResourceRecipe recipe;recipe.expected_versions.push_back(c.read_version({ResourceKey::Kind::inventory,inventory}));recipe.operations.push_back(op);take(c.apply_resources(recipe));const auto&drafts=c.resource_execution_drafts();if(drafts.size()!=before+1||drafts.back().operations.size()!=1)fail("RepairResourceExecutionRequired");take(validate_selected_resource_execution(op,drafts.back().operations.front()));return drafts.back().operations.front();}
inline const RepairPolicy& policy(const RepairReadContext&x,const RepairTerms&t){return *take(read_repair_policy(x.policies,t.policy_ref));}
void terms(const RepairReadContext&,const RepairTerms&,const RepairJob*);
void materials(const RepairReadContext&,const RepairTerms&,const WorldMoment&);
void material_history(const RepairReadContext&,const RepairJob&,bool complete);
void coverage(const RepairReadContext&,const RepairJob&,const std::vector<FixedServiceInputRef>&,const MomentSpan&);
const RepairJob& by_action(const RepairReadContext&,const StableId&);
std::vector<FixedServiceTransition> transitions(const RepairReadContext&,const RepairJob&);
WorkUnits validate_elapsed_history(const RepairReadContext&,const RepairJob&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&);
}
