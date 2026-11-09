#include "safe_exit_named_entry.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
// 未落实的专业分区不是坏数量，也不能隐式退化成一条具名名单。
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::rule){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
std::size_t position(const CommonStore& common,const SourceRef& ref){
 const auto* head=common.state().facts.find(ref.id);const auto& order=common.state().facts.order();
 const auto found=std::find(order.begin(),order.end(),ref.id);
 if(!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind||found==order.end())fail("SafeExitNamedActualSourceRequired",ref.id);
 return static_cast<std::size_t>(found-order.begin());
}
SourceRef entry_source(const BoundaryCrossingReceipt& crossing){return {"travel","boundary_crossing",crossing.receipt_id};}
// 全部空间/组成/原库存证据仍由S1唯一历史reader验证；这里不能重建今日manifest。
void original(const SafeExitNamedEntryContext& x,const BoundaryCrossingReceipt& crossing,
 const TransportEntryManifestSource& supplied){
 if(x.carriers.crossings.find(crossing.receipt_id)!=&crossing)fail("SafeExitNamedBorrowedCrossingRequired",crossing.receipt_id);
 take(validate_group_boundary_crossing_source(x.common,x.executions,x.carriers,x.movement,x.travel_catalog,crossing,x.arithmetic));
 const auto* manifest=take(read_historical_transport_entry_manifest(x.common,x.carriers,x.movement,
  x.executions,x.manifest_history,crossing.execution,crossing.entry_ordinal,crossing.boundary,
  crossing.manifest_revision,crossing.composition_revision,x.arithmetic));
 if(manifest!=&supplied||take(compare_world_moments(manifest->occurred_at,crossing.entry_at,x.arithmetic))||
    position(x.common,{"travel","transport_entry_manifest",manifest->id})>=position(x.common,entry_source(crossing)))
  fail("SafeExitNamedOriginalManifestMismatch",crossing.receipt_id);
 const auto now=take(x.clock.read_current_moment());
 if(take(compare_world_moments(crossing.entry_at,now,x.arithmetic))>=0)fail("SafeExitNamedPositiveSpanRequired",crossing.receipt_id);
 const auto& m=manifest->manifest;
 if(std::holds_alternative<ArmyCarrierRef>(m.carrier)||std::holds_alternative<FleetCarrierRef>(m.carrier)||
    !m.military_force_refs.empty()||!m.vessel_placements.empty())
  fail("SafeExitMilitaryEntryPartitionRequired",manifest->id,ErrorKind::unsupported_capability);
 if(!m.civilian_population_refs.empty()||!manifest->population_sources.empty())
  fail("SafeExitPopulationEntryPartitionRequired",manifest->id,ErrorKind::unsupported_capability);
 if(!m.vehicle_refs.empty())fail("SafeExitVehicleEntryPartitionRequired",manifest->id,ErrorKind::unsupported_capability);
 // 每个原个人库存即使空也保entry证据。零重量可能仍有货，不能作为无货证明。
 for(const auto& inventory:manifest->inventory_sources)if(!inventory.entry_evidence.lots.empty())
  fail("SafeExitCargoEntryPartitionRequired",inventory.entry_evidence.inventory_id,ErrorKind::unsupported_capability);
 if(m.named_actor_refs.empty())fail("SafeExitNamedEntryScopeRequired",manifest->id);
 for(std::size_t i=0;i<m.named_actor_refs.size();++i){const auto& actor=m.named_actor_refs[i];
  if(actor.empty()||std::find(m.named_actor_refs.begin(),m.named_actor_refs.begin()+i,actor)!=m.named_actor_refs.begin()+i)
   fail("SafeExitNamedEntryIdentityMismatch",actor);
  auto actual=take(read_character_presence_before_fact_v2(x.common,x.foundation,x.foundation_catalog,
   x.deaths,x.life,actor,entry_source(crossing),crossing.entry_at,x.clock,x.family_catalog,x.shared,x.born_sources));
  if(!actual.present||!actual.alive)fail("SafeExitNamedLivingEntryRequired",actor);
 }
}
}
Status validate_original_named_safe_exit_entry_sources(const SafeExitNamedEntryContext& x,
 const BoundaryCrossingReceipt& crossing,const TransportEntryManifestSource& manifest){try{
 original(x,crossing,manifest);return common_success();
 }catch(const Failure& failure){return {false,{},failure.error};}}
Outcome<SafeExitEntryPartition> read_original_named_safe_exit_partition(const SafeExitNamedEntryContext& x,
 const BoundaryCrossingReceipt& crossing,const TransportEntryManifestSource& manifest){try{
 original(x,crossing,manifest);SafeExitEntryPartition result;
 for(const auto& actor:manifest.manifest.named_actor_refs)result.entered_subjects.push_back({SubjectKind::actor,actor});
 SafeExitInitialLine line;line.entitlement_source_ref=entry_source(crossing);
 line.subject_scope=SafeExitNamedSubjects{manifest.manifest.named_actor_refs};
 result.lines.push_back(std::move(line));return {true,std::move(result),{}};
 }catch(const Failure& failure){return {false,{},failure.error};}}
Status validate_original_named_safe_exit_partition(const SafeExitNamedEntryContext& x,
 const BoundaryCrossingReceipt& crossing,const TransportEntryManifestSource& manifest,
 const SafeExitEntryPartition& partition){try{
 original(x,crossing,manifest);const auto& actors=manifest.manifest.named_actor_refs;
 if(partition.lines.size()!=1||partition.entered_subjects.size()!=actors.size())fail("SafeExitNamedInitialPartitionMismatch",crossing.receipt_id);
 for(std::size_t i=0;i<actors.size();++i)if(partition.entered_subjects[i].kind!=SubjectKind::actor||partition.entered_subjects[i].id!=actors[i])
  fail("SafeExitNamedOriginalSubjectMismatch",actors[i]);
 const auto& line=partition.lines.front();const auto* scope=std::get_if<SafeExitNamedSubjects>(&line.subject_scope);
 if(!scope||scope->actor_or_member_refs!=actors||!same_source_ref(line.entitlement_source_ref,entry_source(crossing))||!line.cargo_scope_refs.empty())
  fail("SafeExitNamedInitialLineMismatch",crossing.receipt_id);
 return common_success();
 }catch(const Failure& failure){return {false,{},failure.error};}}
Outcome<std::vector<const FamilyDeathRecord*>> read_original_named_safe_exit_deaths(const SafeExitNamedEntryContext& x,
 const BoundaryCrossingReceipt& crossing,const TransportEntryManifestSource& manifest){try{
 original(x,crossing,manifest);std::vector<const FamilyDeathRecord*> result;
 for(const auto& actor:manifest.manifest.named_actor_refs){const auto* death=take(read_character_death_after_fact_v2(
  x.common,x.foundation,x.foundation_catalog,x.deaths,x.life,actor,entry_source(crossing),crossing.entry_at,
  x.clock,x.family_catalog,x.shared,x.born_sources));if(death)result.push_back(death);}
 return {true,std::move(result),{}};
 }catch(const Failure& failure){return {false,{},failure.error};}}
}
