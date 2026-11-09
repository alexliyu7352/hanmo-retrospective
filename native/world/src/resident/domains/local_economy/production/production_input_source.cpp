#include "production_input_source.hpp"
#include "production_population_source.hpp"
#include "resident/domains/tasks/work_execution/work_economic_window_source.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{v.error};}
int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool refs(const std::vector<SourceRef>&a,const std::vector<SourceRef>&b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_source_ref(a[i],b[i]))return false;return true;}
const EconomicProductionAdoptionContext& original(const NativeProductionInputContext&x){
 const auto&o=x.capacity.original;const auto&p=x.labor.labor.original;
 if(&o.common!=&p.common||&o.economy!=&p.economy||&o.catalog!=&x.labor.labor.catalog||&o.sources!=&x.labor.supply.calendar.sources||&o.clock!=&p.clock)fail("ProductionInputSnapshotMismatch");
 return o;
}
const AccumulatedProductionState2& cycle(const NativeProductionInputContext&x,const StableId&unit,const StableId&id){
 const auto&o=original(x);const auto*c=o.sources.cycles.find(id);const auto*p=o.work_catalog.production_profiles.find(unit);
 if(!c||c->cycle_source_ref!=id||c->unit_id!=unit||!p||p->unit_id!=unit||p->labor_denominator<=0||c->labor_denominator!=p->labor_denominator)fail("ProductionInputOriginalCycleRequired",id);
 take(x.capacity.adoptions.read_production_adoption(unit,{{"local_economy","production_cycle_started",id},2}));return *c;
}
MomentOwnerSource adoption(const ProductionSegmentSource&s){
 if(s.asset_use_source_refs.size()<2||!same_source_ref(s.asset_use_source_refs.front(),{"local_economy_catalog","production_capacity_plan",s.unit_id}))fail("ProductionInputAdoptionSourceRequired",s.unit_id);
 const auto&r=s.asset_use_source_refs[1];
 if(r.domain=="local_economy"&&(r.kind=="production_cycle_started"||r.kind=="production_input_segment"))return {r,2};
 if(r.domain=="local_economy"&&(r.kind=="facility_condition_change"||r.kind=="local_ration"||r.kind=="economic_period_opened"))return {r,1};
 if(r.domain=="politics"&&(r.kind==political_takeover_pending_fact_kind||r.kind==political_takeover_fact_kind))return {r,2};
 if(is_original_production_population_change({r,1}))return {r,1};
 fail("ProductionInputAdoptionSourceKindRequired",r.id);
}
struct Reconstructed {ProductionInputRead input;OriginalProductionCapacityInput capacity;OriginalProductionLaborInput labor;};
Reconstructed reconstruct(const NativeProductionInputContext&x,const StableId&unit,const StableId&id,const WorldMoment&from,const WorldMoment&to,const MomentOwnerSource&cut){
 const auto&o=original(x);const auto&c=cycle(x,unit,id);const auto now=take(o.clock.read_current_moment());
 if(compare(from,to)>0||compare(from,{c.start_tick,{}})<0||compare(to,{c.due_tick,{}})>0||compare(from,now)>0)fail("ProductionInputActualWindowMismatch",id);
 Reconstructed out;out.capacity=take(read_original_production_capacity_input(x.capacity,unit,cut));
 if(compare(out.capacity.adopted_at,from)>0||compare(out.capacity.adopted_at,{c.start_tick,{}})<0)fail("ProductionInputAdoptionOutsideCycle",id);
 out.labor=take(read_original_production_labor_input(x.labor,unit,out.capacity.adopted_at,cut.source_ref,c.labor_denominator));
 auto&s=out.input.source;s.unit_id=unit;s.cycle_source_ref=id;s.from=from;s.to=to;
 s.condition_source_refs=out.capacity.condition_source_refs;s.asset_use_source_refs=out.capacity.asset_use_source_refs;
 s.participation_refs=out.labor.participation_refs;s.actual_supply_refs=out.labor.actual_supply_refs;
 s.batch_capacity=out.capacity.batch_capacity;s.labor_rate_numerator=out.labor.labor_rate_numerator;s.labor_rate_denominator=out.labor.labor_rate_denominator;
 out.input.actual_labor_allocation_refs=out.labor.actual_labor_allocation_refs;out.input.actual_anchor=out.labor.actual_anchor;
 return out;
}
void same_source(const ProductionSegmentSource&a,const ProductionSegmentSource&b){
 if(a.unit_id!=b.unit_id||a.cycle_source_ref!=b.cycle_source_ref||compare(a.from,b.from)||compare(a.to,b.to)||a.batch_capacity!=b.batch_capacity||a.labor_rate_numerator!=b.labor_rate_numerator||a.labor_rate_denominator!=b.labor_rate_denominator||!refs(a.condition_source_refs,b.condition_source_refs)||!refs(a.asset_use_source_refs,b.asset_use_source_refs)||!refs(a.participation_refs,b.participation_refs)||!refs(a.actual_supply_refs,b.actual_supply_refs))fail("ProductionFrozenInputMismatch",a.unit_id);
}
ProductionInputRead validated(const NativeProductionInputContext&x,const ProductionSegmentSource&s){
 auto actual=reconstruct(x,s.unit_id,s.cycle_source_ref,s.from,s.to,adoption(s));same_source(actual.input.source,s);
 const auto now=take(original(x).clock.read_current_moment());const auto through=compare(s.to,now)>0?now:s.to;
 if(compare(s.from,through)<0){const MomentSpan elapsed{s.from,through};take(validate_original_production_capacity_window(x.capacity,actual.capacity,elapsed));take(validate_original_production_labor_window(x.labor,s.unit_id,actual.capacity.adopted_at,actual.capacity.adoption_source.source_ref,s.labor_rate_denominator,actual.labor,elapsed));}
 return std::move(actual.input);
}
void close_source(const NativeProductionInputContext&x,const StableId&unit,const WorldMoment&at,const SourceRef&source){
 const auto&o=original(x);if(compare(at,take(o.clock.read_current_moment()))>0)fail("ProductionClosingSourceFromFuture",source.id);
 if(source.domain=="local_economy"&&source.kind=="production_cycle_due"){
  const auto&c=cycle(x,unit,source.id);if(compare(at,{c.due_tick,{}}))fail("ProductionDueBoundaryMismatch",source.id);return;
 }
 if(source.domain=="tasks"&&source.kind=="work_accepted"){
  if(!x.tasks)fail("ProductionSupervisionOriginalSourceRequired",source.id);bool found=false;
  for(const auto&row:o.sources.segments.rows()){const auto&s=row.second;if(s.source.unit_id!=unit||compare(s.source.to,at)||!same_source_ref(s.close_cause,source))continue;
   const auto consumed=take(read_work_supervised_window_for_economic_source(o.common,*x.tasks,source,{{"local_economy","production_input_segment",s.id},2},at));const auto*plan=consumed.episode?std::get_if<SupervisedWorkPlan>(&consumed.episode->frozen_plan):nullptr;const auto*profile=o.work_catalog.production_profiles.find(unit);if(!plan||!profile||plan->economic_work_ref.owner_domain!="local_economy"||plan->economic_work_ref.work_id!=unit||plan->economic_work_ref.kind!=profile->work_kind)fail("ProductionSupervisionOriginalUnitMismatch",source.id);found=true;
  }
  if(!found)fail("ProductionSupervisionActualWindowRequired",source.id);return;
 }
 MomentOwnerSource actual{source,0};
 if((source.domain=="local_economy"&&(source.kind=="facility_condition_change"||source.kind=="local_ration"||source.kind=="economic_period_opened"))||is_original_production_population_change({source,1}))actual.source_schema=1;
 else if(source.domain=="politics"&&(source.kind==political_takeover_pending_fact_kind||source.kind==political_takeover_fact_kind))actual.source_schema=2;
 else fail("ProductionClosingOriginalSourceKindRequired",source.id);
 const auto input=take(read_original_production_capacity_input(x.capacity,unit,actual));if(compare(input.adopted_at,at))fail("ProductionInputChangeActualMomentMismatch",source.id);
}
}
Outcome<ProductionInputRead> NativeProductionInputSourceRead::read_production_segment_inputs(const StableId&unit,const StableId&id,const WorldMoment&from,const WorldMoment&to)const{try{
 const auto&c=cycle(x_,unit,id);const auto source=c.active_input.unit_id.empty()?MomentOwnerSource{{"local_economy","production_cycle_started",id},2}:adoption(c.active_input);
 return {true,std::move(reconstruct(x_,unit,id,from,to,source).input),{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<ProductionInputRead> NativeProductionInputSourceRead::read_production_inputs_after_change(const StableId&unit,const StableId&id,const WorldMoment&from,const WorldMoment&to,const MomentOwnerSource&change)const{try{
 auto actual=reconstruct(x_,unit,id,from,to,change);if(compare(actual.capacity.adopted_at,from))fail("ProductionChangeAdoptionMomentMismatch",change.source_ref.id);
 if(change.source_ref.domain=="local_economy"&&change.source_ref.kind=="production_input_segment"){
  const auto*s=original(x_).sources.segments.find(change.source_ref.id);if(!s||s->source.cycle_source_ref!=id)fail("ProductionChangeOriginalSegmentRequired",change.source_ref.id);close_source(x_,unit,from,s->close_cause);
 }else close_source(x_,unit,from,change.source_ref);
 return {true,std::move(actual.input),{}};
}catch(const Failure&e){return {false,{},e.error};}}
Outcome<ProductionInputRead> NativeProductionInputSourceRead::read_validated_production_segment_source(const ProductionSegmentSource&s)const{try{return {true,validated(x_,s),{}};}catch(const Failure&e){return {false,{},e.error};}}
Status NativeProductionInputSourceRead::validate_production_segment_source(const ProductionSegmentSource&s)const{auto read=read_validated_production_segment_source(s);return read.ok?common_success():Status{false,{},read.error};}
Outcome<std::optional<WorldMoment>> NativeProductionInputSourceRead::read_next_production_rule_boundary(const StableId&unit,const StableId&id)const{try{
 const auto&c=cycle(x_,unit,id);if(c.settlement_receipt_ref)return {true,{}, {}};
 const auto actual=reconstruct(x_,unit,id,c.active_input.from,c.active_input.to,adoption(c.active_input));same_source(actual.input.source,c.active_input);
 const auto&boundary=actual.labor.next_rule_boundary;if(boundary&&compare(*boundary,{c.due_tick,{}})<=0)return {true,boundary,{}};return {true,{}, {}};
}catch(const Failure&e){return {false,{},e.error};}}
Status NativeProductionInputSourceRead::validate_production_input_change(const StableId&unit,const WorldMoment&at,const SourceRef&source)const{try{close_source(x_,unit,at,source);return common_success();}catch(const Failure&e){return {false,{},e.error};}}
}
