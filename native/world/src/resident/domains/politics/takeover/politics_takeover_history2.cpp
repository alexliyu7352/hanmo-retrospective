#include "resident/domains/military/military_model.hpp"
// 历史按真实Common切点重放原S6行；不递归Task验工，不拿今日组织/账户改写历史。
#include "politics_takeover_history2.hpp"
#include "politics_takeover_construction.hpp"
#include "resident/domains/local_economy/production/facility_construction_adoption.hpp"
#include "politics_takeover_detail.hpp"
#include "resident/domains/local_economy/production/production_adoption_source.hpp"
#include "resident/domains/local_economy/production/production_population_source.hpp"
#include "resident/domains/tasks/task_model.hpp"
namespace hanmo::resident {namespace {
struct HistoryFailure{RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw HistoryFailure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw HistoryFailure{std::move(r.error)};return std::move(r.value);}
void checked(Status r){if(!r.ok)throw HistoryFailure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& common,const StableId& id){const auto& order=common.state().facts.order();auto found=std::find(order.begin(),order.end(),id);if(found==order.end())fail("PoliticalHistoryCommonSourceMissing",id);return static_cast<std::size_t>(found-order.begin());}
bool matches(const PoliticalTakeoverRegistration& row,const PoliticalResponsibilityRef& ref){return std::any_of(row.responsibilities.begin(),row.responsibilities.end(),[&](const auto& item){return takeover_detail::key(item.responsibility)==takeover_detail::key(ref);});}
void selected(const PoliticalTakeoverContext& x,const PoliticalResponsibilityRef& ref){if(x.catalog.version!=2||x.state.version!=2||!x.catalog.enabled||!x.actual_clock||!takeover_detail::valid_ref(ref))fail("PoliticalTakeoverTime2HistoryRequired",ref.responsibility_id);checked(validate_political_takeover_state(x));}
const SupervisionActionStartSource& start_source(const PoliticalTakeoverContext& x,const TaskState& tasks,const SourceRef& source){
 if(source.domain!="tasks"||source.kind!="supervision_action_started"||!tasks.work_execution||!task_work_execution_version(tasks.work_execution->version))fail("PoliticalSupervisionStartSourceRequired",source.id);
 const auto* start=tasks.work_execution->supervision_starts.find(source.id);const auto* head=x.common.state().facts.find(source.id);
 if(!start||!head||start->fact_id!=source.id||head->domain!=source.domain||head->kind!=source.kind)fail("PoliticalSupervisionStartSourceMismatch",source.id);
 const auto* episode=tasks.work_execution->episodes.find(start->work_id);const auto* acceptance=tasks.work_execution->acceptances.find(start->work_acceptance_ref.id);
 if(!episode||!acceptance||start->work_acceptance_ref.domain!="tasks"||start->work_acceptance_ref.kind!="work_accepted"||acceptance->fact_id!=start->work_acceptance_ref.id||acceptance->work_id!=start->work_id||episode->acceptance_source_ref!=acceptance->fact_id||episode->work_id!=start->work_id||!std::holds_alternative<SupervisedWorkPlan>(episode->frozen_plan))fail("PoliticalSupervisionStartIdentityMismatch",source.id);
 const auto& plan=std::get<SupervisedWorkPlan>(episode->frozen_plan);if(plan.supervisor_actor_id!=start->actor_id||start->until_tick!=plan.completion_deadline.tick||position(x.common,start->work_acceptance_ref.id)>=position(x.common,source.id))fail("PoliticalSupervisionStartAcceptanceMismatch",source.id);
 const auto now=take(read_organization_authority_now(x.common,x.actual_clock));if(compare(start->started_at,now)>0||compare(start->started_at,WorldMoment{start->until_tick,{}})>=0)fail("PoliticalSupervisionStartMomentMismatch",source.id);
 return *start;
}
// cut为真实Common序的排他上界；approval的所有decision必须已在此界前实际采用。
PoliticalResponsibilityMomentRead snapshot(const PoliticalTakeoverContext& x,const PoliticalResponsibilityRef& ref,const WorldMoment& at,std::size_t cut){
 const PoliticalTakeoverRegistration* registration=nullptr;std::size_t last=0;
 for(const auto& row:x.state.registrations.rows()){if(!matches(row.second,ref))continue;const auto pos=position(x.common,row.first);if(pos>=cut)continue;if(!row.second.registered_at||compare(*row.second.registered_at,at)>0)fail("PoliticalHistoryMomentCutMismatch",row.first);if(!registration||pos>last){registration=&row.second;last=pos;}}
 PoliticalResponsibilityMomentRead out;if(!registration)return out;
 const auto* rule=x.catalog.rules.find(registration->rule_id);if(!rule)fail("PoliticalTakeoverRuleMissing",registration->rule_id);
 PoliticalResponsibilityExecution execution;execution.registration_fact_id=registration->id;execution.responsibility=ref;execution.registration=takeover_detail::registration_source(x.common,*registration,*rule);execution.code="PoliticalResponsibilityPending";
 for(const auto& item:registration->responsibilities)if(takeover_detail::key(item.responsibility)==takeover_detail::key(ref))execution.original_source_fact_ids=item.source_fact_ids;
 out.actual_sources.push_back({{"politics",political_takeover_pending_fact_kind,registration->id},2});
 const PoliticalTakeoverApprovalRecord* approval=nullptr;last=0;
 for(const auto& row:x.state.approvals.rows()){if(row.second.registration_fact_id!=registration->id)continue;const auto pos=position(x.common,row.first);if(pos>=cut)continue;if(!row.second.approved_at||compare(*row.second.approved_at,at)>0)fail("PoliticalHistoryMomentCutMismatch",row.first);for(const auto& decision:row.second.decision_fact_ids)if(position(x.common,decision)>=cut)fail("PoliticalHistoryInsideApprovalCandidate",row.first);if(!approval||pos>last){approval=&row.second;last=pos;}}
 if(approval){out.actual_sources.push_back({{"politics",political_takeover_fact_kind,approval->id},2});for(const auto& id:approval->decision_fact_ids){const auto* row=x.state.decisions.find(id);if(!row)fail("PoliticalDecisionMissing",id);if(takeover_detail::key(row->decision.responsibility)!=takeover_detail::key(ref))continue;execution.decision=row->decision;out.actual_sources.push_back({{"politics",political_takeover_decision_fact_kind,id},2});break;}if(!execution.decision)fail("PoliticalResponsibilityCoverageMismatch",approval->id);const auto d=execution.decision->disposition;const bool permitted=d==PoliticalResponsibilityDisposition::continue_under_original||d==PoliticalResponsibilityDisposition::assume_by_successor;execution.status=permitted?PoliticalResponsibilityExecutionStatus::approved:PoliticalResponsibilityExecutionStatus::suspended;execution.code=permitted?StableId{}:StableId{"PoliticalResponsibilitySuspended"};}
 out.execution=std::move(execution);return out;
}
EconomicProductionAdoption production_adoption(const PoliticalTakeoverContext& x,
 const EconomicProductionAdoptionRead& original,const PoliticalResponsibilityRef& ref,
 const StableId& unit,const MomentOwnerSource& source){
 const auto& r=source.source_ref;
 const bool selected_source=is_original_production_population_change(source)||(r.domain=="local_economy"&&
  ((source.source_schema==2&&(r.kind=="production_cycle_started"||r.kind=="production_input_segment"))||
   (source.source_schema==1&&(r.kind=="facility_condition_change"||r.kind=="local_ration"||r.kind=="economic_period_opened"))));
 if(unit.empty()||!valid_source_ref(r)||!selected_source)
  fail("PoliticalProductionAdoptionSourceRequired",r.id);
 auto actual=take(original.read_production_adoption(unit,source));
 const auto* head=x.common.state().facts.find(r.id);
 if(!head||head->id!=r.id||head->domain!=r.domain||head->kind!=r.kind||
    actual.unit_id!=unit||actual.actual_source.source_schema!=source.source_schema||
    !same_source_ref(actual.actual_source.source_ref,r))
  fail("PoliticalProductionAdoptionSourceMismatch",r.id);
 take(encode_world_moment(actual.occurred_at));
 const auto now=take(read_organization_authority_now(x.common,x.actual_clock));
 if(compare(actual.occurred_at,now)>0)fail("PoliticalProductionAdoptionInFuture",r.id);
 for(std::size_t i=0;i<actual.production_asset_ids.size();++i){
  if(actual.production_asset_ids[i].empty())fail("PoliticalProductionAssetIdentityMissing",r.id);
  for(std::size_t j=0;j<i;++j)if(actual.production_asset_ids[i]==actual.production_asset_ids[j])
   fail("PoliticalProductionAssetIdentityRepeated",actual.production_asset_ids[i]);
 }
 if(ref.kind==PoliticalResponsibilityKind::production_unit){
  if(ref.responsibility_id!=unit||ref.unit_id||ref.asset_id)
   fail("PoliticalProductionResponsibilityMismatch",ref.responsibility_id);
 }else if(ref.kind==PoliticalResponsibilityKind::production_asset_use){
  if(ref.unit_id!=std::optional<StableId>{unit}||ref.asset_id!=std::optional<StableId>{ref.responsibility_id}||
     std::count(actual.production_asset_ids.begin(),actual.production_asset_ids.end(),ref.responsibility_id)!=1)
   fail("PoliticalProductionAssetResponsibilityMismatch",ref.responsibility_id);
 }else fail("PoliticalProductionResponsibilityKind",ref.responsibility_id);
 position(x.common,r.id);return actual;
}
// 使用 S5 原采用读口反查实际条款、成本和 Action；不回入 S6 当前权源。
const FacilityConstruction &
construction_adoption(const PoliticalTakeoverContext &context,
                      const FacilityConstructionAdoptionContext &adoption,
                      const PoliticalResponsibilityRef &ref,
                      const MomentOwnerSource &source) {
  if (&context.common != &adoption.common ||
      &context.common_catalog != &adoption.common_catalog ||
      compare(take(read_organization_authority_now(context.common,
                                                   context.actual_clock)),
              take(adoption.clock.read_current_moment())) != 0)
    fail("PoliticalConstructionCandidateMismatch", source.source_ref.id);
  const auto *actual =
      take(read_facility_construction_adoption_for_original_dependency(adoption,
                                                                       source));
  if (!actual)
    fail("PoliticalConstructionAdoptionMissing", source.source_ref.id);
  const auto *terms = take(
      read_facility_construction_terms(adoption.catalog, actual->terms_ref));
  if (ref.kind != PoliticalResponsibilityKind::production_asset_use ||
      !ref.unit_id || ref.unit_id->empty() ||
      ref.asset_id != std::optional<StableId>{ref.responsibility_id} ||
      !terms || ref.responsibility_id != terms->asset_ref)
    fail("PoliticalConstructionResponsibilityMismatch", ref.responsibility_id);
  const auto *plan = adoption.assets.plans.find(*ref.unit_id);
  const auto *asset = adoption.assets.assets.find(terms->asset_ref);
  if (adoption.assets.version != 1 || !plan || plan->unit_id != *ref.unit_id ||
      !asset || asset->id != terms->asset_ref ||
      asset->kind != ProductionAssetKind::facility || asset->source_key.empty())
    fail("PoliticalConstructionAssetScopeMismatch", ref.responsibility_id);
  std::size_t matching_uses = 0;
  for (const auto &use : plan->asset_uses)
    if (use.asset_id == asset->id) {
      ++matching_uses;
      if (use.units <= 0 || use.batches_per_unit <= 0 ||
          use.units > asset->capacity_units)
        fail("PoliticalConstructionAssetUseMismatch", asset->id);
    }
  if (matching_uses != 1)
    fail("PoliticalConstructionAssetScopeMismatch", ref.responsibility_id);
  // 原 S5 源证明 started_at 和实际 Common cut；这里不另造领域事实。
  position(context.common, source.source_ref.id);
  return *actual;
}

struct Event{SourceRef source;WorldMoment at;std::size_t pos=0,after=0;};
std::vector<Event> events(const PoliticalTakeoverContext& x,const PoliticalResponsibilityRef& ref){std::vector<Event> out;
 for(const auto& row:x.state.registrations.rows())if(matches(row.second,ref)){const auto pos=position(x.common,row.first);out.push_back({{"politics",political_takeover_pending_fact_kind,row.first},*row.second.registered_at,pos,pos+1});}
 for(const auto& row:x.state.approvals.rows()){const auto* r=x.state.registrations.find(row.second.registration_fact_id);if(!r||!matches(*r,ref))continue;const auto pos=position(x.common,row.first);auto after=pos+1;for(const auto& id:row.second.decision_fact_ids)after=std::max(after,position(x.common,id)+1);out.push_back({{"politics",political_takeover_fact_kind,row.first},*row.second.approved_at,pos,after});}
 std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.pos<b.pos;});return out;
}
struct ProductionChangeCut { WorldMoment at;std::size_t after=0; };
ProductionChangeCut production_change_cut(const PoliticalTakeoverContext& x,
 const EconomicProductionAdoptionRead& original,const PoliticalResponsibilityRef* queried,
 const StableId& unit,const MomentOwnerSource& source){
 const auto& r=source.source_ref;
 if(unit.empty()||source.source_schema!=2||r.domain!="politics"||!valid_source_ref(r)||
    (r.kind!=political_takeover_pending_fact_kind&&r.kind!=political_takeover_fact_kind))
  fail("PoliticalProductionResponsibilityChangeRequired",r.id);
 const auto at=take(read_political_takeover_source_moment2(x,r,source.source_schema));
 const auto pos=position(x.common,r.id);auto after=pos+1;
 const PoliticalTakeoverRegistration* registration=nullptr;
 if(r.kind==political_takeover_pending_fact_kind)registration=x.state.registrations.find(r.id);
 else{
  const auto* approval=x.state.approvals.find(r.id);
  if(!approval)fail("PoliticalProductionApprovalMissing",r.id);
  registration=x.state.registrations.find(approval->registration_fact_id);
  // selected() has already revalidated all original decisions and their real
  // responsibility/authority sources. Do not observe a partially written approval.
  for(const auto& id:approval->decision_fact_ids){
   const auto* decision=x.state.decisions.find(id);const auto decision_pos=position(x.common,id);
   if(!decision||decision->approval_fact_id!=approval->id||decision_pos<=pos)
    fail("PoliticalProductionApprovalNotFullyApplied",r.id);
   after=std::max(after,decision_pos+1);
  }
 }
 // The original plan proves the full input scope independently of politics.
 // A change to asset A must also reread the unchanged unit/B at this same cut.
 const auto scope=take(original.read_original_production_asset_scope(unit));
 if(scope.unit_id!=unit)fail("PoliticalProductionAssetScopeMismatch",unit);
 for(std::size_t i=0;i<scope.production_asset_ids.size();++i){
  if(scope.production_asset_ids[i].empty())fail("PoliticalProductionAssetIdentityMissing",unit);
  for(std::size_t j=0;j<i;++j)if(scope.production_asset_ids[i]==scope.production_asset_ids[j])
   fail("PoliticalProductionAssetIdentityRepeated",scope.production_asset_ids[i]);
 }
 const auto in_scope=[&](const PoliticalResponsibilityRef& ref){
  if(ref.kind==PoliticalResponsibilityKind::production_unit)
   return ref.responsibility_id==unit&&!ref.unit_id&&!ref.asset_id;
  return ref.kind==PoliticalResponsibilityKind::production_asset_use&&
   ref.unit_id==std::optional<StableId>{unit}&&ref.asset_id==std::optional<StableId>{ref.responsibility_id}&&
   std::count(scope.production_asset_ids.begin(),scope.production_asset_ids.end(),ref.responsibility_id)==1;
 };
 // selected() revalidated this original pre-transfer roster. The actual change
 // must affect this plan, but a queried unchanged responsibility need not be in it.
 if(!registration||!std::any_of(registration->responsibilities.begin(),registration->responsibilities.end(),
    [&](const auto& item){return in_scope(item.responsibility);}))
  fail("PoliticalProductionChangeResponsibilityMismatch",unit);
 if(queried&&!in_scope(*queried))fail("PoliticalProductionAssetResponsibilityMismatch",queried->responsibility_id);
 // The original writer completes one approval atomically. Reject any other S6
 // registration/approval interleaved before its final decision, for any scope.
 for(const auto& row:x.state.registrations.rows()){
  const auto other=position(x.common,row.first);if(other>pos&&other<after)
   fail("PoliticalHistoryInterleavedApprovalCandidate",r.id);
 }
 for(const auto& row:x.state.approvals.rows()){
  const auto other=position(x.common,row.first);if(other>pos&&other<after)
   fail("PoliticalHistoryInterleavedApprovalCandidate",r.id);
 }
 return {at,after};
}
}}
namespace hanmo::resident {
Outcome<PoliticalResponsibilityMomentRead> NativePoliticalSupervisionTakeoverRead::read_before_supervision_start(const PoliticalResponsibilityRef& ref,const WorldMoment& at,const SourceRef* cut)const{try{
 selected(context_,ref);std::size_t pos=context_.common.state().facts.order().size();
 if(cut){const auto& start=start_source(context_,tasks_,*cut);if(compare(start.started_at,at)!=0)fail("PoliticalSupervisionStartMomentMismatch",cut->id);pos=position(context_.common,cut->id);}
 else if(compare(at,take(read_organization_authority_now(context_.common,context_.actual_clock)))!=0)fail("PoliticalSupervisionCurrentMomentMismatch");
 return {true,snapshot(context_,ref,at,pos),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<PoliticalResponsibilityWindowRead> NativePoliticalSupervisionTakeoverRead::read_supervision_responsibility_window(const PoliticalResponsibilityRef& ref,const SupervisionActionStartSource& claimed,const MomentSpan& span)const{try{
 selected(context_,ref);const auto& start=start_source(context_,tasks_,{"tasks","supervision_action_started",claimed.fact_id});
 if(start.action_id!=claimed.action_id||start.work_id!=claimed.work_id||start.actor_id!=claimed.actor_id||compare(start.started_at,claimed.started_at)!=0||start.until_tick!=claimed.until_tick)fail("PoliticalSupervisionStartClaimMismatch",claimed.fact_id);
 // Work/5 的 until_tick 保留目标期限；真实 F 窗由 S3 原批准 H/S0 核验。
 // 本口只重放该真实服务片的接管历史，不能再以目标日期截断 F 服务。
 const auto now=take(read_organization_authority_now(context_.common,context_.actual_clock));if(compare(span.from,span.to)>=0||compare(span.from,start.started_at)<0||compare(span.to,now)>0||(tasks_.work_execution->version!=5&&compare(span.to,WorldMoment{start.until_tick,{}})>0))fail("PoliticalSupervisionServiceWindowMismatch",start.fact_id);
 const auto all=events(context_,ref);const auto start_pos=position(context_.common,start.fact_id);auto initial_cut=context_.common.state().facts.order().size();
 if(compare(span.from,start.started_at)==0)initial_cut=start_pos;else for(const auto& event:all)if(compare(event.at,span.from)>=0){initial_cut=event.pos;break;}
 PoliticalResponsibilityWindowRead out;out.window=span;out.initial=snapshot(context_,ref,span.from,initial_cut);
 for(const auto& event:all){if(event.pos<start_pos||compare(event.at,span.from)<0||compare(event.at,span.to)>=0)continue;out.changes.push_back({{event.source,2},event.at,snapshot(context_,ref,event.at,event.after)});}
 return {true,std::move(out),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<PoliticalResponsibilityMomentRead> NativePoliticalProductionTakeoverRead::read_before_production_adoption(
 const PoliticalResponsibilityRef& ref,const StableId& unit,const MomentOwnerSource& source)const{try{
 selected(context_,ref);const auto original=production_adoption(context_,adoptions_,ref,unit,source);
 return {true,snapshot(context_,ref,original.occurred_at,position(context_.common,source.source_ref.id)),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<PoliticalResponsibilityWindowRead> NativePoliticalProductionTakeoverRead::read_production_responsibility_window(
 const PoliticalResponsibilityRef& ref,const StableId& unit,const MomentOwnerSource& source,const MomentSpan& span)const{try{
 selected(context_,ref);const auto original=production_adoption(context_,adoptions_,ref,unit,source);
 const auto now=take(read_organization_authority_now(context_.common,context_.actual_clock));
 if(compare(span.from,span.to)>=0||compare(span.from,original.occurred_at)<0||compare(span.to,now)>0)
  fail("PoliticalProductionResponsibilityWindowMismatch",source.source_ref.id);
 const auto all=events(context_,ref);const auto adopted=position(context_.common,source.source_ref.id);
 auto initial_cut=context_.common.state().facts.order().size();
 if(compare(span.from,original.occurred_at)==0)initial_cut=adopted;
 else for(const auto& event:all)if(compare(event.at,span.from)>=0){initial_cut=event.pos;break;}
 PoliticalResponsibilityWindowRead out;out.window=span;out.initial=snapshot(context_,ref,span.from,initial_cut);
 for(const auto& event:all){
  if(event.pos<adopted||compare(event.at,span.from)<0||compare(event.at,span.to)>=0)continue;
  out.changes.push_back({{event.source,2},event.at,snapshot(context_,ref,event.at,event.after)});
 }
 return {true,std::move(out),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<WorldMoment> NativePoliticalProductionTakeoverRead::read_production_responsibility_change_moment(
 const StableId& unit,const MomentOwnerSource& source)const{try{
 PoliticalResponsibilityRef ref;ref.kind=PoliticalResponsibilityKind::production_unit;ref.responsibility_id=unit;
 selected(context_,ref);const auto cut=production_change_cut(context_,adoptions_,nullptr,unit,source);
 return {true,cut.at,{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<PoliticalResponsibilityMomentRead> NativePoliticalProductionTakeoverRead::read_after_production_responsibility_change(
 const PoliticalResponsibilityRef& ref,const StableId& unit,const MomentOwnerSource& source)const{try{
 selected(context_,ref);const auto cut=production_change_cut(context_,adoptions_,&ref,unit,source);
 return {true,snapshot(context_,ref,cut.at,cut.after),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}
Outcome<PoliticalResponsibilityWindowRead> NativePoliticalProductionTakeoverRead::read_production_responsibility_window_after_change(
 const PoliticalResponsibilityRef& ref,const StableId& unit,const MomentOwnerSource& source,const MomentSpan& span)const{try{
 selected(context_,ref);const auto cut=production_change_cut(context_,adoptions_,&ref,unit,source);
 const auto now=take(read_organization_authority_now(context_.common,context_.actual_clock));
 if(compare(span.from,span.to)>=0||compare(span.from,cut.at)<0||compare(span.to,now)>0)
  fail("PoliticalProductionResponsibilityWindowMismatch",source.source_ref.id);
 const auto all=events(context_,ref);auto initial_cut=context_.common.state().facts.order().size();
 if(compare(span.from,cut.at)==0)initial_cut=cut.after;
 else for(const auto& event:all)if(event.pos>=cut.after&&compare(event.at,span.from)>=0){initial_cut=event.pos;break;}
 PoliticalResponsibilityWindowRead out;out.window=span;out.initial=snapshot(context_,ref,span.from,initial_cut);
 for(const auto& event:all){
  if(event.pos<cut.after||compare(event.at,span.from)<0||compare(event.at,span.to)>=0)continue;
  out.changes.push_back({{event.source,2},event.at,snapshot(context_,ref,event.at,event.after)});
 }
 return {true,std::move(out),{}};
 }catch(const HistoryFailure& e){return {false,{},e.error};}}

// 整个原接管 validator 先行；只换入真实建设采用 cut，不放宽旧生产白名单。
Outcome<PoliticalResponsibilityMomentRead>
read_political_before_facility_construction(
    const PoliticalTakeoverContext &context,
    const FacilityConstructionAdoptionContext &adoption,
    const PoliticalResponsibilityRef &ref, const MomentOwnerSource &source) {
  try {
    selected(context, ref);
    const auto &actual = construction_adoption(context, adoption, ref, source);
    return {true,
            snapshot(context, ref, actual.started_at,
                     position(context.common, source.source_ref.id)),
            {}};
  } catch (const HistoryFailure &failure) {
    return {false, {}, failure.error};
  }
}

// 原共同序与完整批准快照直接复用；不以首尾 Owner 相同跳过中间停止。
Outcome<PoliticalResponsibilityWindowRead>
read_political_facility_construction_responsibility_window(
    const PoliticalTakeoverContext &context,
    const FacilityConstructionAdoptionContext &adoption,
    const PoliticalResponsibilityRef &ref, const MomentOwnerSource &source,
    const MomentSpan &span) {
  try {
    selected(context, ref);
    const auto &actual = construction_adoption(context, adoption, ref, source);
    const auto now = take(
        read_organization_authority_now(context.common, context.actual_clock));
    if (compare(span.from, span.to) >= 0 ||
        compare(span.from, actual.started_at) < 0 || compare(span.to, now) > 0)
      fail("PoliticalConstructionResponsibilityWindowMismatch",
           source.source_ref.id);
    const auto all = events(context, ref);
    const auto adopted = position(context.common, source.source_ref.id);
    auto initial_cut = context.common.state().facts.order().size();
    if (compare(span.from, actual.started_at) == 0)
      initial_cut = adopted;
    else
      for (const auto &event : all)
        if (compare(event.at, span.from) >= 0) {
          initial_cut = event.pos;
          break;
        }
    PoliticalResponsibilityWindowRead out;
    out.window = span;
    out.initial = snapshot(context, ref, span.from, initial_cut);
    for (const auto &event : all) {
      if (event.pos < adopted || compare(event.at, span.from) < 0 ||
          compare(event.at, span.to) >= 0)
        continue;
      out.changes.push_back({{event.source, 2},
                             event.at,
                             snapshot(context, ref, event.at, event.after)});
    }
    return {true, std::move(out), {}};
  } catch (const HistoryFailure &failure) {
    return {false, {}, failure.error};
  }
}
}
