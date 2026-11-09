#include "native_growth_market_sources.hpp"
#include <algorithm>
#include "growth_result_projection2.hpp"
#include "world_bundle.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/growth/growth_terminal_publication.hpp"
#include "resident/domains/growth/growth_market_start_read.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){return {false,{},common_failure(code,id,kind).error};}
bool original_header(const FactRecord&h){return (h.domain=="growth"&&h.kind=="life_activity")||(h.domain=="cultivation"&&(h.kind=="training"||h.kind=="skill_study"))||(h.domain=="services"&&h.kind=="long_rest");}
Outcome<const ActionTimeRegistration*> registration(const WorldBundle&w,const CompleteExactTimeRuntime&runtime,const StableId&action){
 const LifeFixedServiceBoundary*initial=nullptr;for(const auto&row:w.growth.fixed_service_boundaries.rows())if(row.second.action_ref==action&&row.second.kind==LifeFixedBoundaryKind::started){if(initial)return fail<const ActionTimeRegistration*>("DuplicateGrowthOriginalStart",action);initial=&row.second;}if(!initial)return fail<const ActionTimeRegistration*>("GrowthOriginalStartRequired",action);
 const ActionTimeRegistration*out=nullptr;for(const auto&r:runtime.action_times)if(r.owner_domain=="growth"&&r.action_kind==initial->action_kind){if(out)return fail<const ActionTimeRegistration*>("DuplicateGrowthTimeRegistration",action);out=&r;}return out?Outcome<const ActionTimeRegistration*>{true,out,{}}:fail<const ActionTimeRegistration*>("GrowthTimeRegistrationRequired",action,ErrorKind::unsupported_capability);
}
template<class T,class F>Outcome<T> read_current(const WorldBundle&w,F fn){if(!w.catalog||!w.catalog->exact_time||!w.catalog->growth||!w.catalog->common||!w.action_elapsed)return fail<T>("GrowthExactResultDependenciesRequired");auto now=w.actual_phase_moment();if(!now.ok)return {false,{},now.error};auto market=read_native_growth_market_exact_sources(w);if(!market.ok)return {false,{},std::move(market.error)};GrowthActionTimeRead reader(w.common,w.growth,*w.catalog->growth,*w.catalog->common,*w.action_elapsed,w.catalog->exact_time->scale_ref,w.catalog->identity,now.value,{},&w.interaction,w.catalog->interaction.get(),market.value?&*market.value:nullptr);return fn(reader);}
// 市场精确开始只引用原schema2持值；返回深ref在第一次复制前预留并随payload活到P10。
Outcome<ExactOwnerFactRead> scalar_start_support(const WorldBundle& w,const StableId& action){
 auto roots=read_native_growth_market_exact_sources(w);if(!roots.ok)return {false,{},std::move(roots.error)};
 if(!roots.value)return fail<ExactOwnerFactRead>("GrowthScalarStartSourceRequired",action);
 auto now=w.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 auto original=read_growth_scalar_market_start_for_action_v2(w.common,*w.catalog->common,*w.catalog->growth,w.growth,roots.value->actor_foundation,roots.value->history_catalog,roots.value->history,action,now.value);
 if(!original.ok)return {false,{},std::move(original.error)};
 if(!original.value.movement_id)return fail<ExactOwnerFactRead>("GrowthScalarStartSourceRequired",action);

 ExactOwnerFactRead out;out.payload_memory=CheckpointCopyLease::reserve();out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.reserve(1);
 out.supporting_sources.push_back({{"growth_market_history","growth_market_movement",checkpoint_fresh_string_copy(*original.value.movement_id)},2});return {true,std::move(out),{}};
}
// 原LE审计头没有精确发生时刻，也不授予动态事实或知识；这里只核真实原账身份。
Outcome<ExactOwnerFactRead> economic_audit_no_projection(
    const WorldBundle& w, const FactRecord& h, const MarketMovementRecord& movement){
 auto roots=read_native_growth_market_exact_sources(w);
 if(!roots.ok)return {false,{},std::move(roots.error)};
 if(!roots.value||movement.kind!=h.kind||movement.business_source_schema!=1||
    !movement.original_business_execution||movement.original_business_execution->schema_version!=1||
    !std::holds_alternative<MarketOriginalTick>(movement.original_business_execution->time_basis)||
    movement.observed_tick<0||movement.observed_tick==INT64_MAX||movement.observed_tick>w.common.tick()||movement.source_tick!=movement.observed_tick+1)
   return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);
 const auto* head=w.common.state().facts.find(movement.business_fact_id);
 if(!head||head->id!=movement.business_fact_id||head->domain!="local_economy")
   return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);
 if(movement.business_kind==MarketCauseKind::local_production){
  const auto* original=w.local_economy.production_receipts.find(movement.business_fact_id);
  if(!original||original->id!=head->id||original->tick!=movement.source_tick||head->kind!="local_production"||w.local_economy.rations.find(head->id))return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);
 }else if(movement.business_kind==MarketCauseKind::local_ration){
  const auto* original=w.local_economy.rations.find(movement.business_fact_id);
  if(!original||original->id!=head->id||original->tick!=movement.source_tick||head->kind!="local_ration"||w.local_economy.production_receipts.find(head->id))return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);
 }else return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);
 const auto& order=w.common.state().facts.order();
 const auto before=std::find(order.begin(),order.end(),head->id);
 const auto after=std::find(order.begin(),order.end(),h.id);
 if(before==order.end()||after==order.end()||before>=after||movement.steps.empty())return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalOrderRequired",h.id);
 for(const auto& step:movement.steps){const auto* binding=std::get_if<MarketOriginalBusinessFifo>(&step.source_binding);if(!binding||binding->business_fact_id!=head->id||binding->execution_stage_ordinal>=movement.original_business_execution->stages.size())return fail<ExactOwnerFactRead>("GrowthEconomicAuditOriginalSourceRequired",h.id);}
 ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;return {true,std::move(out),{}};
}
struct OriginalOutcome {bool success=false;StableId code;};
Outcome<OriginalOutcome> original_outcome(const GrowthState&s,const GrowthTerminalPublication&p){
 switch(p.original_result_ref.kind){
 case LifeFixedOutcomeRef::Kind::training:{const auto*f=s.facts.find(p.action_id);const auto*r=f?std::get_if<TrainingFact>(f):nullptr;if(!r||r->phase!="completed")return fail<OriginalOutcome>("GrowthTrainingTerminalRequired",p.action_id);return {true,{r->success,{}},{}};}
 case LifeFixedOutcomeRef::Kind::study:{const auto*f=s.facts.find(p.action_id);const auto*r=f?std::get_if<StudyFact>(f):nullptr;if(!r||r->phase!="completed")return fail<OriginalOutcome>("GrowthStudyTerminalRequired",p.action_id);return {true,{r->success,r->code},{}};}
 case LifeFixedOutcomeRef::Kind::rest:{const auto*f=s.facts.find(p.action_id);const auto*r=f?std::get_if<RestFact>(f):nullptr;if(!r||r->phase!="completed")return fail<OriginalOutcome>("GrowthRestTerminalRequired",p.action_id);return {true,{r->stop_reason=="duration",r->stop_reason},{}};}
 case LifeFixedOutcomeRef::Kind::life_activity:{const auto*r=s.life_activities.find(p.action_id);if(!r||r->view.phase==LifeActivityPhase::active)return fail<OriginalOutcome>("GrowthLifeTerminalRequired",p.action_id);return {true,{r->view.success,r->view.code},{}};}
 default:return fail<OriginalOutcome>("GrowthTerminalOutcomeReaderRequired",p.action_id,ErrorKind::unsupported_capability);
 }
}
}
Outcome<ExactOwnerFactRead> NativeGrowthResultOwnerRead::read_selected_fact(const FactRecord&h)const{
 const auto*actual=world_.common.state().facts.find(h.id);if(!actual||actual->kind!=h.kind||actual->domain!=h.domain)return fail<ExactOwnerFactRead>("GrowthResultHeaderMismatch",h.id);
 if(h.domain=="growth_market_history"&&h.kind=="growth_market_movement"){
   const auto* movement=world_.growth_market_history.movements.find(h.id);
   if(!movement)return fail<ExactOwnerFactRead>("GrowthMarketOriginalMovementMismatch",h.id);
   if(movement->business_kind!=MarketCauseKind::growth_trade)return economic_audit_no_projection(world_,h,*movement);
   auto support=scalar_start_support(world_,movement->business_fact_id);
   if(support.ok&&(support.value.supporting_sources.size()!=1||support.value.supporting_sources[0].source_ref.id!=h.id))return fail<ExactOwnerFactRead>("GrowthMarketOriginalMovementMismatch",h.id);
   return support;
 }
 if(h.domain=="growth"&&h.kind=="life_activity")if(const auto* activity=world_.growth.life_activities.find(h.id);activity&&activity->view.kind==LifeActivityKind::trade)return scalar_start_support(world_,h.id);
 if(original_header(h)){auto reg=registration(world_,runtime_,h.id);if(!reg.ok)return {false,{},reg.error};return read_current<ExactOwnerFactRead>(world_,[&](const GrowthActionTimeRead&reader)->Outcome<ExactOwnerFactRead>{auto original=read_growth_original_time(reader,*reg.value,h.id);if(!original.ok)return {false,{},original.error};ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.push_back({{h.domain,h.kind,h.id},1});return {true,std::move(out),{}};});}
 if(h.domain!="growth"||h.kind!="growth_result_terminal")return fail<ExactOwnerFactRead>("GrowthResultReaderRequired",h.id,ErrorKind::unsupported_capability);
 const auto*p=world_.growth.terminal_publications.find(h.id);if(!p)return fail<ExactOwnerFactRead>("GrowthTerminalPublicationMissing",h.id);auto reg=registration(world_,runtime_,p->action_id);if(!reg.ok)return {false,{},reg.error};
 return read_current<ExactOwnerFactRead>(world_,[&](const GrowthActionTimeRead&reader)->Outcome<ExactOwnerFactRead>{auto original=read_growth_terminal_publication(world_.common,world_.growth,reader,*reg.value,h.id);if(!original.ok)return {false,{},original.error};auto proof=reader.read_action_time_owner(*reg.value,p->action_id);if(!proof.ok)return {false,{},proof.error};if(proof.value.phase!=ActionTimeOwnerPhase::terminal||original.value.rule_version.empty())return fail<ExactOwnerFactRead>("GrowthOriginalTerminalNotApplied",h.id);auto outcome=original_outcome(world_.growth,*p);if(!outcome.ok)return {false,{},outcome.error};
  ExactOwnerFactRead out;DynamicFactViewV2 fact;fact.id=h.id;fact.kind=h.kind;fact.rule_version=original.value.rule_version;fact.occurred_at=p->occurred_at;fact.time_basis=OwnerFactMoment{{"growth","growth_result_terminal",h.id},1};fact.subject_id=p->action_id;fact.outcome=outcome.value.success?"completed":"stopped";fact.cause_fact_ids={p->action_id};for(const auto&actor:proof.value.participant_actor_ids){fact.participants.push_back({{SubjectKind::actor,actor},actor==original.value.actor_id?"actor":"participant"});out.direct_observers.push_back({SubjectKind::actor,actor});}
  out.action=DynamicActionResultV2{p->action_id,original.value.action_kind,h.id,outcome.value.code,p->occurred_at,fact.time_basis,outcome.value.success,proof.value.participant_actor_ids};out.fact=std::move(fact);out.reportable=true;out.supporting_sources.push_back({p->terminal_source_ref,p->terminal_source_schema});return {true,std::move(out),{}};
 });
}
Outcome<WorldMoment> NativeGrowthResultOwnerRead::read_selected_source_moment(const MomentOwnerSource&source)const{
 const auto&ref=source.source_ref;
 // 只把已选市场的两个真实schema2地址交原Growth读口，不提升旧FSL或原业务schema。
 if(source.source_schema==2&&((ref.domain=="growth_market_history"&&ref.kind=="growth_market_movement")||(ref.domain=="growth"&&ref.kind=="life_activity")))return read_current<WorldMoment>(world_,[&](const GrowthActionTimeRead&reader){return reader.read_action_time_source(source);});
 if(source.source_schema!=1)return fail<WorldMoment>("UnsupportedGrowthResultSchema",source.source_ref.id,ErrorKind::unsupported_capability);
 if(ref.domain=="growth"&&ref.kind=="growth_fixed_service_boundary")return read_current<WorldMoment>(world_,[&](const GrowthActionTimeRead&reader){return reader.read_action_time_source(source);});
 if(ref.domain=="growth"&&ref.kind=="growth_result_terminal"){const auto*p=world_.growth.terminal_publications.find(ref.id);if(!p)return fail<WorldMoment>("GrowthTerminalPublicationMissing",ref.id);auto reg=registration(world_,runtime_,p->action_id);if(!reg.ok)return {false,{},reg.error};return read_current<WorldMoment>(world_,[&](const GrowthActionTimeRead&reader)->Outcome<WorldMoment>{auto read=read_growth_terminal_publication(world_.common,world_.growth,reader,*reg.value,ref.id);if(!read.ok)return {false,{},read.error};return {true,read.value.publication->occurred_at,{}};});}
 // 原业务仍保schema1，但scalar精确开始只属于真实MarketMovement2，不开放旧alias。
 if(ref.domain=="growth"&&ref.kind=="life_activity")if(const auto*activity=world_.growth.life_activities.find(ref.id);activity&&activity->view.kind==LifeActivityKind::trade)return fail<WorldMoment>("GrowthScalarStartSourceRequired",ref.id,ErrorKind::unsupported_capability);
 const auto*h=world_.common.state().facts.find(ref.id);const bool direct=h&&h->domain==ref.domain&&h->kind==ref.kind&&original_header(*h);const bool owner_alias=h&&original_header(*h)&&ref.domain=="growth"&&ref.kind==h->kind;if(!direct&&!owner_alias)return fail<WorldMoment>("GrowthResultSourceReaderRequired",ref.id,ErrorKind::unsupported_capability);auto reg=registration(world_,runtime_,ref.id);if(!reg.ok)return {false,{},reg.error};return read_current<WorldMoment>(world_,[&](const GrowthActionTimeRead&reader)->Outcome<WorldMoment>{auto value=read_growth_original_time(reader,*reg.value,ref.id);if(!value.ok)return {false,{},value.error};return {true,value.value.started_at,{}};});
}
Outcome<std::shared_ptr<const ExactResultOwnerRead>> make_native_growth_result_sources(const WorldBundle&w,const CompleteExactTimeRuntime&r){if(!w.catalog||!w.catalog->exact_time||!w.catalog->growth||!w.catalog->common||!w.action_elapsed)return fail<std::shared_ptr<const ExactResultOwnerRead>>("GrowthExactResultDependenciesRequired");return {true,std::allocate_shared<NativeGrowthResultOwnerRead>(CheckpointAllocator<NativeGrowthResultOwnerRead>{},w,r),{}};}
}
