#include "military_supply_exact.hpp"
#include "military_supply_lookup.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include "resident/kernel/typed_supply_observation.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {using namespace movement_detail;
namespace {
thread_local MilitarySupplyLookupPass* active_supply_lookup = nullptr;
}
// 排序的借用行仅加速候选定位；原 row 内容、Common cut 和 period 每次仍由原 reader 检查。
struct MilitarySupplyLookupPass::Index {
 // 原入口没有 TLS job 时，仅此索引拥有工程 scratch job；不改原业务的 TLS。
 CheckpointJobBudget owned_job;
 std::shared_ptr<CheckpointMemoryResource> caller_resource, resource;
 CheckpointVector<const MilitarySupplyFact*> rows;
 const MilitarySupplyFact* original_data = nullptr;
 std::size_t original_size = 0;
 bool ready = false;
 Index(std::shared_ptr<CheckpointMemoryResource> caller,
       std::shared_ptr<CheckpointMemoryResource> allocation)
   :caller_resource(std::move(caller)),resource(std::move(allocation)),
    rows(CheckpointAllocator<const MilitarySupplyFact*>(resource)){}
 // 先撤掉全部行借用，但同一 pass 的 job/首错不因追加或回滚而换代。
 void invalidate() noexcept {
  rows.clear();original_data=nullptr;original_size=0;ready=false;
 }
};
MilitarySupplyLookupPass::MilitarySupplyLookupPass(
 const CommonState& common,const MilitaryState& military) noexcept
 :common_(&common),military_(&military),previous_(active_supply_lookup){active_supply_lookup=this;}
MilitarySupplyLookupPass::~MilitarySupplyLookupPass() noexcept {stop();}
void MilitarySupplyLookupPass::stop() noexcept {
 if(!active_)return;
 index_.reset();active_supply_lookup=previous_;active_=false;
}
void invalidate_military_supply_lookup(const MilitaryState& military) noexcept {
 for(auto* pass=active_supply_lookup;pass;pass=pass->previous_)
  if(pass->military_==&military&&pass->index_)pass->index_->invalidate();
}
const MilitarySupplyFact* find_field_supply_candidate(
 const CommonStore& c,const MilitaryState& military,const SourceRef& ref,Integer schema,std::size_t cut){
 const auto visit=[&](const MilitarySupplyFact& row){
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::loop_candidate_visit,
   "military.supply.lookup_candidates",&c.state(),nullptr,{"fact","military","military_supply",row.id,schema},{},world_stage_observation::observed_cut(cut)});
 };
 auto* pass=active_supply_lookup;
 const auto caller_resource=current_checkpoint_memory();
 // 没有明确 pass、另一根或另一 caller job，只走原逐行候选规则。
 // 空 caller TLS 仍是合法常规入口，后面给局部索引自己的真 scratch job。
 if(!pass||pass->common_!=&c.state()||pass->military_!=&military||
    (pass->index_&&pass->index_->caller_resource!=caller_resource)){
  const MilitarySupplyFact* original=nullptr;
  for(const auto& row:military.supply_facts){visit(row);if(row.id==ref.id){
   if(original)fail("MilitarySupplySourceRepeated",ref.id);original=&row;
  }}
  return original;
 }
 if(!pass->index_){
  CheckpointJobBudget owned_job;
  auto resource=caller_resource;
  if(!resource){
   auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),
                                        engineering_slot_capacity_profile());
   if(!job.ok)throw checkpoint_job_detail::FailureException{std::move(job.error)};
   owned_job=std::move(job.value);
   auto memory=checkpoint_memory_resource(owned_job,CheckpointJobUse::scratch);
   if(!memory.ok)throw checkpoint_job_detail::FailureException{std::move(memory.error)};
   resource=std::move(memory.value);
  }
  // 显式 allocator 只作用于索引，不安装 ScopedCheckpointMemory 或重绑 typed reader。
  try {
   auto index=std::allocate_shared<MilitarySupplyLookupPass::Index>(
    CheckpointAllocator<MilitarySupplyLookupPass::Index>(resource),caller_resource,resource);
   index->owned_job=std::move(owned_job);
   pass->index_=std::move(index);
  }catch(const std::bad_alloc&){
   if(!caller_resource)throw checkpoint_job_detail::FailureException{
    owned_job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};
   throw;
  }
 }
 auto& index=*pass->index_;
 // 真 writer 已先撤销；地址／数量检查只作为额外防陈旧检查，不冒充写入版本。
 if(index.ready&&(index.original_data!=military.supply_facts.data()||
    index.original_size!=military.supply_facts.size()))index.invalidate();
 // 复用容量也仍核原 job 的取消／首错；不因没有新分配而借失败 job 继续。
 index.resource->reserve_external(0);
 const bool built=index.ready;
 if(!built){
  try {
   index.rows.clear();
   index.rows.reserve(military.supply_facts.size());
   for(const auto& row:military.supply_facts){visit(row);index.rows.push_back(&row);}
   std::sort(index.rows.begin(),index.rows.end(),[](const auto* a,const auto* b){
    return a->id<b->id||(a->id==b->id&&a<b);
   });
   index.original_data=military.supply_facts.data();index.original_size=military.supply_facts.size();
   index.ready=true;
  }catch(const std::bad_alloc&){
   if(!caller_resource)throw checkpoint_job_detail::FailureException{
    index.owned_job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};
   throw;
  }catch(const std::length_error&){
   if(!caller_resource)throw checkpoint_job_detail::FailureException{
    index.owned_job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};
   throw;
  }
 }
 world_stage_observation::observe_node({built?world_stage_observation::NodeMetric::cache_hit:world_stage_observation::NodeMetric::cache_miss,
  "military.supply.candidate_index",&c.state(),nullptr,{"fact",ref.domain,ref.kind,ref.id,schema},{},world_stage_observation::observed_cut(cut)});
 const auto& rows=pass->index_->rows;
 const auto found=std::lower_bound(rows.begin(),rows.end(),ref.id,
  [](const auto* row,const StableId& id){return row->id<id;});
 if(found==rows.end()||(*found)->id!=ref.id)return nullptr;
 if(found+1!=rows.end()&&(*(found+1))->id==ref.id)fail("MilitarySupplySourceRepeated",ref.id);
 return *found;
}
namespace {
std::size_t position(const CommonStore& c,const SourceRef& ref){
 return take(read_actual_common_fact_position(c,ref));
}
void original_id(const CommonStore& c,const StableId& id){constexpr char prefix[]="military:";if(id.rfind(prefix,0)!=0)fail("MilitarySupplyOriginalAllocationRequired",id);auto text=id.substr(sizeof(prefix)-1);Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||text!=std::to_string(n))fail("MilitarySupplyOriginalAllocationRequired",id);}
// 原 cause 只允许本期 Opening 或一份真实 Common 群旅头；后者的 typed 旅程/重算闭包由 S1 owner 验证。
void supply_cause(const CommonStore& c,const MilitarySupplyFact& supply,
 const ExactTrainingPeriodView& period,std::size_t cut){
 if(supply.cause_fact_ids.size()!=1)fail("MilitarySupplyActualCauseRequired",supply.id);
 const auto& id=supply.cause_fact_ids.front();const auto* head=c.state().facts.find(id);
 if(!head||position(c,{head->domain,head->kind,id})>=cut)fail("MilitarySupplyCauseCutMismatch",supply.id);
 world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "military.supply.cause", &c.state(), nullptr, {"fact", "military", "military_supply", supply.id, 0}, {"fact",head->domain,head->kind,id,0}, world_stage_observation::observed_cut(cut)});
 if(id==period.original_opening.source_ref.id){
  if(head->domain!=period.original_opening.source_ref.domain||head->kind!=period.original_opening.source_ref.kind)fail("MilitarySupplyOpeningCauseMismatch",supply.id);
 }else if(supply.army_id.empty()||!supply.garrison_id.empty()||head->domain!="travel"||head->kind!="group_journey")
  fail("MilitarySupplyJourneyCauseRequired",supply.id);
}
void actor_existed(const CommonStore& c,const StableId& actor,const WorldMoment& at,std::size_t cut){
 const auto* a=c.state().actors.find(actor);if(!a||a->id!=actor||(a->created_tick&&(*a->created_tick<0||*a->created_tick>at.half_day_tick)))fail("MilitarySupplyOriginalActorRequired",actor);
 // Dynamic creation has its original same-identity Common source. Its actual
 // append must precede Supply even when both happened at the same exact M.
 const auto* creation=c.state().facts.find(actor);
 if(creation&&creation->kind=="actor_creation"){
  if((creation->domain!="family"&&creation->domain!="family_birth")||position(c,{creation->domain,creation->kind,actor})>=cut)fail("MilitarySupplyActorCreationCutMismatch",actor);
 }else if(a->created_tick&&*a->created_tick!=0)fail("MilitarySupplyActorCreationSourceRequired",actor);
 // Original authored initialization sets created_tick=0. S4's locked authored
 // origin (or actual dynamic creation) still proves that initial branch and M;
 // this source-local reader does not infer author identity from the zero alone.
}
}
Outcome<ExactMilitarySupplySourceView> read_exact_military_supply_source(const CommonStore& c,const MilitaryState& military,const MilitaryExactState& exact,const ExactTrainingPeriodRead& periods,const SourceRef& ref,Integer schema,const WorldMoment& now,ExactArithmeticBudget budget){return guard<ExactMilitarySupplySourceView>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_supply")fail("ExactMilitarySupplySourceRequired",ref.id);original_id(c,ref.id);const auto cut=position(c,ref);take(encode_world_moment(now,budget));
 const MilitarySupplyFact* original=nullptr;for(const auto& row:military.supply_facts)if(row.id==ref.id){if(original)fail("MilitarySupplySourceRepeated",ref.id);original=&row;}
 const auto* source=exact.supply_effect_sources.find(ref.id);
 if(!original||!source||!same_source(source->original_supply_fact_ref,ref)||source->period_ref.empty()||source->original_rule_ref!=original_military_supply_rule_ref||original->army_id.empty()==original->garrison_id.empty()||original->tick<1||original->required<0||original->consumed<0||original->consumed>original->required||original->shortage!=original->required-original->consumed||source->shortage_value!=original->shortage)fail("MilitarySupplySourceMismatch",ref.id);
 const auto period=take(periods.read_training_period(original->tick-1));
 if(period.period_ref!=source->period_ref||period.from_tick!=original->tick-1||period.to_tick!=original->tick||period.original_opening.source_schema!=1||!same_source(period.original_opening.source_ref,source->opening_source_ref)||!same_moment(period.opening_at,source->occurred_at,budget)||take(compare_world_moments(source->occurred_at,now,budget))>0||position(c,source->opening_source_ref)>=cut)fail("MilitarySupplyActualPeriodMismatch",ref.id);
 supply_cause(c,*original,period,cut);
 const auto& mouths=original->mouth_ids;if(!std::is_sorted(mouths.begin(),mouths.end())||std::adjacent_find(mouths.begin(),mouths.end())!=mouths.end())fail("MilitarySupplyMouthOrderMismatch",ref.id);
 std::size_t selected=0;for(const auto& id:mouths){if(id.empty())fail("MilitarySupplyMouthRequired",ref.id);if(!c.actor(id))continue;actor_existed(c,id,source->occurred_at,cut);if(original->shortage>0){if(selected>=source->selected_named_actor_ids.size()||source->selected_named_actor_ids[selected]!=id)fail("MilitarySupplyNamedSelectionMismatch",ref.id);++selected;}}
 if(selected!=source->selected_named_actor_ids.size())fail("MilitarySupplyNamedSelectionMismatch",ref.id);
 // The once key is period + original mouth, not source + selected wounded
 // actor. Full-fed and non-Actor mouths retain the same unique responsibility.
 for(const auto& other:military.supply_facts)if(other.id!=original->id&&other.tick==original->tick){
  for(const auto& mouth:mouths)if(std::find(other.mouth_ids.begin(),other.mouth_ids.end(),mouth)!=other.mouth_ids.end())fail("DuplicateMilitarySupplyMouthResponsibility",mouth);
 }
 return ExactMilitarySupplySourceView{original,source,period};
});}
Outcome<FieldMilitarySupplySourceView> read_field_military_supply_source(
    const CommonStore& c,const MilitaryState& military,const ExactTrainingPeriodRead& periods,
    const SourceRef& ref,Integer schema,const WorldMoment& now,ExactArithmeticBudget budget) {
 world_stage_observation::observe_node({world_stage_observation::NodeMetric::api_attempt, "military.supply.field_reader", &c.state(), nullptr, {"fact", ref.domain, ref.kind, ref.id, schema}, {}, -1, now.half_day_tick, true});
 return guard<FieldMilitarySupplySourceView>([&] {
  typed_supply_observation::Context observed;
  observed.common_identity=&c.state();observed.world_id=c.state().identity.world_id;
  observed.revision=c.state().identity.revision;observed.execution_epoch=c.state().identity.execution_epoch;
  observed.common_tick=c.state().identity.tick;observed.fact_count=c.state().facts.order().size();
  observed.original_roots={&military,&periods};observed.source={ref.domain,ref.kind,ref.id,schema};
  typed_supply_observation::Scope observation(typed_supply_observation::Stage::field_supply_reader,observed);
  if(schema!=1||ref.domain!="military"||ref.kind!="military_supply")
   fail("FieldMilitarySupplySourceRequired",ref.id);
  original_id(c,ref.id);const auto cut=position(c,ref);observation.cut(cut);
  // 复用原唯一编码的临时值；同步观察后即销毁，不延长到下一业务调用。
  {const auto encoded_now=take(encode_world_moment(now,budget));observation.moment("actual_now",encoded_now);}
  const MilitarySupplyFact* original=find_field_supply_candidate(c,military,ref,schema,cut);
  if(!original||original->army_id.empty()==original->garrison_id.empty()||original->tick<1||
     original->required<0||original->consumed<0||original->consumed>original->required||
     original->shortage!=original->required-original->consumed)
   fail("MilitarySupplySourceMismatch",ref.id);
  const auto period=take(periods.read_training_period(original->tick-1));
  if(period.period_ref.empty()||period.from_tick!=original->tick-1||period.to_tick!=original->tick||
     period.original_opening.source_schema!=1||period.opening_at.half_day_tick!=period.from_tick||
     compare(period.opening_at.sub_half_day,{},budget)!=0||
     take(compare_world_moments(period.opening_at,now,budget))>0)
   fail("MilitarySupplyActualPeriodMismatch",ref.id);
  const auto opening_cut=position(c,period.original_opening.source_ref);
  if(opening_cut>=cut)fail("MilitarySupplyActualPeriodMismatch",ref.id);
  // /1 的 tick 是该期 to 标签，不是当前时钟。真供粮头须仍属于
  // 该唯一 Opening 的 Common 区间，不能把后期补记伪装为早期供粮。
  for(const auto& row:c.state().facts.rows())
   if(row.second.domain==period.original_opening.source_ref.domain&&
      row.second.kind==period.original_opening.source_ref.kind&&row.first!=period.original_opening.source_ref.id){
    const auto other=position(c,{row.second.domain,row.second.kind,row.first});
    if(other>opening_cut&&other<cut)fail("MilitarySupplyOpeningCutMismatch",ref.id);
   }
  supply_cause(c,*original,period,cut);
 const auto& mouths=original->mouth_ids;
  if(!std::is_sorted(mouths.begin(),mouths.end())||std::adjacent_find(mouths.begin(),mouths.end())!=mouths.end())
   fail("MilitarySupplyMouthOrderMismatch",ref.id);
  for(const auto& id:mouths){if(id.empty())fail("MilitarySupplyMouthRequired",ref.id);
   if(c.actor(id))actor_existed(c,id,period.opening_at,opening_cut);
  }
  for(const auto& other:military.supply_facts)if(other.id!=original->id&&other.tick==original->tick)
   for(const auto& mouth:mouths)
    if(std::find(other.mouth_ids.begin(),other.mouth_ids.end(),mouth)!=other.mouth_ids.end())
     fail("DuplicateMilitarySupplyMouthResponsibility",mouth);
  world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "military.supply.field_validated", &c.state(), nullptr, {"fact", ref.domain, ref.kind, ref.id, schema}, {}, world_stage_observation::observed_cut(cut), period.opening_at.half_day_tick, true});
  observation.stop();
  return FieldMilitarySupplySourceView{original,period};
 });
}
Outcome<ExactMilitarySupplyActorConsequence> read_field_military_supply_actor_consequence(
 const CommonStore& c,const MilitaryState& military,const ExactTrainingPeriodRead& periods,
 const SourceRef& ref,Integer schema,Integer ordinal,const WorldMoment& now,ExactArithmeticBudget budget){
 return guard<ExactMilitarySupplyActorConsequence>([&]{
  const auto source=take(read_field_military_supply_source(c,military,periods,ref,schema,now,budget));
  const auto& original=*source.original_supply;
  if(original.shortage<=0||ordinal<0||std::size_t(ordinal)>=original.mouth_ids.size()||
     !c.actor(original.mouth_ids[std::size_t(ordinal)]))
   fail("MilitarySupplyActualActorConsequenceRequired",ref.id);
  return ExactMilitarySupplyActorConsequence{original.mouth_ids[std::size_t(ordinal)],ref,
    source.original_period.period_ref,source.original_period.original_opening,
    source.original_period.opening_at,ordinal,original_military_supply_rule_ref};
 });
}
Outcome<ExactMilitarySupplyActorConsequence> read_exact_military_supply_actor_consequence(const CommonStore& c,const MilitaryState& military,const MilitaryExactState& exact,const ExactTrainingPeriodRead& periods,const SourceRef& ref,Integer schema,Integer ordinal,const WorldMoment& now,ExactArithmeticBudget budget){return guard<ExactMilitarySupplyActorConsequence>([&]{
 const auto source=take(read_exact_military_supply_source(c,military,exact,periods,ref,schema,now,budget));const auto& original=*source.original_supply;const auto& actual=*source.exact_source;
 if(original.shortage<=0||ordinal<0||std::size_t(ordinal)>=original.mouth_ids.size()||!c.actor(original.mouth_ids[std::size_t(ordinal)]))fail("MilitarySupplyActualActorConsequenceRequired",ref.id);
 return ExactMilitarySupplyActorConsequence{original.mouth_ids[std::size_t(ordinal)],ref,actual.period_ref,source.original_period.original_opening,actual.occurred_at,ordinal,actual.original_rule_ref};
});}
Outcome<std::vector<MomentOwnerSource>> read_exact_military_supply_opening_sources(const CommonStore& c,const MilitaryState& military,const MilitaryExactState& exact,const MilitaryCatalog& catalog,const MilitaryAccess& access,const ExactTrainingPeriodRead& periods,const MilitaryFormationSourceRead& formations,Integer from,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryReturnSourceRead* returns){return guard<std::vector<MomentOwnerSource>>([&]{
 const auto period=take(periods.read_training_period(from));if(period.from_tick!=from||period.to_tick<=from||period.to_tick-from!=1||period.original_opening.source_schema!=1||take(compare_world_moments(period.opening_at,now,budget))>0)fail("MilitarySupplyActualPeriodRequired",period.period_ref);
 if(returns&&!returns->bound_to(c,military))fail("CrossCandidateMilitaryReturnSources");
 if(!returns)for(const auto& row:c.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_logistics_completion_applied")fail("MilitarySupplyReturnOriginalReadRequired",row.first);
 const auto opening_cut=position(c,period.original_opening.source_ref);
 const auto serial=[&](const StableId& id){const auto colon=id.rfind(':');if(colon==StableId::npos)fail("MilitarySupplySourceSerialRequired",id);const auto text=id.substr(colon+1);Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||text!=std::to_string(n))fail("MilitarySupplySourceSerialRequired",id);return n;};
 const auto opening_serial=serial(period.original_opening.source_ref.id);std::set<StableId> expected,actual;
 for(const auto& camp:catalog.garrisons)if(camp.id.empty()||!expected.insert("garrison:"+camp.id).second)fail("MilitarySupplyGarrisonDefinitionMismatch",camp.id);
 for(const auto& army:military.armies){
  const auto formed=take(read_original_military_formation(c.state(),military,army,now,&formations,budget));if(formed.original_completion_serial>=opening_serial)continue;
  if(take(compare_world_moments(formed.completed_at,period.opening_at,budget))>0)fail("MilitarySupplyFormationCutMismatch",army.id);bool active=true;
  for(const auto& returned:military.return_facts)if(returned.army_id==army.id&&position(c,{"military","military_return",returned.id})<opening_cut)active=false;
  if(returns){const auto returned=take(returns->read_exact_army_return(army.id));if(returned){if(!returned->original||returned->actual_return.source_schema!=2||returned->actual_application.source_schema!=2)fail("MilitarySupplyReturnSourceRequired",army.id);const auto return_cut=position(c,returned->actual_return.source_ref);const auto app_cut=position(c,returned->actual_application.source_ref);if(return_cut>=app_cut)fail("MilitarySupplyReturnCutMismatch",army.id);if(return_cut<opening_cut){if(app_cut>=opening_cut||take(compare_world_moments(returned->original->completed_at,period.opening_at,budget))>0)fail("MilitarySupplyReturnCutMismatch",army.id);active=false;}}}
  for(const auto& integrated:military.integration_facts)for(const auto& source:integrated.source_armies)if(source.id==army.id&&position(c,{"military","military_integration",integrated.id})<opening_cut)active=false;
  if(army.status=="defeated"){
   if(!access.naval_defeat_source)fail("MilitarySupplyNavalDefeatSourceRequired",army.id);const auto defeat=access.naval_defeat_source(army.id);const auto* source=defeat?c.state().facts.find(*defeat):nullptr;if(!source||source->domain!="military_naval")fail("MilitarySupplyNavalDefeatSourceRequired",army.id);if(position(c,{source->domain,source->kind,source->id})<opening_cut)active=false;
  }
  if(active&&!expected.insert("army:"+army.id).second)fail("MilitarySupplyArmyDefinitionRepeated",army.id);
 }
 std::vector<MomentOwnerSource> out;
 for(const auto& row:military.supply_facts)if(row.tick==period.to_tick){const SourceRef ref{"military","military_supply",row.id};const auto source=take(read_exact_military_supply_source(c,military,exact,periods,ref,2,now,budget));if(source.exact_source->period_ref!=period.period_ref)fail("MilitarySupplyPeriodAlias",row.id);const auto key=row.army_id.empty()?"garrison:"+row.garrison_id:"army:"+row.army_id;if(!actual.insert(key).second)fail("DuplicateMilitarySupply",row.id);out.push_back({ref,2});}
 if(actual!=expected)fail("MilitarySupplyOpeningCoverageMismatch",period.period_ref);return out;
});}
Outcome<std::vector<MomentOwnerSource>> read_field_military_supply_opening_sources(const CommonStore& c,const MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryAccess& access,const ExactTrainingPeriodRead& periods,const MilitaryFormationSourceRead& formations,Integer from,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryReturnSourceRead* returns){return guard<std::vector<MomentOwnerSource>>([&]{
 const auto period=take(periods.read_training_period(from));if(period.from_tick!=from||period.to_tick<=from||period.to_tick-from!=1||period.original_opening.source_schema!=1||take(compare_world_moments(period.opening_at,now,budget))>0)fail("MilitarySupplyActualPeriodRequired",period.period_ref);
 if(returns&&!returns->bound_to(c,military))fail("CrossCandidateMilitaryReturnSources");
 if(!returns)for(const auto& row:c.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="military_logistics_completion_applied")fail("MilitarySupplyReturnOriginalReadRequired",row.first);
 const auto opening_cut=position(c,period.original_opening.source_ref);
 const auto serial=[&](const StableId& id){const auto colon=id.rfind(':');if(colon==StableId::npos)fail("MilitarySupplySourceSerialRequired",id);const auto text=id.substr(colon+1);Integer n=0;auto p=std::from_chars(text.data(),text.data()+text.size(),n);if(p.ec!=std::errc{}||p.ptr!=text.data()+text.size()||n<1||n>=c.state().next_instance_id||text!=std::to_string(n))fail("MilitarySupplySourceSerialRequired",id);return n;};
 const auto opening_serial=serial(period.original_opening.source_ref.id);std::set<StableId> expected,actual;
 for(const auto& camp:catalog.garrisons)if(camp.id.empty()||!expected.insert("garrison:"+camp.id).second)fail("MilitarySupplyGarrisonDefinitionMismatch",camp.id);
 for(const auto& army:military.armies){
  const auto formed=take(read_original_military_formation(c.state(),military,army,now,&formations,budget));if(formed.original_completion_serial>=opening_serial)continue;
  if(take(compare_world_moments(formed.completed_at,period.opening_at,budget))>0)fail("MilitarySupplyFormationCutMismatch",army.id);bool active=true;
  for(const auto& returned:military.return_facts)if(returned.army_id==army.id&&position(c,{"military","military_return",returned.id})<opening_cut)active=false;
  if(returns){const auto returned=take(returns->read_exact_army_return(army.id));if(returned){if(!returned->original||returned->actual_return.source_schema!=2||returned->actual_application.source_schema!=2)fail("MilitarySupplyReturnSourceRequired",army.id);const auto return_cut=position(c,returned->actual_return.source_ref);const auto app_cut=position(c,returned->actual_application.source_ref);if(return_cut>=app_cut)fail("MilitarySupplyReturnCutMismatch",army.id);if(return_cut<opening_cut){if(app_cut>=opening_cut||take(compare_world_moments(returned->original->completed_at,period.opening_at,budget))>0)fail("MilitarySupplyReturnCutMismatch",army.id);active=false;}}}
  for(const auto& integrated:military.integration_facts)for(const auto& source:integrated.source_armies)if(source.id==army.id&&position(c,{"military","military_integration",integrated.id})<opening_cut)active=false;
  if(army.status=="defeated"){
   if(!access.naval_defeat_source)fail("MilitarySupplyNavalDefeatSourceRequired",army.id);const auto defeat=access.naval_defeat_source(army.id);const auto* source=defeat?c.state().facts.find(*defeat):nullptr;if(!source||source->domain!="military_naval")fail("MilitarySupplyNavalDefeatSourceRequired",army.id);if(position(c,{source->domain,source->kind,source->id})<opening_cut)active=false;
  }
  if(active&&!expected.insert("army:"+army.id).second)fail("MilitarySupplyArmyDefinitionRepeated",army.id);
 }
 std::vector<MomentOwnerSource> out;
 for(const auto& row:military.supply_facts)if(row.tick==period.to_tick){const SourceRef ref{"military","military_supply",row.id};const auto source=take(read_field_military_supply_source(c,military,periods,ref,1,now,budget));if(source.original_period.period_ref!=period.period_ref)fail("MilitarySupplyPeriodAlias",row.id);const auto key=row.army_id.empty()?"garrison:"+row.garrison_id:"army:"+row.army_id;if(!actual.insert(key).second)fail("DuplicateMilitarySupply",row.id);out.push_back({ref,1});}
 if(actual!=expected)fail("MilitarySupplyOpeningCoverageMismatch",period.period_ref);return out;
});}
Outcome<std::optional<ExactMilitarySupplyMouthResponsibility>> read_exact_military_supply_mouth_responsibility(const CommonStore& c,const MilitaryState& military,const MilitaryExactState& exact,const MilitaryCatalog& catalog,const MilitaryAccess& access,const ExactTrainingPeriodRead& periods,const MilitaryFormationSourceRead& formations,Integer from,const StableId& mouth,const WorldMoment& now,ExactArithmeticBudget budget,const MilitaryReturnSourceRead* returns){return guard<std::optional<ExactMilitarySupplyMouthResponsibility>>([&]{
 if(mouth.empty())fail("MilitarySupplyMouthRequired");
 const auto complete=take(read_exact_military_supply_opening_sources(c,military,exact,catalog,access,periods,formations,from,now,budget,returns));
 std::optional<ExactMilitarySupplyMouthResponsibility> result;
 for(const auto& ref:complete){const auto source=take(read_exact_military_supply_source(c,military,exact,periods,ref.source_ref,ref.source_schema,now,budget));const auto& ids=source.original_supply->mouth_ids;const auto found=std::find(ids.begin(),ids.end(),mouth);if(found==ids.end())continue;if(result)fail("DuplicateMilitarySupplyMouthResponsibility",mouth);result=ExactMilitarySupplyMouthResponsibility{source,Integer(found-ids.begin())};}
 return result;
});}
}
