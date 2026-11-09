// Common时间投影与真实owner逐项反验。等待0不是完成；未知分类不回退成默认一半日。
#include "action_time_validation.hpp"
#include "hanmo/resident_world_api.hpp"
#include "fixed_service_history.hpp"
#include "world_copy_plan.hpp"
#include <type_traits>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <stdexcept>
namespace hanmo::resident {
namespace {
CheckpointMemoryCharge remaining_memory(const ActualServiceRemainingBuildView& value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge remaining_memory(const ActualAuthorizationRemainingBuildView& value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge proof_memory(const ActionTimeOwnerProofBuildView& value){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge proof_memory(const ActionTimeOwnerProof& value){return CheckpointMemoryCharge(0);}
SourceRef proof_source(const SourceRefBuildView& value){return {StableId(value.domain),StableId(value.kind),StableId(value.id)};}
ElapsedTimeSource proof_time(const ElapsedTimeSourceBuildView& value){return std::visit([](const auto& source)->ElapsedTimeSource{using T=std::decay_t<decltype(source)>;if constexpr(std::is_same_v<T,MomentOwnerSourceBuildView>)return MomentOwnerSource{proof_source(source.source_ref),source.source_schema};else return OriginalPeriodSource{proof_source(source.source_ref),source.source_schema,source.processed_tick};},value);}
std::vector<StableId> proof_strings(const CheckpointStringVectorBuildView& value){std::vector<StableId> out;out.reserve(value.values.size());for(const auto text:value.values)out.emplace_back(text);return out;}
std::optional<ElapsedTimeSource> proof_optional_time(const std::optional<ElapsedTimeSourceBuildView>& value){if(value)return proof_time(*value);return {};}
}
ActualServiceRemaining::ActualServiceRemaining(const ActualServiceRemainingBuildView& value):copied_fields(remaining_memory(value)),remaining(value.remaining),actual_basis(proof_source(value.actual_basis)),source_schema(value.source_schema){}
ActualServiceRemaining::ActualServiceRemaining(const ActualServiceRemaining& value):ActualServiceRemaining(ActualServiceRemainingBuildView{value.remaining,{value.actual_basis.domain,value.actual_basis.kind,value.actual_basis.id},value.source_schema}){}
void ActualServiceRemaining::swap(ActualServiceRemaining& value)noexcept{remaining.value.swap(value.remaining.value);actual_basis.domain.swap(value.actual_basis.domain);actual_basis.kind.swap(value.actual_basis.kind);actual_basis.id.swap(value.actual_basis.id);std::swap(source_schema,value.source_schema);copied_fields.swap(value.copied_fields);}
ActualAuthorizationRemaining::ActualAuthorizationRemaining(const ActualAuthorizationRemainingBuildView& value):copied_fields(remaining_memory(value)),until_tick(value.until_tick),original_terms_source(proof_source(value.original_terms_source)),source_schema(value.source_schema){}
ActualAuthorizationRemaining::ActualAuthorizationRemaining(const ActualAuthorizationRemaining& value):ActualAuthorizationRemaining(ActualAuthorizationRemainingBuildView{value.until_tick,{value.original_terms_source.domain,value.original_terms_source.kind,value.original_terms_source.id},value.source_schema}){}
void ActualAuthorizationRemaining::swap(ActualAuthorizationRemaining& value)noexcept{std::swap(until_tick,value.until_tick);original_terms_source.domain.swap(value.original_terms_source.domain);original_terms_source.kind.swap(value.original_terms_source.kind);original_terms_source.id.swap(value.original_terms_source.id);std::swap(source_schema,value.source_schema);copied_fields.swap(value.copied_fields);}
ActionTimeOwnerProof::ActionTimeOwnerProof(const ActionTimeOwnerProofBuildView& value)
 :copied_fields(proof_memory(value)),action_ref(value.action_ref),action_kind(value.action_kind),actor_id(value.actor_id),participant_actor_ids(proof_strings(value.participant_actor_ids)),original_execution_source(proof_source(value.original_execution_source)),source_schema(value.source_schema),started_source(proof_time(value.started_source)),phase(value.phase),retained_wait_source(value.retained_wait_source?std::optional<SourceRef>{proof_source(*value.retained_wait_source)}:std::nullopt),ended_source(proof_optional_time(value.ended_source)),terminal_result_ref(value.terminal_result_ref?std::optional<StableId>{StableId(*value.terminal_result_ref)}:std::nullopt),retained_gate_result_refs(proof_strings(value.retained_gate_result_refs)),suspended_source(proof_optional_time(value.suspended_source)){}
ActionTimeOwnerProof::ActionTimeOwnerProof(const ActionTimeOwnerProof& value)
 :copied_fields(proof_memory(value)),action_ref(value.action_ref),action_kind(value.action_kind),actor_id(value.actor_id),participant_actor_ids(value.participant_actor_ids),original_execution_source(value.original_execution_source),source_schema(value.source_schema),started_source(value.started_source),phase(value.phase),retained_wait_source(value.retained_wait_source),ended_source(value.ended_source),terminal_result_ref(value.terminal_result_ref),retained_gate_result_refs(value.retained_gate_result_refs),suspended_source(value.suspended_source){}
ActionTimeOwnerProof::ActionTimeOwnerProof(ActionTimeOwnerProof&& value):ActionTimeOwnerProof(static_cast<const ActionTimeOwnerProof&>(value)){}
void ActionTimeOwnerProof::swap(ActionTimeOwnerProof& value)noexcept{
 static_assert(std::is_nothrow_swappable_v<SourceRef>&&std::is_nothrow_swappable_v<ElapsedTimeSource>&&std::is_nothrow_swappable_v<std::optional<ElapsedTimeSource>>);
 using std::swap;swap(action_ref,value.action_ref);swap(action_kind,value.action_kind);swap(actor_id,value.actor_id);swap(participant_actor_ids,value.participant_actor_ids);swap(original_execution_source,value.original_execution_source);swap(source_schema,value.source_schema);swap(started_source,value.started_source);swap(phase,value.phase);swap(retained_wait_source,value.retained_wait_source);swap(ended_source,value.ended_source);swap(terminal_result_ref,value.terminal_result_ref);swap(retained_gate_result_refs,value.retained_gate_result_refs);swap(suspended_source,value.suspended_source);copied_fields.swap(value.copied_fields);
}
namespace {
struct Failure{RuleError error;};
using BorrowedActionIdSet=std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>>;
template<class F>class OriginalOwnerSink final:public ActionTimeOwnerIdSink {F& append_;public:explicit OriginalOwnerSink(F& fn):append_(fn){}Status append(std::string_view id)override{return append_(id);}};

void fail(const char* code,ErrorKind kind=ErrorKind::rule){RuleError e;e.original={code,"action_time"};e.kind=kind;throw Failure{std::move(e)};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
template<class T,class F>Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"ActionTimeResourceExhausted","action_time"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"ActionTimeResourceExhausted","action_time"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
const ActionTimeRegistration& registration(const std::vector<ActionTimeRegistration>& rows,const StableId& kind){const ActionTimeRegistration* out=nullptr;for(const auto& row:rows)if(row.action_kind==kind){if(out)fail("DuplicateActionTimeRegistration");out=&row;}if(!out)fail("MissingActionTimeRegistration",ErrorKind::unsupported_capability);return *out;}
const ActionElapsedStore::Entry* entry(const ActionElapsedStore& state,const StableId& id){const ActionElapsedStore::Entry* out=nullptr;for(const auto& row:state.entries)if(action_elapsed_entry_ref(row)==id){if(out)fail("DuplicateActionElapsedEntry");out=&row;}return out;}
const ActionElapsedTerminalBinding* terminal(const ActionElapsedStore& state,const StableId& id){const ActionElapsedTerminalBinding* out=nullptr;for(const auto& row:state.terminal_bindings)if(row.action_ref==id){if(out)fail("DuplicateActionTerminalBinding");out=&row;}return out;}
void ids(const std::vector<StableId>& values){BorrowedActionIdSet unique;for(const auto& id:values)if(id.empty()||!unique.insert(id).second)fail("DuplicateActionTimeReference");}
bool same_time_source(const ElapsedTimeSource& a,const ElapsedTimeSource& z){if(a.index()!=z.index())return false;return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(z);if(x.source_schema!=y.source_schema||!same_source_ref(x.source_ref,y.source_ref))return false;if constexpr(std::is_same_v<T,OriginalPeriodSource>)return x.processed_tick==y.processed_tick;return true;},a);}
WorldMoment read_time(const ElapsedTimeSource& ref,const ActionTimeOriginalSourceRead& reader,ExactArithmeticBudget b){std::visit([&](const auto& v){if(!valid_source_ref(v.source_ref)||v.source_schema<=0)fail("InvalidActionTimeSource");},ref);auto at=take(reader.read_action_time_source(ref));take(encode_world_moment(at,b));if(const auto* original=std::get_if<OriginalPeriodSource>(&ref))if(original->processed_tick<0||at.half_day_tick!=original->processed_tick||take(compare_fractions(at.sub_half_day,CanonicalFraction{},b))!=0)fail("OriginalActionTimeBoundaryMismatch");return at;}
void owner(const ActionTimeRegistration& reg,const ActionTimeOwnerProof& v){if(v.action_ref.empty()||v.action_kind!=reg.action_kind||v.actor_id.empty()||!valid_source_ref(v.original_execution_source)||v.original_execution_source.domain!=reg.owner_domain||v.source_schema!=reg.source_schema||static_cast<unsigned>(v.phase)>static_cast<unsigned>(ActionTimeOwnerPhase::terminal))fail("InvalidActionTimeOwner");ids(v.participant_actor_ids);ids(v.retained_gate_result_refs);if(reg.mode!=ActionTimeMode::shared_work&&!v.retained_gate_result_refs.empty())fail("NonWorkActionHasWorkGates");if(v.suspended_source&&(reg.mode!=ActionTimeMode::service_window||v.phase!=ActionTimeOwnerPhase::suspended))fail("UnexpectedAuthorizationSuspensionSource");if(reg.mode==ActionTimeMode::service_window&&v.phase==ActionTimeOwnerPhase::suspended&&!v.suspended_source)fail("AuthorizationSuspensionSourceRequired");if(v.phase==ActionTimeOwnerPhase::terminal){if(!v.ended_source||!v.terminal_result_ref||v.terminal_result_ref->empty()||v.retained_wait_source)fail("InvalidActionTimeTerminalOwner");}else if(v.ended_source||v.terminal_result_ref)fail("ActiveActionHasTerminalSource");const bool wait=v.phase==ActionTimeOwnerPhase::awaiting_process_choice||v.phase==ActionTimeOwnerPhase::awaiting_output_space||v.phase==ActionTimeOwnerPhase::awaiting_settlement;if(wait){if(!v.retained_wait_source||!valid_source_ref(*v.retained_wait_source)||std::find(reg.retained_wait_phases.begin(),reg.retained_wait_phases.end(),v.phase)==reg.retained_wait_phases.end())fail("UnprovenRetainedActionWait");}else if(v.retained_wait_source)fail("UnexpectedActionWaitSource");}
WorkExecutionBinding result_binding(const SharedWorkGateResult& v,const ActionTimeOriginalSourceRead& source,Integer n,ExactArithmeticBudget b){return std::visit([&](const auto& x)->WorkExecutionBinding{using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,LifeHalfDayStageGateResult>){const auto* reader=source.life_work_period_sources(x.work_period_ref);if(!reader)fail("MissingOriginalLifeWorkPeriodReader",ErrorKind::unsupported_capability);return take(read_verified_life_work_period(x.work_period_ref,*reader,n,b)).execution_binding;}else return x.execution_binding;},v);}
Integer ordinal(const SharedWorkGateResult& v){return std::visit([](const auto& x)->Integer{using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,LifeHalfDayStageGateResult>)return x.completed_halfday_ordinal;else return x.gate_ordinal;},v);}
ActionTimeOwnerPhase work_phase(WorkExecutionPhase phase){switch(phase){case WorkExecutionPhase::working:return ActionTimeOwnerPhase::working;case WorkExecutionPhase::suspended:return ActionTimeOwnerPhase::suspended;case WorkExecutionPhase::awaiting_process_choice:return ActionTimeOwnerPhase::awaiting_process_choice;case WorkExecutionPhase::awaiting_output_space:return ActionTimeOwnerPhase::awaiting_output_space;case WorkExecutionPhase::terminal:return ActionTimeOwnerPhase::terminal;case WorkExecutionPhase::awaiting_settlement:return ActionTimeOwnerPhase::awaiting_settlement;}fail("UnknownWorkPhase");return ActionTimeOwnerPhase::terminal;}
void binding_owner(const WorkExecutionBinding& binding,const ActionTimeOwnerProof& proof){take(validate_work_binding(binding));const auto* actual=std::get_if<ActorActionWorkBinding>(&binding);if(!actual||actual->action_id!=proof.action_ref||actual->action_kind!=proof.action_kind||!same_source_ref(actual->source_owner_ref,proof.original_execution_source))fail("SharedActionBindingMismatch");}
Integer shared_remaining(const SharedWorkElapsedState& s,const ActionTimeOwnerProof& proof,const TimeScaleRef& scale,ExactArithmeticBudget b){take(validate_work_execution_state(s.execution));if(work_phase(s.execution.phase)!=proof.phase)fail("SharedOwnerPhaseMismatch");if(const auto* craft=std::get_if<CraftWorkGate>(&s.gate)){binding_owner(craft->execution_binding,proof);if(proof.phase==ActionTimeOwnerPhase::awaiting_settlement||proof.phase==ActionTimeOwnerPhase::terminal)return 0;return take(craft_remaining_half_days(s.execution,*craft,scale.units_per_half_day,b));}if(const auto* original=std::get_if<OriginalHalfDayGate>(&s.gate)){binding_owner(original->original_execution_binding,proof);if(original->next_period_ordinal<0||static_cast<unsigned>(original->original_gate_profile_ref)>4||s.execution.total_work_units%scale.units_per_half_day||s.execution.completed_work_units%scale.units_per_half_day)fail("InvalidOriginalWorkProjection");if(original->last_consumed_period_ref.has_value()!=s.execution.last_processed_period.has_value()||(original->last_consumed_period_ref&&!same_work_period_ref(*original->last_consumed_period_ref,*s.execution.last_processed_period)))fail("OriginalWorkPeriodBindingMismatch");return (s.execution.total_work_units-s.execution.completed_work_units)/scale.units_per_half_day;}return take(life_half_day_stage_remaining(s.execution,std::get<LifeHalfDayStageGateState>(s.gate),scale.units_per_half_day,b));}
void compare_action(const ActionTimeRegistration& reg,const ActionCoreView& a,const ActionTimeOwnerProof& proof,const ActionTimeProjection& projected){
    // Original F suspension releases live Common occupancy while the owner proof
    // retains execution identities. The FSL/timing source was checked above.
    const bool released_fixed=reg.mode==ActionTimeMode::fixed_duration&&proof.phase==ActionTimeOwnerPhase::suspended;
    const bool participants_match=released_fixed?a.participants.empty():a.participants==proof.participant_actor_ids;
    if(a.id!=proof.action_ref||a.kind!=proof.action_kind||a.actor_id!=proof.actor_id||!participants_match||a.start_tick!=projected.started_at.half_day_tick||a.remaining_ticks!=projected.remaining_half_days||proof.phase==ActionTimeOwnerPhase::terminal)fail("ActionTimeProjectionMismatch");
}
}
Outcome<ActionTimeOwnerProof> ActionTimeOriginalSourceRead::read_action_time_owner_borrowed(const ActionTimeRegistration& reg,std::string_view action)const{return guard<ActionTimeOwnerProof>([&]{auto copied_id=CheckpointMemoryCharge(0);const StableId id(action);return take(read_action_time_owner(reg,id));});}
Outcome<ExhaustedAuthorizationWindow> ActionTimeOriginalSourceRead::read_exhausted_authorization_window(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const{return guard<ExhaustedAuthorizationWindow>([]{fail("OriginalAuthorizationWindowCloseReaderRequired",ErrorKind::unsupported_capability);return ExhaustedAuthorizationWindow{};});}
namespace {
using RegistrationKindSet=BorrowedActionIdSet;
void validate_registration_row(const ActionTimeRegistration& r,RegistrationKindSet* kinds){if(r.action_kind.empty()||r.owner_domain.empty()||r.profile_id.empty()||r.profile_version<=0||r.source_schema<=0||(kinds&&!kinds->insert(r.action_kind).second)||static_cast<unsigned>(r.mode)>4||static_cast<unsigned>(r.remaining_meaning)>2)fail("InvalidActionTimeRegistration");if((r.mode==ActionTimeMode::shared_work)!=r.shared_gate_kind.has_value()||(r.shared_gate_kind&&*r.shared_gate_kind>2))fail("InvalidSharedGateRegistration");if(r.mode!=ActionTimeMode::shared_work&&!r.retained_wait_phases.empty())fail("NonWorkActionRetainedWaitRegistration");// Fixed S3 TI11 / section2.2 gives this execution one original F clock.
// Required target periods and authorization/deadline windows are not its H.
const bool supervision=r.owner_domain=="tasks"&&(r.action_kind=="agronomy_supervision"||r.action_kind=="industry_supervision"||r.action_kind=="construction_supervision");if(supervision&&(r.mode!=ActionTimeMode::fixed_duration||r.remaining_meaning!=ActionRemainingMeaning::service_estimate))fail("SupervisionOriginalFixedExecutionTermsRequired",ErrorKind::unsupported_capability);if(r.remaining_meaning==ActionRemainingMeaning::authorization_window){const bool military=r.owner_domain=="military"&&r.action_kind=="military_station"&&r.profile_id=="military_duty_presence"&&r.profile_version==2&&r.source_schema==1;if(r.mode!=ActionTimeMode::service_window||!military)fail("UnregisteredAuthorizationWindowKind");}if((r.mode==ActionTimeMode::original_boundary)!=(r.remaining_meaning==ActionRemainingMeaning::original_boundary_periods))fail("OriginalBoundaryMeaningMismatch");std::set<ActionTimeOwnerPhase,std::less<ActionTimeOwnerPhase>,CheckpointAllocator<ActionTimeOwnerPhase>> unique;for(auto phase:r.retained_wait_phases)if((phase!=ActionTimeOwnerPhase::awaiting_process_choice&&phase!=ActionTimeOwnerPhase::awaiting_output_space&&phase!=ActionTimeOwnerPhase::awaiting_settlement)||!unique.insert(phase).second)fail("InvalidActionWaitRegistration");}
}
Status validate_action_time_registration(const ActionTimeRegistration& registration){return guard<Done>([&]{validate_registration_row(registration,nullptr);return Done{};});}
Status validate_action_time_registry(const std::vector<ActionTimeRegistration>& registrations){return guard<Done>([&]{RegistrationKindSet kinds;for(const auto& row:registrations)validate_registration_row(row,&kinds);return Done{};});}
Status validate_action_time_owner_proof(const ActionTimeRegistration& reg,const ActionTimeOwnerProof& proof){return guard<Done>([&]{owner(reg,proof);return Done{};});}
Outcome<ActionTimeProjection> read_action_time_projection(
    const ActionTimeRegistration &reg, const ActionTimeOwnerProof &proof,
    const ActionElapsedStore &elapsed, const WorldMoment &current,
    const TimeScaleRef &scale, const ActionTimeOriginalSourceRead &source,
    ExactArithmeticBudget b) {
  return guard<ActionTimeProjection>([&] {
    take(validate_time_scale(scale));
    owner(reg, proof);
    ActionTimeProjection projected{read_time(proof.started_source, source, b),
                                   0, reg.remaining_meaning};
    if (take(compare_world_moments(projected.started_at, current, b)) > 0)
      fail("ActionStartedInFuture");
    const auto *active = entry(elapsed, proof.action_ref);
    if (reg.mode == ActionTimeMode::fixed_duration) {
      if (!active || active->index() != 0)
        fail("MissingFixedActionElapsed");
      const auto &fixed = std::get<ActionElapsedState>(*active);
      take(validate_fixed_elapsed_value(fixed, current, scale, b));
      ActionElapsedStoreBuildView retained_view;
      retained_view.fixed_service_transitions.reserve(
          elapsed.fixed_service_transitions.size());
      for (const auto &transition : elapsed.fixed_service_transitions)
        if (transition.action_ref == proof.action_ref)
          retained_view.fixed_service_transitions.push_back(&transition);
      ActionElapsedStore retained(retained_view);
      take(source.validate_fixed_action_sources_in_candidate(
          reg, proof, fixed, retained.fixed_service_transitions, elapsed));
      if (take(compare_world_moments(projected.started_at, fixed.started_at,
                                     b)) != 0)
        fail("FixedActionStartSourceMismatch");
      if ((proof.phase == ActionTimeOwnerPhase::working) !=
              std::holds_alternative<ElapsedRunning>(fixed.timing) ||
          (proof.phase == ActionTimeOwnerPhase::suspended) !=
              std::holds_alternative<ElapsedSuspended>(fixed.timing) ||
          (proof.phase == ActionTimeOwnerPhase::terminal) !=
              std::holds_alternative<ElapsedTerminal>(fixed.timing))
        fail("FixedActionOwnerPhaseMismatch");
      projected.remaining_half_days =
          take(fixed_elapsed_remaining_ticks(fixed, scale, b));
    } else if (reg.mode == ActionTimeMode::shared_work) {
      if (!active || active->index() != 1)
        fail("MissingSharedWorkElapsed");
      const auto &shared = std::get<SharedWorkElapsedState>(*active);
      if (!reg.shared_gate_kind || *reg.shared_gate_kind != shared.gate.index())
        fail("WrongSharedWorkGateKind");
      take(source.validate_shared_action_sources(reg, proof, shared));
      projected.remaining_half_days = shared_remaining(shared, proof, scale, b);
      if (shared.gate.index() == 1 &&
          take(compare_fractions(projected.started_at.sub_half_day,
                                 CanonicalFraction{}, b)) != 0)
        fail("OriginalHalfDayWorkFractionStart");
      if (projected.remaining_half_days == 0 &&
          proof.phase != ActionTimeOwnerPhase::terminal) {
        // A real Craft cancellation waits only for original material
        // settlement. Its source validator above proves that no further
        // service/gate was earned. Preserve its actual incomplete work instead
        // of forging full N.
        const bool craft_settlement =
            std::holds_alternative<CraftWorkGate>(shared.gate) &&
            proof.phase == ActionTimeOwnerPhase::awaiting_settlement;
        if (!proof.retained_wait_source ||
            (!craft_settlement && shared.execution.completed_work_units !=
                                      shared.execution.total_work_units))
          fail("UnprovenZeroWorkAction");
      }
    } else {
      if (active)
        fail("DuplicateOriginalActionClock");
      auto remaining =
          take(source.read_original_action_remaining(reg, proof, current));
      if (auto *value = std::get_if<ActualServiceRemaining>(&remaining)) {
        if (reg.remaining_meaning != ActionRemainingMeaning::service_estimate ||
            !valid_source_ref(value->actual_basis) || value->source_schema <= 0)
          fail("InvalidServiceRemainingSource");
        projected.remaining_half_days =
            take(ceil_half_day_duration(value->remaining, b));
      } else if (auto *value =
                     std::get_if<ActualAuthorizationRemaining>(&remaining)) {
        if (reg.remaining_meaning !=
                ActionRemainingMeaning::authorization_window ||
            !valid_source_ref(value->original_terms_source) ||
            value->source_schema <= 0 || value->until_tick < 0)
          fail("InvalidAuthorizationRemainingSource");
        WorldMoment end{value->until_tick, {}};
        projected.remaining_half_days =
            take(compare_world_moments(current, end, b)) >= 0
                ? 0
                : take(ceil_half_day_duration(
                      take(elapsed_world_duration(current, end, b)), b));
      } else {
        const auto &boundary_value =
            std::get<ActualBoundaryRemaining>(remaining);
        if (reg.remaining_meaning !=
                ActionRemainingMeaning::original_boundary_periods ||
            boundary_value.unprovided_periods < 0 ||
            !valid_source_ref(boundary_value.original_period_source) ||
            boundary_value.source_schema <= 0 ||
            take(compare_fractions(projected.started_at.sub_half_day,
                                   CanonicalFraction{}, b)) != 0)
          fail("InvalidOriginalBoundaryRemaining");
        projected.remaining_half_days = boundary_value.unprovided_periods;
      }
    }
    // M/V/原边界没有S0
    // elapsed；它们也必须反读真实终局时刻，不能靠phase标签放行未来结果。
    if (proof.phase == ActionTimeOwnerPhase::terminal) {
      const auto ended_at = read_time(*proof.ended_source, source, b);
      if (take(compare_world_moments(ended_at, projected.started_at, b)) < 0 ||
          take(compare_world_moments(ended_at, current, b)) > 0)
        fail("ActionTerminalTimeOrder");
    }
    if (reg.mode == ActionTimeMode::service_window &&
        proof.phase == ActionTimeOwnerPhase::suspended) {
      const auto paused_at = read_time(*proof.suspended_source, source, b);
      if (take(compare_world_moments(paused_at, projected.started_at, b)) < 0 ||
          take(compare_world_moments(paused_at, current, b)) > 0)
        fail("AuthorizationSuspensionTimeOrder");
    }
    if (proof.phase != ActionTimeOwnerPhase::terminal &&
        reg.mode != ActionTimeMode::shared_work &&
        projected.remaining_half_days <= 0) {
      if (reg.mode != ActionTimeMode::service_window ||
          reg.remaining_meaning !=
              ActionRemainingMeaning::authorization_window ||
          proof.phase != ActionTimeOwnerPhase::suspended)
        fail("UnconsumedActionTerminalDue");
      const auto actual =
          take(source.read_exhausted_authorization_window(reg, proof, current));
      const auto remaining =
          take(source.read_original_action_remaining(reg, proof, current));
      const auto *window =
          std::get_if<ActualAuthorizationRemaining>(&remaining);
      if (!window || actual.action_ref != proof.action_ref ||
          !same_source_ref(actual.original_execution_source,
                           proof.original_execution_source) ||
          !same_source_ref(actual.original_terms_source,
                           window->original_terms_source) ||
          actual.source_schema != window->source_schema ||
          actual.until_tick != window->until_tick ||
          !same_time_source(actual.actual_window_closed_source,
                            *proof.suspended_source) ||
          (actual.stop != RunStop::paused && actual.stop != RunStop::blocked))
        fail("AuthorizationExhaustionSourceMismatch");
      auto closed = read_time(actual.actual_window_closed_source, source, b);
      if (take(compare_world_moments(closed, {actual.until_tick, {}}, b)) !=
              0 ||
          take(compare_world_moments(closed, current, b)) > 0)
        fail("AuthorizationWindowNotClosed");
      if (take(source.read_action_continuation_stop(reg, proof, current)) !=
          actual.stop)
        fail("AuthorizationWindowStopMismatch");
    }
    return projected;
  });
}
Status BoundActionTimeValidationRead::validate_action_time(const ActionCoreView& action)const{return guard<Done>([&]{const auto& reg=registration(registrations_,action.kind);auto proof=take(source_.read_action_time_owner(reg,action.id));auto projected=take(read_action_time_projection(reg,proof,elapsed_,current_,scale_,source_,budget_));if(terminal(elapsed_,action.id))fail("ActiveActionHasTerminalBinding");compare_action(reg,action,proof,projected);return Done{};});}
// 原owner枚举与共同段双向核对；没有来源压缩协议时不删除必要的早期gate。
Status validate_action_time_closure(const CommonRead& common,const ActionElapsedStore& elapsed,const std::vector<ActionTimeRegistration>& registrations,const WorldMoment& current,const TimeScaleRef& scale,const ActionTimeOriginalSourceRead& source,ExactArithmeticBudget b){return guard<Done>([&]{take(validate_action_time_registry(registrations));take(validate_fixed_history_values(elapsed,current,scale,b));BoundActionTimeValidationRead active_reader(elapsed,registrations,current,scale,source,b);BorrowedActionIdSet active_ids;for(const auto& action_id:common.action_order()){const auto* action=common.action(action_id);if(!action||action->id!=action_id||!active_ids.insert(action_id).second)fail("DuplicateActionTimeIdentity");take(active_reader.validate_action_time(*action));}
    // Keys own fresh text for this validation only; nodes are tracked separately.
    CheckpointMemoryCharge result_key_memory;
    using ResultMapValue=std::pair<const StableId,const SharedWorkGateResult*>;
    std::map<StableId,const SharedWorkGateResult*,std::less<StableId>,CheckpointAllocator<ResultMapValue>> results;
    const StableId* previous_result=nullptr;
    for(const auto& result:elapsed.gate_results){auto id=take(source.read_gate_result_id(result));if(id.empty()||(previous_result&&id<=*previous_result))fail("InvalidSharedGateResultOrder");(result_key_memory).grow(0);auto inserted=results.emplace(id,&result);if(!inserted.second)fail("InvalidSharedGateResultOrder");previous_result=&inserted.first->first;auto original=take(source.read_original_gate_result(id));if(!same_shared_work_gate_result(result,original))fail("SharedGateOriginalResultMismatch");take(source.validate_original_gate_result(original));}
    BorrowedActionIdSet seen_entries,used_results,seen_terminals;for(const auto& item:elapsed.entries){const auto& id=action_elapsed_entry_ref(item);if(!seen_entries.insert(id).second)fail("DuplicateActionElapsedEntry");const auto* action=common.action(id);const auto* ended=terminal(elapsed,id);if((action!=nullptr)==(ended!=nullptr))fail("ElapsedActiveTerminalMismatch");const auto& reg=registration(registrations,action?action->kind:ended->action_kind);auto proof=take(source.read_action_time_owner(reg,id));auto projection=take(read_action_time_projection(reg,proof,elapsed,current,scale,source,b));if(action){if(!active_ids.count(id))fail("MissingActionTimeEnumeration");compare_action(reg,*action,proof,projection);}else{if(!seen_terminals.insert(id).second||proof.phase!=ActionTimeOwnerPhase::terminal||ended->timing_kind!=static_cast<Integer>(item.index())||ended->actor_id!=proof.actor_id||ended->participant_actor_ids!=proof.participant_actor_ids||!same_source_ref(ended->original_execution_source_ref,proof.original_execution_source)||!same_time_source(ended->started_source_ref,proof.started_source)||!proof.ended_source||!same_time_source(ended->ended_source_ref,*proof.ended_source)||!proof.terminal_result_ref||ended->terminal_result_ref!=*proof.terminal_result_ref||ended->retained_gate_result_refs!=proof.retained_gate_result_refs)fail("ActionTerminalSourceMismatch");auto end=read_time(ended->ended_source_ref,source,b);if(take(compare_world_moments(end,projection.started_at,b))<0||take(compare_world_moments(end,current,b))>0)fail("ActionTerminalTimeOrder");}
        if(item.index()==0){if(!proof.retained_gate_result_refs.empty())fail("FixedActionHasWorkGates");continue;}const auto& shared=std::get<SharedWorkElapsedState>(item);CheckpointVector<const SharedWorkGateResult*> chain;for(const auto& result_id:proof.retained_gate_result_refs){auto it=results.find(result_id);if(it==results.end()||!used_results.insert(it->first).second)fail("MissingOrSharedGateResult");binding_owner(result_binding(*it->second,source,scale.units_per_half_day,b),proof);if(it->second->index()!=shared.gate.index())fail("WrongGateResultKind");chain.push_back(it->second);}std::sort(chain.begin(),chain.end(),[](const auto* a,const auto* z){return ordinal(*a)<ordinal(*z);});Integer expected=0,done=0;std::optional<WorldMoment> last_life_applied;for(const auto* result:chain){if(ordinal(*result)!=expected)fail("SharedGateHistoryGap");if(expected==std::numeric_limits<Integer>::max())fail("SharedGateOrdinalOverflow");++expected;std::visit([&](const auto& r){using T=std::decay_t<decltype(r)>;if constexpr(std::is_same_v<T,CraftWorkGateResult>){if(r.work_units_before!=done)fail("SharedGateCumulativeMismatch");done=r.work_units_after;}else if constexpr(std::is_same_v<T,OriginalHalfDayGateResult>){if(r.completed_before!=done)fail("SharedGateCumulativeMismatch");done=r.completed_after;}else{const auto* reader=source.life_work_period_sources(r.work_period_ref);if(!reader)fail("MissingOriginalLifeWorkPeriodReader",ErrorKind::unsupported_capability);auto period=take(read_verified_life_work_period(r.work_period_ref,*reader,scale.units_per_half_day,b));const auto& gate=std::get<LifeHalfDayStageGateState>(shared.gate);if(period.completed_halfday_ordinal!=r.completed_halfday_ordinal||period.gate_profile!=gate.gate_profile_ref||!same_source_ref(period.stage_rule_source_ref,gate.stage_rule_source_ref)||period.completed_stage_ordinals!=r.completed_stage_ordinals||period.original_stage_result_refs.size()!=r.original_stage_result_refs.size())fail("LifeGatePeriodResultMismatch");for(std::size_t k=0;k<r.original_stage_result_refs.size();++k)if(!same_source_ref(period.original_stage_result_refs[k],r.original_stage_result_refs[k]))fail("LifeGateStageResultMismatch");if(take(compare_world_moments(period.applied_at,current,b))>0||take(compare_world_moments(period.service_segments.front().actual_span.from,projection.started_at,b))<0||(last_life_applied&&take(compare_world_moments(*last_life_applied,period.service_segments.front().actual_span.from,b))>0))fail("LifeGatePeriodTimeOrder");last_life_applied=period.applied_at;if(done>std::numeric_limits<Integer>::max()-scale.units_per_half_day)fail("SharedGateWorkOverflow");done+=scale.units_per_half_day;}},*result);}
        if(done!=shared.execution.completed_work_units)fail("SharedGateWorkCoverageMismatch");if(ended){const std::optional<StableId> latest=chain.empty()?std::nullopt:std::optional<StableId>{take(source.read_gate_result_id(*chain.back()))};if(ended->last_closed_gate_result_ref!=latest)fail("TerminalLastSharedGateMismatch");}if(const auto* craft=std::get_if<CraftWorkGate>(&shared.gate)){if(craft->last_closed_gate_result_ref.has_value()!=!chain.empty()||(!chain.empty()&&*craft->last_closed_gate_result_ref!=take(source.read_gate_result_id(*chain.back()))))fail("CraftLastGateMismatch");if(ended&&ended->last_closed_gate_result_ref!=craft->last_closed_gate_result_ref)fail("TerminalLastCraftGateMismatch");}else if(const auto* original=std::get_if<OriginalHalfDayGate>(&shared.gate)){if(original->next_period_ordinal!=expected||original->last_consumed_period_ref.has_value()!=!chain.empty()||(!chain.empty()&&!same_work_period_ref(*original->last_consumed_period_ref,std::get<OriginalHalfDayGateResult>(*chain.back()).period_source_ref)))fail("OriginalLastGateMismatch");}}
    // 反向由真实owner枚举；同时删elapsed+terminal仍不能把已发生固定/工作行动藏掉。
    BorrowedActionIdSet original_ids;for(const auto& reg:registrations){auto visit=[&](std::string_view original_id)->Status{return guard<Done>([&]{if(original_id.empty()||!original_ids.insert(original_id).second)fail("DuplicateOriginalActionTimeOwner");auto proof=take(source.read_action_time_owner_borrowed(reg,original_id));if(proof.action_ref!=original_id)fail("OriginalActionTimeOwnerIdentityMismatch");const StableId& id=proof.action_ref;auto projected=take(read_action_time_projection(reg,proof,elapsed,current,scale,source,b));const bool has_entry=entry(elapsed,id)!=nullptr;if((reg.mode==ActionTimeMode::fixed_duration||reg.mode==ActionTimeMode::shared_work)!=has_entry)fail("OriginalActionElapsedCoverageMismatch");const auto* actual=common.action(id);if(proof.phase==ActionTimeOwnerPhase::terminal){if(actual)fail("TerminalOriginalActionStillActive");}else{if(!actual||!active_ids.count(id))fail("OriginalActiveActionMissing");compare_action(reg,*actual,proof,projected);}return Done{};});};OriginalOwnerSink<decltype(visit)> sink(visit);take(source.enumerate_action_time_owners(reg,sink));}
    for(const auto& id:active_ids)if(!original_ids.count(id))fail("MissingOriginalActionEnumeration");for(const auto& id:seen_entries)if(!original_ids.count(id))fail("MissingOriginalElapsedEnumeration");
    if(used_results.size()!=results.size()||seen_terminals.size()!=elapsed.terminal_bindings.size())fail("OrphanActionTimeSource");return Done{};});}
}
