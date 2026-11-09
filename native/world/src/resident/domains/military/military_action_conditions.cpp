#include "military_action_conditions.hpp"
#include "military_action_original_rules.hpp"
#include "military_action_time_records.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/actor_foundation/character_condition_replay.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "training_exact_model.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
CheckpointMemoryCharge reserve(const MilitaryActionAcceptanceConditionsBuildView& v){for(const auto* head:v.participant_heads)if(!head)fail("MilitaryActionConditionHeadRequired");return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryActionAcceptanceConditions1& v){return CheckpointMemoryCharge(0);}
const MilitaryActionConditionContext& context(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access){
 const auto* x=access.action_conditions;
 if((access.military_action_conditions_schema!=1&&access.military_action_conditions_schema!=2)||!x||&x->common!=&c||&x->actions!=&state||&x->sources.common!=&c||&x->sources.state!=&x->foundation)fail("MilitaryActionConditionContextRequired");
 if(!same_moment(take(x->clock.read_current_moment()),x->sources.actual_now))fail("MilitaryActionConditionClockMismatch");return *x;
}
void selection_absent(const MilitaryAccess& access){if(access.military_action_conditions_schema!=0||access.action_conditions)fail("MilitaryActionConditionSelectedSchemaRequired");}
const StableId& participant_id(const StableId& id){return id;}
const StableId& participant_id(const StableId* id){if(!id)fail("MilitaryActionConditionParticipantRequired");return *id;}
template<class People>void unique(const People& ids){
 if(ids.empty())fail("MilitaryActionConditionParticipantsRequired");
 for(std::size_t i=0;i<ids.size();++i){if(participant_id(ids[i]).empty())fail("MilitaryActionConditionParticipantRequired");for(std::size_t j=0;j<i;++j)if(participant_id(ids[i])==participant_id(ids[j]))fail("MilitaryActionConditionParticipantDuplicate",participant_id(ids[i]));}
}
}
MilitaryActionAcceptanceConditions1::MilitaryActionAcceptanceConditions1(const MilitaryActionAcceptanceConditionsBuildView& v):copied_fields(reserve(v)){
 participant_heads.reserve(v.participant_heads.size());for(const auto* head:v.participant_heads){if(!head)fail("MilitaryActionConditionHeadRequired");participant_heads.emplace_back(*head);}
}
MilitaryActionAcceptanceConditions1::MilitaryActionAcceptanceConditions1(const MilitaryActionAcceptanceConditions1& v):copied_fields(reserve(v)),participant_heads(v.participant_heads){}
void MilitaryActionAcceptanceConditions1::swap(MilitaryActionAcceptanceConditions1& v)noexcept{copied_fields.swap(v.copied_fields);participant_heads.swap(v.participant_heads);}
// 捕获真实军旅参与人的条件头并核对完整人物伤势；来源变化或不一致沿原错误拒绝。
Outcome<std::optional<MilitaryActionAcceptanceConditions1>>
capture_military_action_acceptance_conditions(
    const CommonStore &c, const MilitaryActionTimeState &state,
    const MilitaryAccess &access, const ActionCoreView &action) {
  return guard<std::optional<MilitaryActionAcceptanceConditions1>>([&] {
    if (access.military_action_conditions_schema == 0) {
      selection_absent(access);
      return std::optional<MilitaryActionAcceptanceConditions1>{};
    }
    const auto &x = context(c, state, access);
    if (access.military_action_conditions_schema == 2 &&
        action.kind == "army_form")
      return std::optional<MilitaryActionAcceptanceConditions1>{};
    if (!access.actor_injury)
      fail("MilitaryActionConditionInjuryReadRequired", action.id);
    const auto *actual = c.action(action.id);
    if (!actual || actual != &action ||
        (action.kind != "army_form" && action.kind != "army_return"))
      fail("MilitaryActionConditionActualActionRequired", action.id);
    unique(action.participants);
    CheckpointVector<const ActorConditionHead *> heads;
    heads.reserve(action.participants.size());
    const auto revision = c.revision();
    for (const auto &actor : action.participants) {
      heads.push_back(take(read_verified_current_character_condition_head(
          x.sources, c, x.foundation, actor, x.clock)));
      const auto *person = c.state().actors.find(actor);
      if (!person || access.actor_injury(actor) != person->injury)
        fail("MilitaryActionConditionInjuryReadMismatch", actor);
    }
    if (c.revision() != revision || c.action(action.id) != actual)
      fail("MilitaryActionConditionCaptureChanged", action.id);
    for (std::size_t i = 0; i < heads.size(); ++i)
      if (x.foundation.character_condition_extensions.actor_condition_heads
              .find(action.participants[i]) != heads[i])
        fail("MilitaryActionConditionCaptureChanged", action.participants[i]);
    return std::optional<MilitaryActionAcceptanceConditions1>{
        std::in_place, MilitaryActionAcceptanceConditionsBuildView{heads}};
  });
}
namespace {
template<class Visit>void visit_verified_heads(const MilitaryActionConditionContext& x,const MilitaryActionAcceptanceConditions1& saved,const WorldMoment& at,Visit visit){
 if(&x.sources.common!=&x.common||&x.sources.state!=&x.foundation||!same_moment(take(x.clock.read_current_moment()),x.sources.actual_now))fail("MilitaryActionConditionContextRequired");
 if(saved.participant_heads.empty()||take(compare_world_moments(at,x.sources.actual_now))>0)fail("MilitaryActionConditionPrefixRequired");
 for(std::size_t i=0;i<saved.participant_heads.size();++i){const auto& head=saved.participant_heads[i];if(head.actor_id.empty())fail("MilitaryActionConditionParticipantRequired");for(std::size_t j=0;j<i;++j)if(saved.participant_heads[j].actor_id==head.actor_id)fail("MilitaryActionConditionParticipantDuplicate",head.actor_id);const auto actual=take(read_character_condition_verified_prefix(x.sources,head,at));visit(head,actual);}
}
}
Status validate_military_action_condition_prefix(const MilitaryActionConditionContext& x,const MilitaryActionAcceptanceConditions1& saved,const WorldMoment& at){return status([&]{visit_verified_heads(x,saved,at,[](const auto&,const auto&){});});}
namespace {
template<class People>Status validate_acceptance(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access,const StableId& action_id,const WorldMoment& accepted_at,const People& people,const std::optional<MilitaryActionAcceptanceConditions1>& saved){return status([&]{
 if(access.military_action_conditions_schema==0){selection_absent(access);if(saved)fail("UnexpectedMilitaryActionConditions",action_id);return;}
 const auto& x=context(c,state,access);
 if(access.military_action_conditions_schema==2){const auto* row=state.dynamic_action_facts.find(action_id);if(row&&row->command=="army_form"){
  if(saved||row->id!=action_id||!same_moment(row->started_at,accepted_at))fail("MilitaryActionEndpointOwnerRequired",action_id);unique(people);const auto* terms=std::get_if<ArmyFormationTerms>(&row->original_terms);if(!terms||terms->member_actor_ids.size()!=people.size())fail("MilitaryActionConditionParticipantsMismatch",action_id);for(std::size_t i=0;i<people.size();++i)if(terms->member_actor_ids[i]!=participant_id(people[i]))fail("MilitaryActionConditionParticipantsMismatch",action_id);
  if(!take(read_military_action_endpoint_body_ready(x,action_id,MilitaryActionConditionEndpoint2::acceptance,access.military_action_conditions_schema)))fail("MilitaryActionOriginalParticipantUnavailable",action_id);return;
 }}
 if(!saved)fail("MilitaryActionAcceptanceConditionsRequired",action_id);unique(people);
 take(read_actual_common_fact_position(c,SourceRefBuildView{"military","military_action",action_id}));
 if(take(compare_world_moments(accepted_at,x.sources.actual_now))>0||saved->participant_heads.size()!=people.size())fail("MilitaryActionConditionParticipantsMismatch",action_id);
 for(std::size_t i=0;i<people.size();++i){const auto& head=saved->participant_heads[i];if(head.actor_id!=participant_id(people[i]))fail("MilitaryActionConditionParticipantsMismatch",action_id);}
 auto memory=CheckpointMemoryCharge(0);const SourceRef boundary{"military","military_action",action_id};
 visit_verified_heads(x,*saved,accepted_at,[&](const ActorConditionHead& head,const CharacterConditionAtHead& body){
  const auto availability=take(read_character_availability_before_fact_provenance_v2(x.sources,head.actor_id,boundary,accepted_at,x.clock));
  if(!availability.present||availability.alive!=body.body.alive||!original_military_actor_life_ready(body.body.alive,availability.available,body.body.hp)||!original_military_actor_injury_ready(body.body.injury))fail("MilitaryActionOriginalParticipantUnavailable",head.actor_id);
 });
});}
}
Status validate_military_action_acceptance_conditions(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access,const StableId& action_id,const WorldMoment& at,const std::vector<StableId>& people,const std::optional<MilitaryActionAcceptanceConditions1>& saved){return validate_acceptance(c,state,access,action_id,at,people,saved);}
Status validate_military_action_acceptance_conditions(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access,const StableId& action_id,const WorldMoment& at,const CheckpointVector<const StableId*>& people,const std::optional<MilitaryActionAcceptanceConditions1>& saved){return validate_acceptance(c,state,access,action_id,at,people,saved);}
namespace {
bool same_endpoint_head(const ActorConditionHead& a,const ActorConditionHead& b){return a.actor_id==b.actor_id&&a.condition_revision==b.condition_revision&&a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key&&bool(a.latest_receipt_ref)==bool(b.latest_receipt_ref)&&(!a.latest_receipt_ref||a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);}
CheckpointMemoryCharge reserve_endpoint(const MilitaryActionEndpointObservationBuildView2& v){for(const auto* head:v.participant_heads)if(!head)fail("MilitaryActionConditionHeadRequired");return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve_endpoint(const MilitaryActionEndpointObservation2& v){return CheckpointMemoryCharge(0);}
CommonActualFactPrefix own_endpoint_prefix(const CurrentCommonFactPrefix& v){CommonActualFactPrefix result;result.common_fact_count=v.common_fact_count;if(v.last_common_fact)result.last_common_fact=SourceRef{v.last_common_fact->domain,v.last_common_fact->kind,v.last_common_fact->id};return result;}
void endpoint_kind(MilitaryActionConditionEndpoint2 endpoint){if(endpoint!=MilitaryActionConditionEndpoint2::acceptance&&endpoint!=MilitaryActionConditionEndpoint2::completion)fail("MilitaryActionConditionEndpointRequired");}
const MilitaryActionConditionContext& endpoint_context(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access){
 const auto* x=access.action_conditions;if(access.military_action_conditions_schema!=2||!x||&x->common!=&c||&x->actions!=&state||&x->sources.common!=&c||&x->sources.state!=&x->foundation)fail("MilitaryActionEndpointContextRequired");
 if(!same_moment(take(x->clock.read_current_moment()),x->sources.actual_now))fail("MilitaryActionConditionClockMismatch");return *x;
}
// Only the explicitly selected /2 entry points construct this synchronous
// reader. Old /0-/1 paths do not copy roots or gain new requirements.
class EndpointActualSourceRead final {
 CharacterConditionSources sources_;
 MilitaryActionConditionContext context_;
 static CharacterConditionSources selected(const CharacterConditionSources& original){
  auto result=original;result.military_history_order=MilitaryCasualtyHistoryOrder::actual_common_prefix;return result;
 }
public:
 explicit EndpointActualSourceRead(const MilitaryActionConditionContext& original)
  :sources_(selected(original.sources)),context_{original.common,original.actions,original.foundation,sources_,original.clock}{}
 EndpointActualSourceRead(const EndpointActualSourceRead&)=delete;
 EndpointActualSourceRead& operator=(const EndpointActualSourceRead&)=delete;
 EndpointActualSourceRead(EndpointActualSourceRead&&)=delete;
 EndpointActualSourceRead& operator=(EndpointActualSourceRead&&)=delete;
 const MilitaryActionConditionContext& context()const noexcept{return context_;}
};
void endpoint_prefix_current(const MilitaryActionConditionContext& x,const MilitaryActionEndpointObservation2& saved){
 take(read_actual_common_fact_prefix(x.common,saved.common_prefix));const auto common=take(read_current_common_fact_prefix(x.common));
 take(validate_character_condition_applied_prefix(x.sources,saved.condition_prefix));
 if(common.common_fact_count!=saved.common_prefix.common_fact_count||x.foundation.character_condition_extensions.applied_receipts.size()!=static_cast<std::size_t>(saved.condition_prefix.applied_receipt_count))fail("MilitaryActionConditionCaptureChanged");
 if(!same_moment(take(x.clock.read_current_moment()),x.sources.actual_now))fail("MilitaryActionConditionClockMismatch");
}
// 核验真实军旅端点的身体、来源前缀与完成归属；任一不符都保留原失败门。
void endpoint_current(const MilitaryActionConditionContext &x,
                      const MilitaryAccess &access,
                      const ActionCoreView &action,
                      MilitaryActionConditionEndpoint2 endpoint,
                      const MilitaryActionEndpointObservation2 &saved) {
  endpoint_kind(endpoint);
  if (!access.actor_injury || x.common.action(action.id) != &action ||
      (action.kind != "army_form" && action.kind != "office_accept"))
    fail("MilitaryActionConditionActualActionRequired", action.id);
  unique(action.participants);
  if (saved.participant_heads.size() != action.participants.size())
    fail("MilitaryActionConditionParticipantsMismatch", action.id);
  endpoint_prefix_current(x, saved);
  for (std::size_t i = 0; i < saved.participant_heads.size(); ++i) {
    const auto &head = saved.participant_heads[i];
    if (head.actor_id != action.participants[i])
      fail("MilitaryActionConditionParticipantsMismatch", action.id);
    const auto *current = take(read_verified_current_character_condition_head(
        x.sources, x.common, x.foundation, head.actor_id, x.clock));
    if (!current || !same_endpoint_head(*current, head))
      fail("MilitaryActionConditionCaptureChanged", head.actor_id);
    const auto *person = x.common.state().actors.find(head.actor_id);
    if (!person || access.actor_injury(head.actor_id) != person->injury)
      fail("MilitaryActionConditionInjuryReadMismatch", head.actor_id);
  }
  // 其他来源回调之后不再解引用先前借用的头记录。
  endpoint_prefix_current(x, saved);
  if (x.common.action(action.id) != &action)
    fail("MilitaryActionConditionCaptureChanged", action.id);
  for (const auto &head : saved.participant_heads) {
    const auto *current =
        x.foundation.character_condition_extensions.actor_condition_heads.find(
            head.actor_id);
    if (!current || !same_endpoint_head(*current, head))
      fail("MilitaryActionConditionCaptureChanged", head.actor_id);
  }
  if (endpoint == MilitaryActionConditionEndpoint2::completion) {
    const auto *row = x.actions.dynamic_action_facts.find(action.id);
    if (!row || row->id != action.id || row->command != action.kind ||
        row->actor_id != action.actor_id || row->phase != "started" ||
        row->completed_at || row->completion_observation ||
        !row->acceptance_observation || row->acceptance_conditions)
      fail("MilitaryActionCompletionObservationOwnerRequired", action.id);
  }
}
const DynamicMilitaryActionFact2& endpoint_row(const MilitaryActionConditionContext& x,const StableId& id){
 if(&x.sources.common!=&x.common||&x.sources.state!=&x.foundation||!same_moment(take(x.clock.read_current_moment()),x.sources.actual_now))fail("MilitaryActionEndpointContextRequired");
 const auto* row=x.actions.dynamic_action_facts.find(id);if(!row||row->id!=id||row->rule_version!=2||(row->command!="army_form"&&row->command!="office_accept")||(row->phase!="started"&&row->phase!="completed")||row->acceptance_conditions||!row->acceptance_observation||bool(row->completion_observation)!=(row->phase=="completed")||bool(row->completed_at)!=(row->phase=="completed"))fail("MilitaryActionEndpointOwnerRequired",id);return *row;
}
CheckpointVector<const StableId*> endpoint_people(const DynamicMilitaryActionFact2& row){
 CheckpointVector<const StableId*> result;if(row.command=="army_form"){const auto* t=std::get_if<ArmyFormationTerms>(&row.original_terms);if(!t||t->actor_id!=row.actor_id)fail("MilitaryActionEndpointTermsMismatch",row.id);result.reserve(t->member_actor_ids.size());for(const auto& actor:t->member_actor_ids)result.push_back(&actor);}
 else {const auto* t=std::get_if<OfficeTerms>(&row.original_terms);if(!t||t->actor_id!=row.actor_id)fail("MilitaryActionEndpointTermsMismatch",row.id);result.push_back(&t->actor_id);result.push_back(&t->provider_actor_id);}
 unique(result);return result;
}
template<class Visit>void endpoint_source(const MilitaryActionConditionContext& x,const StableId& id,MilitaryActionConditionEndpoint2 endpoint,Visit visit){
 endpoint_kind(endpoint);const auto& row=endpoint_row(x,id);const auto people=endpoint_people(row);
 const bool completing=endpoint==MilitaryActionConditionEndpoint2::completion;if(completing&&row.phase!="completed")fail("MilitaryActionCompletionObservationRequired",id);
 const auto& saved=completing?*row.completion_observation:*row.acceptance_observation;const auto& at=completing?*row.completed_at:row.started_at;
 if(take(compare_world_moments(at,x.sources.actual_now))>0||saved.participant_heads.size()!=people.size())fail("MilitaryActionConditionParticipantsMismatch",id);
 const auto start=SourceRefBuildView{"military","military_action",id};const auto start_position=take(read_actual_common_fact_position(x.common,start));
 CurrentCommonFactPrefix cut;
 if(completing){if(!row.terminal_source_ref)fail("MilitaryActionConditionTerminalRequired",id);const auto& source=*row.terminal_source_ref;
  const std::string_view kind=row.command=="office_accept"?"military_office_completion_applied":row.success?"military_action_completion_applied":"military_formation_completion_checked";
  if(source.domain!="military"||source.kind!=kind||source.id==id||take(read_actual_common_fact_position(x.common,source))<=start_position)fail("MilitaryActionConditionTerminalMismatch",id);
  for(const auto& item:x.actions.dynamic_action_facts.rows())if(item.first!=id&&item.second.terminal_source_ref&&item.second.terminal_source_ref->id==source.id)fail("MilitaryActionConditionTerminalDuplicate",source.id);
  for(const auto& item:x.actions.dynamic_logistics_facts.rows())if(item.second.terminal_source_ref&&item.second.terminal_source_ref->id==source.id)fail("MilitaryActionConditionTerminalDuplicate",source.id);
  cut=take(read_actual_common_fact_prefix_before(x.common,source));
  if(row.acceptance_observation->common_prefix.common_fact_count>saved.common_prefix.common_fact_count||row.acceptance_observation->condition_prefix.applied_receipt_count>saved.condition_prefix.applied_receipt_count)fail("MilitaryActionConditionObservationOrderMismatch",id);
 }else cut=take(read_actual_common_fact_prefix_before(x.common,start));
 // The selected producer checks before effects. No arbitrary intervening
 // source suffix is admitted as proof of that original observation.
 take(read_actual_common_fact_prefix(x.common,saved.common_prefix));if(saved.common_prefix.common_fact_count!=cut.common_fact_count)fail("MilitaryActionConditionPublicationPrefixMismatch",id);
 take(validate_character_condition_applied_prefix(x.sources,saved.condition_prefix));
 for(std::size_t i=0;i<people.size();++i){const auto& head=saved.participant_heads[i];if(head.actor_id!=*people[i])fail("MilitaryActionConditionParticipantsMismatch",id);const auto actual=take(read_character_condition_at_applied_prefix(x.sources,head.actor_id,saved.condition_prefix,at));if(!same_endpoint_head(actual.head,head))fail("MilitaryActionConditionMaximalHeadMismatch",head.actor_id);visit(row,head,actual,at);}
}
}
MilitaryActionEndpointObservation2::MilitaryActionEndpointObservation2(const MilitaryActionEndpointObservationBuildView2& v):copied_fields(reserve_endpoint(v)),common_prefix(own_endpoint_prefix(v.common_prefix)),condition_prefix(v.condition_prefix){participant_heads.reserve(v.participant_heads.size());for(const auto* head:v.participant_heads){if(!head)fail("MilitaryActionConditionHeadRequired");participant_heads.emplace_back(*head);}}
MilitaryActionEndpointObservation2::MilitaryActionEndpointObservation2(const MilitaryActionEndpointObservation2& v):copied_fields(reserve_endpoint(v)),common_prefix(v.common_prefix),condition_prefix(v.condition_prefix),participant_heads(v.participant_heads){}
void MilitaryActionEndpointObservation2::swap(MilitaryActionEndpointObservation2& v)noexcept{copied_fields.swap(v.copied_fields);std::swap(common_prefix.common_fact_count,v.common_prefix.common_fact_count);common_prefix.last_common_fact.swap(v.common_prefix.last_common_fact);std::swap(condition_prefix.applied_receipt_count,v.condition_prefix.applied_receipt_count);condition_prefix.last_applied_receipt.swap(v.condition_prefix.last_applied_receipt);participant_heads.swap(v.participant_heads);}
Outcome<std::optional<MilitaryActionEndpointObservation2>> capture_military_action_endpoint_observation(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access,const ActionCoreView& action,MilitaryActionConditionEndpoint2 endpoint){return guard<std::optional<MilitaryActionEndpointObservation2>>([&]{
 endpoint_kind(endpoint);if(access.military_action_conditions_schema==0||access.military_action_conditions_schema==1)return std::optional<MilitaryActionEndpointObservation2>{};
 const EndpointActualSourceRead actual(endpoint_context(c,state,access));const auto& x=actual.context();if(c.action(action.id)!=&action||(action.kind!="office_accept"&&action.kind!="army_form"))fail("MilitaryActionConditionActualActionRequired",action.id);unique(action.participants);
 // Capture's raw receipt-prefix copy has its own enclosing lifetime owner;
 // it is not paid by a charge destroyed before the returned observation.
 const auto& receipts=x.foundation.character_condition_extensions.applied_receipts.rows();auto prefix_memory=CheckpointMemoryCharge(0);
 const auto condition_prefix=take(capture_character_condition_applied_prefix(x.sources));const auto common_prefix=take(read_current_common_fact_prefix(c));
 CheckpointVector<const ActorConditionHead*> heads;heads.reserve(action.participants.size());for(const auto& actor:action.participants){const auto* head=x.foundation.character_condition_extensions.actor_condition_heads.find(actor);if(!head)fail("MilitaryActionConditionHeadRequired",actor);heads.push_back(head);}
 // Copy all borrowed anchors and heads before any external condition source
 // callback. The returned object owns its own, separately planned payload.
 std::optional<MilitaryActionEndpointObservation2> result{std::in_place,MilitaryActionEndpointObservationBuildView2{common_prefix,condition_prefix,heads}};
 endpoint_current(x,access,action,endpoint,*result);return result;
});}
Status validate_military_action_endpoint_observation_unchanged(const CommonStore& c,const MilitaryActionTimeState& state,const MilitaryAccess& access,const ActionCoreView& action,MilitaryActionConditionEndpoint2 endpoint,const MilitaryActionEndpointObservation2& saved){return status([&]{const EndpointActualSourceRead actual(endpoint_context(c,state,access));endpoint_current(actual.context(),access,action,endpoint,saved);});}
Status MilitaryActionCompletionConditionGuard::validate_before_effects(const CommonWrite& c,const MilitaryAccess& access,const ActionCoreView& action)const{
 if(&c!=static_cast<const CommonWrite*>(&common_)||&access!=&access_||&action!=&action_)return common_failure("MilitaryActionCompletionConditionGuardMismatch");
 return validate_military_action_endpoint_observation_unchanged(common_,actions_,access_,action_,MilitaryActionConditionEndpoint2::completion,observation_);
}
Status validate_military_action_endpoint_observation_source(const MilitaryActionConditionContext& x,const StableId& id,MilitaryActionConditionEndpoint2 endpoint,Integer selected){return status([&]{if(selected!=2)fail("MilitaryActionConditionSelectedSchemaRequired");endpoint_kind(endpoint);(void)endpoint_row(x,id);const EndpointActualSourceRead actual(x);endpoint_source(actual.context(),id,endpoint,[](const auto&,const auto&,const auto&,const auto&){});});}
namespace {
Outcome<bool> endpoint_body_ready(const MilitaryActionConditionContext& original,const StableId& id,MilitaryActionConditionEndpoint2 endpoint,Integer selected,const StableId* participant){return guard<bool>([&]{if(selected!=2)fail("MilitaryActionConditionSelectedSchemaRequired");endpoint_kind(endpoint);(void)endpoint_row(original,id);const EndpointActualSourceRead actual(original);const auto& x=actual.context();bool ready=true,found=false;endpoint_source(x,id,endpoint,[&](const DynamicMilitaryActionFact2& row,const ActorConditionHead& head,const CharacterConditionAtHead& body,const WorldMoment& at){
 if(participant&&head.actor_id!=*participant)return;found=true;
 const bool completing=endpoint==MilitaryActionConditionEndpoint2::completion;const auto boundary=completing?SourceRefBuildView{row.terminal_source_ref->domain,row.terminal_source_ref->kind,row.terminal_source_ref->id}:SourceRefBuildView{"military","military_action",row.id};
 auto memory=CheckpointMemoryCharge(0);const SourceRef source{StableId(boundary.domain),StableId(boundary.kind),StableId(boundary.id)};
 const auto available=take(read_character_availability_before_fact_provenance_v2(x.sources,head.actor_id,source,at,x.clock));if(!available.present||available.alive!=body.body.alive)fail("MilitaryActionConditionAvailabilityMismatch",head.actor_id);
 const bool health=row.command=="army_form"||completing||head.actor_id==row.actor_id;
 if(health){if(!original_military_actor_life_ready(body.body.alive,available.available,body.body.hp)||!original_military_actor_injury_ready(body.body.injury))ready=false;}else if(!body.body.alive||!available.available)ready=false;
 });if(participant&&!found)fail("MilitaryActionConditionParticipantRequired",*participant);return ready;});}
}
Outcome<bool> read_military_action_endpoint_body_ready(const MilitaryActionConditionContext& x,const StableId& id,MilitaryActionConditionEndpoint2 endpoint,Integer selected){return endpoint_body_ready(x,id,endpoint,selected,nullptr);}
Outcome<bool> read_military_action_endpoint_participant_body_ready(const MilitaryActionConditionContext& x,const StableId& id,MilitaryActionConditionEndpoint2 endpoint,const StableId& participant,Integer selected){return endpoint_body_ready(x,id,endpoint,selected,&participant);}
}
