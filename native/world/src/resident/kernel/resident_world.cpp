#include "resident/domains/military/military_supply_lookup.hpp"
#include "native_political_relations.hpp"
#include "checkpoint_phase_timing.hpp"
#include "world_stage_observation.hpp"
#include <cerrno>
#include <exception>
#include "native_interaction_commit.hpp"
#include "native_military_battle_lifecycle.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/military/military_battle_lifecycle.hpp"
#include "native_interaction_catalog.hpp"
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include "native_production_cycles.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "world_checkpoint_write_scope.hpp"
#include "world_restore_candidate.hpp"
#include "json_decoded_world_seed.hpp"
#include "world_restore_retention.hpp"
#include "exact_social_period.hpp"
#include "exact_time_world.hpp"
#include "exact_complete_world.hpp"
#include "exact_rest_binding.hpp"
#include "complete_growth_command_binding.hpp"
#include "complete_task_command_binding.hpp"
#include "native_control_command_route.hpp"
#include "native_field_army_exit_command_route.hpp"
#include "native_director_wake_world.hpp"
#include "native_transport_catalog.hpp"
#include "resident/domains/growth/profile_acquisition_world.hpp"
#include "common_command_sources.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "private_shop_liquidation_integration.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "economic_world_integration.hpp"
#include "private_shop_integration.hpp"
#include "private_shop_daily_rent_integration.hpp"
#include "political_shop_registry_integration.hpp"
// 原子提交唯一常驻世界；整候选成功才换指针，失败保旧clock/RNG/资产/请求与结果。
#include "dynamic_integration.hpp"
#include "world_impl.hpp"
#include "continuation_snapshot.hpp"
#include "player_plan_integration.hpp"
#include "resident/domains/military/military_phases.hpp"
#include "resident/views/journey_view.hpp"
#include <atomic>
#include <chrono>
#include <limits>
#include <random>
namespace hanmo::resident::world_stage_observation {
namespace {
// 唯一线程局部标量；只在显式宿主窗内生成观察身份，不复用Common序号。
thread_local Binding active_binding;
thread_local std::uint64_t next_call = 0;
thread_local std::uint32_t active_depth = 0;
constexpr const char* stage_names[] = {
  "submit.call", "submit.prepare", "submit.candidate_copy", "submit.source_capture",
  "submit.apply", "submit.commit_tail", "commit_tail.call", "commit_tail.validate",
  "opening.call", "opening.candidate_copy", "opening.apply", "opening.validate",
  "capture.fallback_begin", "capture.settle", "capture.finish",
  "checkpoint.validate", "checkpoint.snapshot", "checkpoint.serialize", "world.validate"
};
// 时间/CPU由宿主同步采集；系统诊断调用不得改变原业务可见的errno。
void emit(Binding binding, const char* stage, const char* edge,
          std::uint64_t id, std::uint32_t depth) noexcept {
  const int previous_errno = errno;
  binding.sink(binding.context, stage, edge, id, depth);
  errno = previous_errno;
}
}
Binding exchange_binding(Binding next) noexcept {
  const auto previous = active_binding;
  active_binding = next;
  return previous;
}
Scope::Scope(Stage stage) noexcept {
  if (!active_binding.sink ||
      next_call == std::numeric_limits<std::uint64_t>::max() ||
      active_depth == std::numeric_limits<std::uint32_t>::max()) return;
  binding_ = active_binding;
  stage_ = stage_names[static_cast<unsigned>(stage)];
  call_id_ = ++next_call;
  depth_ = ++active_depth;
  exceptions_ = std::uncaught_exceptions();
  emit(binding_, stage_, "BEGIN", call_id_, depth_);
}
Scope::~Scope() noexcept { stop(); }
void Scope::stop() noexcept {
  if (!binding_.sink) return;
  const auto binding = binding_;
  binding_.sink = nullptr;
  active_depth = depth_ - 1;
  emit(binding, stage_, std::uncaught_exceptions() > exceptions_ ? "ABORT" : "END",
       call_id_, depth_);
}
}
namespace hanmo::resident {
Outcome<ReceiptDetails> apply_command_with_complete_capture(
    WorldBundle &, const StableId &, const PreparedDomainOp &,
    CompleteExactCommandCapture &);

namespace {
template<class T> Outcome<T> failure(const StableId& code){return {false,{},common_failure(code).error};}
// 一进程中所有live/replacement实例共享代次；恢复同一持久world也不能复用旧回调身份。
Outcome<Integer> allocate_execution_epoch(){static std::atomic<Integer> next{1};Integer value=next.load(std::memory_order_relaxed);for(;;){if(value==std::numeric_limits<Integer>::max())return failure<Integer>("ExecutionEpochExhausted");if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return {true,value,{}};}}
bool zero_id(const WorldId& id){return std::all_of(id.begin(),id.end(),[](auto b){return b==0;});}
bool same_world(const WorldIdentity& id,const WorldId& world,Integer epoch){return id.world_id==world&&id.execution_epoch==epoch;}
Status seed_ready(const PreparedCatalog* c,const InitialWorldSeed& s){if(!c||!c->common||!c->travel||!c->military||!c->interaction||!c->social||!c->estate||!c->tasks||!c->family||!c->personal||!c->growth||!s.tasks||!s.family||!s.personal||!s.growth||!s.travel||!s.military||!s.interaction||!s.social||!s.estate)return common_failure("IncompleteResidentSeed");return common_success();}
}
// 新局在任何原领域证据写入前取得唯一WorldId；旧安装零ID回退沿用同一分配器。
// 只产生私有候选身份，不分配成功发布时才取得的execution epoch。
WorldId allocate_native_world_id() {
  static std::atomic<std::uint64_t> counter{1};
  std::random_device random;
  WorldId id{};
  for (auto &b : id)
    b = static_cast<std::uint8_t>(random());
  auto n = counter.fetch_add(1);
  for (std::size_t i = 0; i < 8; ++i)
    id[i] ^= static_cast<std::uint8_t>(n >> (i * 8));
  return id;
}
ResidentWorld::ResidentWorld() : impl_(std::make_unique<Impl>()) {}
ResidentWorld::~ResidentWorld() = default;
Status ResidentWorld::install(std::shared_ptr<const PreparedCatalog> catalog,InitialWorldSeed&& seed,bool fresh_native){
    return install_seed(std::move(catalog),seed,fresh_native,nullptr);
}
Status ResidentWorld::install_json_seed(std::shared_ptr<const PreparedCatalog> catalog,JsonDecodedWorldSeedOwner&& owner){
    auto prepared=owner.require_catalog(std::move(catalog));
    return install_seed(std::move(prepared),owner.install_seed(),false,&owner);
}
// 只在私有候选中按依赖顺序初始化；原拥有根校验和全验成功后才发布。
Status
ResidentWorld::install_seed(std::shared_ptr<const PreparedCatalog> catalog,
                            InitialWorldSeed &seed, bool fresh_native,
                            JsonDecodedWorldSeedOwner *json_owner) {
  WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
  if (storage_restore_pending())
    return common_failure("StorageRestoreBusy");
  if (impl_->world)
    return common_failure("AlreadyInstalled");
  auto ready = seed_ready(catalog.get(), seed);
  if (!ready.ok)
    return ready;
  const bool dynamic =
      catalog->dynamic_profile == DynamicProfile::local_society_v1;
  if ((!dynamic || fresh_native) && zero_id(seed.common.identity.world_id))
    seed.common.identity.world_id = allocate_native_world_id();
  seed.common.identity.execution_epoch = 0;
  auto candidate = json_owner ? checkpoint_make_shared<WorldBundle>(
                                    std::move(catalog), std::move(*json_owner))
                              : checkpoint_make_shared<WorldBundle>(
                                    std::move(catalog), std::move(seed));
  if (fresh_native) {
    auto relations = initialize_native_political_relations_in_candidate(*candidate);
    if (!relations.ok)
      return relations;
    // 原卡取得必须先由真实初态writer完成；Social/Production随后借全图时已要求此根。
    // WorldId和原seed均已建立，恢复不走本分支，不能以空CardAcquisitionState替代核验。
    if (candidate->catalog->growth->complete_capability_application_chain) {
      auto initialized =
          initialize_profile_acquisitions_from_original_seed_in_candidate(
              *candidate);
      if (!initialized.ok)
        return initialized;
    }
    auto initialized =
        initialize_private_commerce_world_in_candidate(*candidate);
    if (!initialized.ok)
      return initialized;
    initialized =
        initialize_private_shop_liquidation_world_in_candidate(*candidate);
    if (!initialized.ok)
      return initialized;
    initialized =
        initialize_private_shop_command_results_world_in_candidate(*candidate);
    if (!initialized.ok)
      return initialized;
    initialized = bootstrap_exact_social_in_candidate(*candidate);
    if (!initialized.ok)
      return initialized;
    initialized = initialize_native_production_cycles_in_candidate(*candidate);
    if (!initialized.ok)
      return initialized;
  }
  if (!fresh_native) {
    auto hydrated =
        restore_native_transport_derived_values_in_candidate(*candidate);
    if (!hydrated.ok)
      return hydrated;
  }
  auto director_sources =
      fresh_native
          ? initialize_native_complete_director_in_candidate(*candidate)
          : restore_native_complete_director_wake_cache_in_candidate(
                *candidate);
  if (!director_sources.ok)
    return director_sources;
  auto valid = candidate->validate();
  if (!valid.ok)
    return valid;
  if (fresh_native && !candidate->catalog->exact_time) {
    valid = bootstrap_dynamic_world(*candidate);
    if (!valid.ok)
      return valid;
    valid = candidate->validate();
    if (!valid.ok)
      return valid;
  }
  auto epoch = allocate_execution_epoch();
  if (!epoch.ok)
    return {false, {}, epoch.error};
  if (zero_id(candidate->common.state().identity.world_id))
    candidate->common.state().identity.world_id = allocate_native_world_id();
  candidate->common.state().identity.execution_epoch = epoch.value;
  candidate->map_presence_index = build_map_presence_index(candidate->common);
  candidate->economic_index = rebuild_economic_world_index(*candidate);
  candidate->fiscal_index = rebuild_economic_fiscal_index(*candidate);
  candidate->household_provision_index =
      rebuild_household_provision_index(*candidate);
  candidate->politics_view_index = make_politics_view_index(
      candidate->common, candidate->politics, candidate->organization,
      candidate->catalog->organization ? *candidate->catalog->organization
                                       : OrganizationCatalog{},
      candidate->social, &candidate->political_fiscal);
  candidate->governance_view_index = make_governance_view_index(
      candidate->common, candidate->politics_governance, candidate->tasks,
      candidate->governance_tasks);
  candidate->rebuild_diplomacy_index();
  impl_->world = std::move(candidate);
  return common_success();
}
Outcome<WorldIdentity> ResidentWorld::identity()const{if(!impl_->world)return failure<WorldIdentity>("NoWorld");return {true,impl_->world->common.state().identity,{}};}
Outcome<std::shared_ptr<const PreviewResult>> ResidentWorld::preview(const CommandEnvelope& command)const{
    if(!impl_->world)return failure<std::shared_ptr<const PreviewResult>>("NoWorld");const auto& w=*impl_->world;
    if(!same_world(w.common.state().identity,command.world_id,command.execution_epoch))return failure<std::shared_ptr<const PreviewResult>>("StaleWorld");
    if(!command.payload)return failure<std::shared_ptr<const PreviewResult>>("InvalidCommand");if(w.dynamic_enabled()&&w.catalog->director->dynamic_subjects&&w.common.actor(command.actor_id)&&command.actor_id!=w.common.state().metadata.player_actor_id)return failure<std::shared_ptr<const PreviewResult>>("PlayerCommandAuthorityRequired");return prepare_command(w,command.actor_id,*command.payload);
}
// 原请求缓存与版本优先于重新报价；不能让新payload改变一次已成功请求的回执。
Outcome<std::shared_ptr<const CommitResult>> ResidentWorld::submit(const CommandEnvelope& command){
    world_stage_observation::Scope submit_observation(world_stage_observation::Stage::submit_call);
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
    if(storage_restore_pending())return failure<std::shared_ptr<const CommitResult>>("StorageRestoreBusy");
    impl_->last_submission_timing.clear();if(!impl_->world)return failure<std::shared_ptr<const CommitResult>>("NoWorld");auto& current=*impl_->world;
    if(!same_world(current.common.state().identity,command.world_id,command.execution_epoch))return failure<std::shared_ptr<const CommitResult>>("StaleWorld");
    if(command.request_seq<1||command.request_seq>2147483647)return failure<std::shared_ptr<const CommitResult>>("InvalidCommand");
    if(!current.common.actor(command.actor_id))return failure<std::shared_ptr<const CommitResult>>("UnknownActor");
    if(current.dynamic_enabled()&&current.catalog->director->dynamic_subjects&&command.actor_id!=current.common.state().metadata.player_actor_id)return failure<std::shared_ptr<const CommitResult>>("PlayerCommandAuthorityRequired");
    auto key=command.actor_id+":"+std::to_string(command.request_seq);auto* sequence=current.common.state().request_sequences.find(command.actor_id);
    if(!sequence)return failure<std::shared_ptr<const CommitResult>>("InvalidRequestSequence");
    if(command.request_seq<=*sequence){auto* cached=current.common.state().results.find(key);if(cached)return {true,*cached,{}};return failure<std::shared_ptr<const CommitResult>>("AlreadyApplied");}
    if(command.expected_revision!=current.common.revision())return failure<std::shared_ptr<const CommitResult>>("StalePreview");
    if(!command.payload)return failure<std::shared_ptr<const CommitResult>>("InvalidCommand");
    world_stage_observation::Scope prepare_observation(world_stage_observation::Stage::submit_prepare);
    auto prepared=prepare_command(current,command.actor_id,*command.payload);prepare_observation.stop();if(!prepared.ok)return {false,{},prepared.error};
    if(current.catalog->exact_time&&std::holds_alternative<AdvanceCommand>(command.payload->data)&&current.world_timeline->boundary_state==PeriodBoundaryState::ready){
        // 原命令身份/seq/preview已核；Opening是独立真实协调提交，不捏Actor命令或占用请求序。
        auto opened=impl_->open_exact_period();if(!opened.ok)return {false,{},opened.error};auto continued=command;continued.expected_revision=impl_->world->common.revision();return submit(continued);
    }
    const auto* plan_quote=std::get_if<PlayerPlanQuote>(&prepared.value->quote->data);const auto* plan_execution=plan_quote&&plan_quote->execution?plan_quote->execution.get():nullptr;
    if(plan_execution){const auto* person=current.common.actor(command.actor_id);const auto* wallet=person?current.common.wallet(person->wallet_id):nullptr;if(!wallet||wallet->owner.kind!=Owner::Kind::actor||wallet->owner.id!=command.actor_id)return failure<std::shared_ptr<const CommitResult>>("RunPersonalWalletRequired");}
    const auto clone_start=std::chrono::steady_clock::now();world_stage_observation::Scope copy_observation(world_stage_observation::Stage::submit_candidate_copy);auto candidate=checkpoint_make_shared<WorldBundle>(current);copy_observation.stop();candidate->timing.emplace_back("candidate_clone",std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-clone_start).count());if(impl_->run_spend_guard||plan_execution){const auto wallet=impl_->run_spend_guard?impl_->run_spend_guard->wallet_id:candidate->common.actor(command.actor_id)->wallet_id;auto observed=candidate->common.begin_wallet_debit_observation(wallet);if(!observed.ok)return {false,{},observed.error};}
    std::unique_ptr<CompleteExactCommandCapture> actual_capture;
    if(candidate->catalog->exact_time&&exact_complete_selected(*candidate->catalog->exact_time)){
        // Advance and zero-time commands share the one actual submission
        // capture through the true Common seal and original postseal binders.
        world_stage_observation::Scope capture_observation(world_stage_observation::Stage::submit_source_capture);
        auto begun=CompleteExactCommandCapture::begin(*candidate);capture_observation.stop();if(!begun.ok)return {false,{},std::move(begun.error)};actual_capture=std::move(begun.value);
    }
    world_stage_observation::Scope apply_observation(world_stage_observation::Stage::submit_apply);
    MilitarySupplyLookupPass supply_lookup(candidate->common.state(),candidate->military);
    auto applied=actual_capture?apply_command_with_complete_capture(*candidate,command.actor_id,*prepared.value->operation,*actual_capture):apply_command(*candidate,command.actor_id,*prepared.value->operation);supply_lookup.stop();apply_observation.stop();if(!applied.ok)return {false,{},applied.error};
    // 只在显式测试编译中于真实advance完成后拒绝发布，不用提交再恢复来伪造回滚。
#ifdef HANMO_RESIDENT_TEST_FAULTS
    if(impl_->fail_after_full_advance_once&&std::holds_alternative<AdvanceDetails>(applied.value.data)){impl_->fail_after_full_advance_once=false;return failure<std::shared_ptr<const CommitResult>>("OracleInjectedLateFailure");}
#endif
    world_stage_observation::Scope tail_observation(world_stage_observation::Stage::submit_commit_tail);
    std::shared_ptr<const CommitResult> result;const auto committed_kind=plan_execution&&plan_execution->nested?plan_execution->nested->command.kind:command.payload->kind;StableId registry_acceptance_id;if(committed_kind==political_shop_registry_accept_command){auto source=political_shop_registry_new_player_acceptance(current.political_shop_registry_successions,candidate->political_shop_registry_successions,command.actor_id,candidate->common.tick());if(!source.ok)return {false,{},source.error};registry_acceptance_id=std::move(source.value);}StableId licensing_source_id;if(candidate->catalog->political_shop_licensing_receipts&&political_shop_licensing_command_kind(committed_kind)){auto source=political_shop_licensing_new_player_source(current.political_shop_licensing,candidate->political_shop_licensing,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};licensing_source_id=std::move(source.value);}StableId governance_continuation_id;if(committed_kind==political_governance_continue_command){if(!current.political_governance_continuations||!candidate->political_governance_continuations)return {false,{},common_failure("GovernanceContinuationProfileRequired").error};auto source=political_governance_continuation_new_player_source(*current.political_governance_continuations,*candidate->political_governance_continuations,command.actor_id,candidate->common.tick());if(!source.ok)return {false,{},source.error};governance_continuation_id=std::move(source.value);}StableId political_succession_source_id;if(is_political_succession_command(committed_kind)){if(!current.political_successions||!candidate->political_successions)return {false,{},common_failure("PoliticalSuccessionProfileRequired").error};auto source=political_succession_new_player_source(*current.political_successions,*candidate->political_successions,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_succession_source_id=std::move(source.value);}StableId political_head_handover_source_id;if(is_political_head_handover_command(committed_kind)){if(!current.political_head_handovers||!candidate->political_head_handovers)return {false,{},common_failure("PoliticalHeadHandoverProfileRequired").error};auto source=political_head_handover_new_player_source(*current.political_head_handovers,*candidate->political_head_handovers,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_head_handover_source_id=std::move(source.value);}StableId political_successor_round_source_id;if(is_political_successor_round_command(committed_kind)){if(!current.political_successor_rounds||!candidate->political_successor_rounds)return {false,{},common_failure("PoliticalSuccessorRoundProfileRequired").error};auto source=political_successor_round_new_player_source(*current.political_successor_rounds,*candidate->political_successor_rounds,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_successor_round_source_id=std::move(source.value);}StableId political_term_handover_source_id;if(is_political_term_handover_command(committed_kind)){if(!current.political_term_handovers||!candidate->political_term_handovers)return {false,{},common_failure("PoliticalTermHandoverProfileRequired").error};auto source=political_term_handover_new_player_source(*current.political_term_handovers,*candidate->political_term_handovers,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_term_handover_source_id=std::move(source.value);}StableId political_office_reception_source_id;if(is_political_office_reception_command(committed_kind)){if(!current.political_office_receptions||!candidate->political_office_receptions)return {false,{},common_failure("PoliticalOfficeReceptionProfileRequired").error};auto source=political_office_reception_new_player_source(*current.political_office_receptions,*candidate->political_office_receptions,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_office_reception_source_id=std::move(source.value);}StableId political_office_resignation_source_id;if(is_political_office_resignation_command(committed_kind)){if(!current.political_office_resignations||!candidate->political_office_resignations)return {false,{},common_failure("PoliticalOfficeResignationProfileRequired").error};auto source=political_office_resignation_new_player_source(*current.political_office_resignations,*candidate->political_office_resignations,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_office_resignation_source_id=std::move(source.value);}StableId subordinate_appointment_source_id;if(is_political_subordinate_appointment_command(committed_kind)){if(!current.political_subordinate_appointments||!candidate->political_subordinate_appointments)return {false,{},common_failure("PoliticalSubordinateAppointmentProfileRequired").error};auto source=political_subordinate_appointment_new_player_source(*current.political_subordinate_appointments,*candidate->political_subordinate_appointments,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};subordinate_appointment_source_id=std::move(source.value);}StableId dismissal_source_id;if(candidate->catalog->political_dismissal_sources){if(!current.political_dismissal_sources||!candidate->political_dismissal_sources)return {false,{},common_failure("PoliticalDismissalSourceProfileRequired").error};auto source=political_dismissal_new_result_source(*current.political_dismissal_sources,*candidate->political_dismissal_sources,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};dismissal_source_id=std::move(source.value);}StableId political_governance_agency_source_id;if(is_political_governance_agency_command(committed_kind)){if(!current.political_governance_agencies||!candidate->political_governance_agencies)return {false,{},common_failure("PoliticalGovernanceAgencyProfileRequired").error};auto source=political_governance_agency_new_player_source(*current.political_governance_agencies,*candidate->political_governance_agencies,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};political_governance_agency_source_id=std::move(source.value);}StableId invitation_reply_source_id;if(is_governance_invitation_reply_command(committed_kind)){if(!current.governance_invitation_replies||!candidate->governance_invitation_replies)return {false,{},common_failure("GovernanceInvitationProfileRequired").error};auto source=governance_invitation_new_player_source(*current.governance_invitation_replies,*candidate->governance_invitation_replies,command.actor_id,committed_kind,candidate->common.tick());if(!source.ok)return {false,{},source.error};invitation_reply_source_id=std::move(source.value);}auto visit_message_sources=visit_message_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_message_sources.ok)return {false,{},visit_message_sources.error};auto visit_invitation_source=visit_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_invitation_source.ok)return {false,{},visit_invitation_source.error};auto visit_meeting_sources=visit_appointment_meeting_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_meeting_sources.ok)return {false,{},visit_meeting_sources.error};auto shop_order_contact_source=task_shop_order_contact_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!shop_order_contact_source.ok)return {false,{},shop_order_contact_source.error};auto known_dialogue_source=known_personal_dialogue_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!known_dialogue_source.ok)return {false,{},known_dialogue_source.error};auto born_dialogue_source=born_personal_dialogue_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!born_dialogue_source.ok)return {false,{},born_dialogue_source.error};auto visit_reply_source=visit_reply_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!visit_reply_source.ok)return {false,{},visit_reply_source.error};auto inquiry_sources=contact_inquiry_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!inquiry_sources.ok)return {false,{},inquiry_sources.error};auto appointment_sources=personal_appointment_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!appointment_sources.ok)return {false,{},appointment_sources.error};auto personal_cancellation_source=task_personal_cancellation_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!personal_cancellation_source.ok)return {false,{},personal_cancellation_source.error};auto personal_acceptance_sources=task_personal_acceptance_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!personal_acceptance_sources.ok)return {false,{},personal_acceptance_sources.error};auto personal_report_sources=task_personal_report_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!personal_report_sources.ok)return {false,{},personal_report_sources.error};auto personal_message_sources=task_personal_message_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!personal_message_sources.ok)return {false,{},personal_message_sources.error};auto task_return_sources=task_message_return_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value);if(!task_return_sources.ok)return {false,{},task_return_sources.error};auto completed=commit_tail(*candidate,command.actor_id,command.request_seq,committed_kind,applied.value,result,plan_execution,registry_acceptance_id,appointment_sources.value,current.personal_recovery_enabled()?&current.personal_recovery:nullptr,licensing_source_id,current.private_shop_liquidations_enabled()?&current.private_shop_liquidations:nullptr,current.tasks.shop_orders&&current.tasks.shop_orders->estate_cancellations?&*current.tasks.shop_orders->estate_cancellations:nullptr,current.family_apprenticeship_enabled()?&current.family_apprenticeship:nullptr,governance_continuation_id,inquiry_sources.value,visit_reply_source.value,current.family_disciple_legacy_enabled()?&current.family_legacy:nullptr,current.family_disciple_legacy_enabled()?&current.family_disciple_legacy:nullptr,invitation_reply_source_id,current.private_shop_daily_rent_enabled()?&current.private_shop_daily_rent:nullptr,born_dialogue_source.value,current.family_biological_legacy_enabled()?&current.family_legacy:nullptr,current.family_biological_legacy_enabled()?&current.family_biological_legacy:nullptr,political_succession_source_id,visit_invitation_source.value,visit_meeting_sources.value,political_head_handover_source_id,current.growth_tactic_lessons_enabled()?&current.growth_tactic_lessons:nullptr,current.common.tick(),political_successor_round_source_id,&current,political_term_handover_source_id,visit_message_sources.value,known_dialogue_source.value,political_office_reception_source_id,shop_order_contact_source.value,current.family_collateral_legacy_enabled()?&current.family_legacy:nullptr,current.family_collateral_legacy_enabled()?&current.family_collateral_legacy:nullptr,political_governance_agency_source_id,task_return_sources.value,current.family_voluntary_paths_enabled()?&current:nullptr,personal_message_sources.value,political_office_resignation_source_id,personal_report_sources.value,personal_cancellation_source.value,subordinate_appointment_source_id,personal_acceptance_sources.value,dismissal_source_id,actual_capture.get());tail_observation.stop();if(!completed.ok)return {false,{},completed.error};
    // 有限回执的完整转换也在发布前准备；不因输出结构缺失发布后才报错。
    auto encoded=encode_resident_receipt({true,result,{}});if(!encoded.ok)return {false,{},encoded.error};
    // 全部真实领域与提交尾段完成后才核毛支出；超额候选整体丢弃，绝不采纳后回滚。
    if(impl_->run_spend_guard){const auto spent=candidate->common.observed_wallet_debits();if(spent>impl_->run_spend_guard->remaining)return failure<std::shared_ptr<const CommitResult>>("RunMoneyBudgetExceeded");impl_->run_spend_guard->observed=spent;candidate->common.clear_wallet_debit_observation();}
    if(plan_execution)candidate->common.clear_wallet_debit_observation();
    impl_->last_submission_timing=candidate->timing;impl_->world=std::move(candidate);return {true,std::move(result),{}};
}
Status commit_tail(WorldBundle& w,const StableId& actor,Integer sequence,const StableId& kind,const ReceiptDetails& details,std::shared_ptr<const CommitResult>& result,const PlayerPlanExecution* plan_execution,const StableId& registry_acceptance_id,const std::vector<StableId>& appointment_sources,const GrowthRecoveryState* recovery_before,const StableId& licensing_source_id,const PrivateShopLiquidationState* liquidation_before,const TaskShopOrderEstateCancellationState* estate_orders_before,const FamilyApprenticeshipState* apprenticeship_before,const StableId& governance_continuation_id,const std::vector<StableId>& inquiry_sources,const StableId& visit_reply_source,const FamilyLegacyState* disciple_legacy_before,const DiscipleControlState* disciple_bindings_before,const StableId& invitation_reply_source_id,const PrivateShopDailyRentState* daily_rent_before,const StableId& born_dialogue_source,const FamilyLegacyState* biological_legacy_before,const BiologicalControlState* biological_bindings_before,const StableId& political_succession_source_id,const StableId& visit_invitation_source,const std::vector<StableId>& visit_meeting_sources,const StableId& political_head_handover_source_id,const GrowthTacticLessonState* tactic_lesson_before,Integer tactic_lesson_from_tick,const StableId& political_successor_round_source_id,const WorldBundle* shop_command_before,const StableId& political_term_handover_source_id,const VisitMessageResultSources& visit_message_sources,const StableId& known_dialogue_source,const StableId& political_office_reception_source_id,const StableId& shop_order_contact_source,const FamilyLegacyState* collateral_legacy_before,const CollateralControlState* collateral_before,const StableId& political_governance_agency_source_id,const TaskMessageReturnResultBatch& task_return_sources,const WorldBundle* voluntary_before,const TaskPersonalMessageResultBatch& personal_message_sources,const StableId& political_office_resignation_source_id,const TaskPersonalReportResultBatch& personal_report_sources,const StableId& personal_cancellation_source,const StableId& subordinate_appointment_source_id,const TaskPersonalAcceptanceResultBatch& personal_acceptance_sources,const StableId& dismissal_source_id,CompleteExactCommandCapture* actual_capture){
    world_stage_observation::Scope commit_observation(world_stage_observation::Stage::commit_tail_call);
    const bool full_exact=w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time);
    auto response_layout=validate_native_interaction_response_layout(*w.catalog,w.interaction);if(!response_layout.ok)return response_layout;
    auto response_selected=selected_native_interaction_response_version(w.catalog->identity);if(!response_selected.ok)return {false,{},std::move(response_selected.error)};
    if(response_selected.value&&!full_exact)return common_failure("InteractionResponseCompleteSealRequired",{},ErrorKind::unsupported_capability);
    std::unique_ptr<CompleteExactCommandCapture> tail_capture;
    if(full_exact){
        if(actual_capture){if(!actual_capture->bound_to(w))return common_failure("CompleteCommitCaptureMismatch");}
        else{
            // NPC adoption inside an advance uses its own original acceptance tail;
            // it cannot publish or close the containing player's capture here.
            if(w.active_exact_write_context())return common_failure("NestedCompleteCommitTailOriginalFinalizerRequired");
            world_stage_observation::Scope fallback_observation(world_stage_observation::Stage::capture_fallback_begin);
            auto begun=CompleteExactCommandCapture::begin(w);fallback_observation.stop();if(!begun.ok)return {false,{},std::move(begun.error)};
            tail_capture=std::move(begun.value);actual_capture=tail_capture.get();
        }
    }
    // 新开工须先与真实旧根和外层ActionStarted联接，再进入任何封存前结算。
    if(actual_capture){auto identified=actual_capture->identify_pending_growth_start(shop_command_before,actor,sequence,details);if(!identified.ok)return identified;}
    {auto lifecycle=validate_native_military_battle_lifecycle_layout(w);if(!lifecycle.ok)return lifecycle;
    WorldCurrentMomentRead lifecycle_clock(w);
    MilitaryBattleLifecycleWriteContext lifecycle_context{w.common,w.military,lifecycle_clock};
    auto release_ia=w.services.interaction_access();auto release_ma=w.services.military_access();release_ma.interaction_access=&release_ia;
    release_ma.battle_lifecycle=release_ma.military_battle_lifecycle_schema?&lifecycle_context:nullptr;
    auto released=release_invalid_military_commands(w.common,w.military,*w.catalog->military,release_ma,w.common.tick());if(!released.ok)return released;}
    if(w.population_military_enabled()){auto context=w.population_write();auto population=reconcile_population_military(context,w.common.tick());if(!population.ok)return {false,{},population.error};}
    // 零时水战等真实死亡也必须在本次结果分配前完成原休养和等待对白的同候选终止。
    auto recovery=finish_growth_recovery_boundary_world(w);if(!recovery.ok)return recovery;
    auto lessons=finish_growth_tactic_lesson_boundary_world(w);if(!lessons.ok)return lessons;
    auto tasks=observe_escort_tasks(w.common,w.tasks,*w.catalog->tasks);if(!tasks.ok)return tasks;
    if(actual_capture){world_stage_observation::Scope settle_observation(world_stage_observation::Stage::capture_settle);auto adopted=actual_capture->settle();settle_observation.stop();if(!adopted.ok)return adopted;}
    const bool complete_exact=full_exact;
    std::shared_ptr<const CommitResult> receipt;
    if(complete_exact){
        auto sealed=seal_complete_command_result_in_candidate(w,actor,sequence,kind,details);
        if(!sealed.ok)return {false,{},std::move(sealed.error)};receipt=std::move(sealed.value);
    }else{
        // 旧档保持原封口、精确 Rest 绑定和逐项裁剪的先后顺序。
        auto& state=w.common.state();if(state.identity.revision==std::numeric_limits<Integer>::max())return common_failure("WorldRevisionOverflow");++state.identity.revision;state.request_sequences.set(actor,sequence);
        auto id=w.common.next_id("result");if(!id.ok)return {false,{},id.error};auto original=checkpoint_make_shared<CommitResult>();original->kind=kind;original->tick=w.common.tick();original->revision=w.common.revision();original->result_id=id.value;original->details=checkpoint_make_shared<const ReceiptDetails>(details);
        auto exact_started=bind_exact_rest_started(w,actor,sequence,*original);if(!exact_started.ok)return exact_started;
        state.results.set(actor+":"+std::to_string(sequence),original);
        while(state.results.size()>64){StableId oldest;Integer minimum=2147483647;for(const auto& row:state.results.rows())if((!w.player_plan_enabled()||!player_plan_pins_result(w.player_plan,row.second->result_id))&&row.second->revision<minimum){minimum=row.second->revision;oldest=row.first;}if(oldest.empty())return common_failure("ResultEvictionFailed",{},ErrorKind::internal);state.results.erase(oldest);}
        state.history.push_back({w.common.tick(),kind,actor,id.value,{}});if(state.history.size()>2000)state.history.erase(state.history.begin());
        receipt=std::move(original);
    }
    // 真实成功结果已封口；先绑定本次选口，后续来源读不可遇到未绑定的玩家选择。
    auto field_exit_bound=bind_native_field_army_exit_command_result(w,actor,sequence,*receipt);if(!field_exit_bound.ok)return field_exit_bound;
    if(complete_exact){
        if(!actual_capture)return common_failure("InteractionDeathActualCommandCaptureRequired",{},ErrorKind::internal);
        const SourceRef sealed_deaths{"common","command_result",receipt->result_id};
        auto shop_bound=actual_capture->bind_private_shop_command_result(sealed_deaths);if(!shop_bound.ok)return shop_bound;
        auto deaths_bound=actual_capture->bind_interaction_session_death_result(sealed_deaths);if(!deaths_bound.ok)return deaths_bound;
        auto confirmation_bound=actual_capture->bind_interaction_action_confirmation_result(sealed_deaths);if(!confirmation_bound.ok)return confirmation_bound;
        auto tasks_bound=bind_complete_task_command_result(w,*receipt);if(!tasks_bound.ok)return tasks_bound;
        auto started=actual_capture->bind_growth_command_result(shop_command_before,actor,sequence,*receipt);if(!started.ok)return started;
        if(response_selected.value){
            if(!actual_capture)return common_failure("InteractionOriginalResponseCaptureRequired",{},ErrorKind::internal);
            const SourceRef sealed{"common","command_result",receipt->result_id};
            // Same candidate: real seal/history first, unique original pump
            // next, then the original source binder and second consequence pass.
            auto finalized=actual_capture->finalize_selected_interaction_response(sealed);
            if(!finalized.ok)return finalized;
            auto bound=bind_interaction_selected_response_result_in_candidate(
                w.common,w.interaction,*w.catalog->interaction,sealed);
            if(!bound.ok)return bound;w.note_write_boundary();
        }
        // A retained Control3 response names this actual sealed result. Its
        // original S3 resume binder must consume the selected tail before the
        // full S4 receipt reader follows that same source. The original
        // finalizer dispatches its captured mode and never pumps a resume twice.
        auto control_bound=bind_native_control_command_result(w,*receipt);if(!control_bound.ok)return control_bound;
    }
    // 本次实际新接册与已分配结果同候选绑定；不从裁剪历史或相邻ID猜原来源。
    if(kind==political_shop_registry_accept_command||!registry_acceptance_id.empty()){if(kind!=political_shop_registry_accept_command||registry_acceptance_id.empty()||!w.political_shop_registry_successions_enabled())return common_failure("PoliticalShopRegistryNewPlayerSourceRequired");auto bound=bind_political_shop_registry_player_receipt(w.common,w.political_shop_registry_successions,registry_acceptance_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}
    auto appointment_bound=bind_native_personal_appointment_commit_in_candidate(w,appointment_sources,actor,sequence,*receipt);if(!appointment_bound.ok)return appointment_bound;
    auto inquiry_bound=bind_contact_inquiry_result_world(w,inquiry_sources,actor,sequence,*receipt);if(!inquiry_bound.ok)return inquiry_bound;
    auto visit_reply_bound=bind_visit_reply_result_world(w,visit_reply_source,actor,sequence,*receipt);if(!visit_reply_bound.ok)return visit_reply_bound;
    auto born_dialogue_bound=bind_born_personal_dialogue_result_world(w,born_dialogue_source,actor,sequence,*receipt);if(!born_dialogue_bound.ok)return born_dialogue_bound;
    auto known_dialogue_bound=bind_known_personal_dialogue_result_world(w,known_dialogue_source,actor,sequence,*receipt);if(!known_dialogue_bound.ok)return known_dialogue_bound;
    auto contact_bound=bind_task_shop_order_contact_result_world(w,shop_order_contact_source,actor,sequence,*receipt);if(!contact_bound.ok)return contact_bound;
    auto returns_bound=bind_task_message_return_result_world(w,task_return_sources,actor,sequence,*receipt);if(!returns_bound.ok)return returns_bound;
    auto personal_bound=bind_task_personal_message_result_world(w,personal_message_sources,actor,sequence,*receipt);if(!personal_bound.ok)return personal_bound;
    auto report_bound=bind_task_personal_report_result_world(w,personal_report_sources,actor,sequence,*receipt);if(!report_bound.ok)return report_bound;
    auto acceptance_bound=bind_task_personal_acceptance_result_world(w,personal_acceptance_sources,actor,sequence,*receipt);if(!acceptance_bound.ok)return acceptance_bound;
    auto cancellation_bound=bind_task_personal_cancellation_result_world(w,personal_cancellation_source,actor,sequence,*receipt);if(!cancellation_bound.ok)return cancellation_bound;
    auto visit_bound=bind_native_visit_appointment_commit_in_candidate(w,visit_invitation_source,visit_meeting_sources,actor,sequence,*receipt);if(!visit_bound.ok)return visit_bound;
    auto messages_bound=bind_visit_message_result_world(w,visit_message_sources,actor,sequence,*receipt);if(!messages_bound.ok)return messages_bound;
    // 真正前后差分在零时死亡终止之后取；实际result入缓存后才绑定，先于Plan消费。
    auto lessons_bound=bind_growth_tactic_lesson_result_world(w,tactic_lesson_before,actor,sequence,*receipt,tactic_lesson_from_tick);if(!lessons_bound.ok)return lessons_bound;
    auto recovery_bound=bind_growth_recovery_result_world(w,recovery_before,actor,sequence,*receipt);if(!recovery_bound.ok)return recovery_bound;
    // 同一真实result发布前绑定本次新司法来源；不借NPC或后来同tick旧记录。
    if(!licensing_source_id.empty()||(w.catalog->political_shop_licensing_receipts&&political_shop_licensing_command_kind(kind))){if(!w.catalog->political_shop_licensing_receipts||!w.political_shop_licensing_receipts||licensing_source_id.empty()||!political_shop_licensing_command_kind(kind))return common_failure("PoliticalShopLicensingNewPlayerSourceRequired");auto bound=bind_political_shop_licensing_player_result(w.common,w.political_shop_licensing,*w.political_shop_licensing_receipts,licensing_source_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}
    if(w.private_shop_liquidations_enabled()&&!liquidation_before)return common_failure("PrivateShopLiquidationBeforeStateRequired");
    const PrivateShopLiquidationState empty_liquidation;auto liquidation_bound=bind_private_shop_liquidation_result_world(w,liquidation_before?*liquidation_before:empty_liquidation,actor,sequence,*receipt);if(!liquidation_bound.ok)return liquidation_bound;
    // 仅真实提交前后日租账差分可绑定，原Common结果和history已分配后才写永久来源。
    if(w.private_shop_daily_rent_enabled()&&!daily_rent_before)return common_failure("PrivateShopDailyRentBeforeStateRequired");const PrivateShopDailyRentState empty_daily_rent;auto daily_rent_bound=bind_private_shop_daily_rent_result_world(w,daily_rent_before?*daily_rent_before:empty_daily_rent,actor,sequence,*receipt);if(!daily_rent_bound.ok)return daily_rent_bound;
    auto estate_orders_bound=bind_task_shop_order_estate_result_world(w,estate_orders_before,actor,sequence,*receipt);if(!estate_orders_bound.ok)return estate_orders_bound;
    // 师承只绑定本请求前后真正新增的一份本人决定；历史裁剪不追认旧未绑定账。
    auto apprenticeship_bound=bind_family_apprenticeship_result_world(w,apprenticeship_before,actor,sequence,*receipt);if(!apprenticeship_bound.ok)return apprenticeship_bound;
    // 本次唯一新续办在真实Common结果已分配并入缓存后绑定，裁剪后不猜更早同tick来源。
    if(!governance_continuation_id.empty()||kind==political_governance_continue_command){if(kind!=political_governance_continue_command||!w.catalog->political_governance_continuations||!w.political_governance_continuations||governance_continuation_id.empty())return common_failure("GovernanceContinuationNewPlayerSourceRequired");auto bound=bind_political_governance_continuation_result(w.common,*w.political_governance_continuations,governance_continuation_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}
    if(!invitation_reply_source_id.empty()||is_governance_invitation_reply_command(kind)){if(!is_governance_invitation_reply_command(kind)||!w.catalog->political_governance_invitation_replies||!w.governance_invitation_replies||invitation_reply_source_id.empty())return common_failure("GovernanceInvitationNewPlayerSourceRequired");auto bound=bind_governance_invitation_player_result(w.common,*w.governance_invitation_replies,invitation_reply_source_id,actor,sequence,*receipt,[](const CommitResult& r){return r.details&&std::holds_alternative<Done>(r.details->data);});if(!bound.ok)return bound;}
    // 新弟子尾只绑定本次真实新增的原控制来源，先于Plan与最终完整史发布门。
    if(w.family_voluntary_paths_enabled()){if(!voluntary_before)return common_failure("VoluntaryPathBeforeStateRequired");auto bound=bind_family_voluntary_paths_result_world(w,voluntary_before->common.state().identity,&voluntary_before->family,&voluntary_before->family_voluntary_paths,actor,sequence,*receipt);if(!bound.ok)return bound;}else if(voluntary_before)return common_failure("VoluntaryPathProfileRequired");
    auto collateral_bound=bind_family_collateral_result_world(w,collateral_legacy_before,collateral_before,actor,sequence,*receipt);if(!collateral_bound.ok)return collateral_bound;
    auto disciple_bound=bind_family_disciple_legacy_result_world(w,disciple_legacy_before,disciple_bindings_before,actor,sequence,*receipt);if(!disciple_bound.ok)return disciple_bound;
    auto biological_bound=bind_family_biological_result_world(w,biological_legacy_before,biological_bindings_before,actor,sequence,*receipt);if(!biological_bound.ok)return biological_bound;
    if(!political_succession_source_id.empty()||is_political_succession_command(kind)){auto bound=bind_political_succession_result_world(w,political_succession_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_head_handover_source_id.empty()||is_political_head_handover_command(kind)){auto bound=bind_political_head_handover_result_world(w,political_head_handover_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_successor_round_source_id.empty()||is_political_successor_round_command(kind)){auto bound=bind_political_successor_round_result_world(w,political_successor_round_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_term_handover_source_id.empty()||is_political_term_handover_command(kind)){auto bound=bind_political_term_handover_result_world(w,political_term_handover_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_office_reception_source_id.empty()||is_political_office_reception_command(kind)){auto bound=bind_political_office_reception_result_world(w,political_office_reception_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_office_resignation_source_id.empty()||is_political_office_resignation_command(kind)){auto bound=bind_political_office_resignation_result_world(w,political_office_resignation_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!subordinate_appointment_source_id.empty()||is_political_subordinate_appointment_command(kind)){auto bound=bind_political_subordinate_appointment_result_world(w,subordinate_appointment_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!dismissal_source_id.empty()||(w.catalog->political_dismissal_sources&&kind=="political_dismiss")){auto bound=bind_political_dismissal_result_world(w,dismissal_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    if(!political_governance_agency_source_id.empty()||is_political_governance_agency_command(kind)){auto bound=bind_political_governance_agency_result_world(w,political_governance_agency_source_id,actor,sequence,*receipt);if(!bound.ok)return bound;}
    auto ia=w.services.interaction_access();auto observed=reconcile_events(w.common,w.interaction,*w.catalog->interaction,ia);if(!observed.ok)return observed;
    // 出生、继任、死亡及活动开始/结束在同次候选提交内刷新生命周期，不等另一次tick。
    if(w.dynamic_enabled()&&!complete_exact){auto registered=reconcile_dynamic_director_registry(w);if(!registered.ok)return registered;}
    // 当面邀请与本人决定的参与者在同次命令内获知；不额外推进周期或投递远方消息。
    if(w.named_membership_enabled()||(w.dynamic_enabled()&&w.catalog->director->dynamic_subjects)){auto reports=collect_dynamic_reports(w.common,w.social,w.services,w.common.tick(),w.catalog->director.get(),&w.director);if(!reports.ok)return reports;}
    {auto collected=collect_exact_social_in_candidate(w);if(!collected.ok)return collected;}
    // 全部真实Shop写者与实际Common结果已完成，先绑定后让Plan消费；before只活到本次返回。
    auto shop_bound=bind_private_shop_command_results_world(shop_command_before,w,actor,sequence,*receipt);if(!shop_bound.ok)return shop_bound;
    if(plan_execution||!player_plan_command_kind(kind)){auto plan=finish_player_plan_commit(w,actor,plan_execution,details,receipt->result_id);if(!plan.ok)return plan;}
    if(actual_capture){
        world_stage_observation::Scope settle_observation(world_stage_observation::Stage::capture_settle);
        auto adopted=actual_capture->settle();settle_observation.stop();if(!adopted.ok)return adopted;
        auto released=bind_complete_growth_release_result(shop_command_before,w,actor,sequence,*receipt);if(!released.ok)return released;
        // Idempotent for the same actual seal. A genuinely new postseal death
        // is rejected by original revision/order checks, never assigned an old seal.
        const SourceRef sealed_deaths{"common","command_result",receipt->result_id};
        auto deaths_bound=actual_capture->bind_interaction_session_death_result(sealed_deaths);if(!deaths_bound.ok)return deaths_bound;
        world_stage_observation::Scope finish_observation(world_stage_observation::Stage::capture_finish);
        auto closed=actual_capture->finish();finish_observation.stop();if(!closed.ok)return closed;
    }
    if(complete_exact){
        // All actual result/source bindings and live input consumers precede
        // this pending-wake adoption. Restore-time rebuild cannot replace it.
        auto wakes=collect_native_complete_director_wakes_in_candidate(w);if(!wakes.ok)return wakes;
        auto retained=prune_complete_command_sources(w);if(!retained.ok)return retained;
    }
    // 本人任务携行页也服务旧profile；派生ID索引不改变原观察query的能力门。
    w.observation_index=build_world_observation_index(w.common,w.social,w.tasks);
    // 页索引绑定最终revision；冷恢复的epoch另在安装发布前重建。
    w.map_presence_index=build_map_presence_index(w.common);
    w.politics_view_index=make_politics_view_index(w.common,w.politics,w.organization,w.catalog->organization?*w.catalog->organization:OrganizationCatalog{},w.social,&w.political_fiscal);
    w.governance_view_index=make_governance_view_index(w.common,w.politics_governance,w.tasks,w.governance_tasks);w.rebuild_diplomacy_index();
    w.economic_index=rebuild_economic_world_index(w);w.fiscal_index=rebuild_economic_fiscal_index(w);w.household_provision_index=rebuild_household_provision_index(w);
    const auto validation_start=std::chrono::steady_clock::now();world_stage_observation::Scope validate_observation(world_stage_observation::Stage::commit_tail_validate);auto valid=w.validate();validate_observation.stop();w.timing.emplace_back("profile_validate",std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-validation_start).count());if(!valid.ok)return valid;result=std::move(receipt);return common_success();
}
Outcome<Bytes> ResidentWorld::checkpoint(){if(!impl_->world)return failure<Bytes>("NoWorld");return encode_world_checkpoint(*impl_->world);}
Status ResidentWorld::restore(const Bytes& bytes,std::shared_ptr<const PreparedCatalog> catalog,const std::optional<ContinuationSnapshot>& continuation){
    WorldCheckpointWriteScope checkpoint_write(active_storage_capture());
    if(storage_restore_pending())return common_failure("StorageRestoreBusy");
    if(!catalog)return common_failure("NoCatalog");checkpoint_phase_timing::Scope decode_time(checkpoint_phase_timing::Stage::restore_decode);auto seed=decode_world_checkpoint(bytes,*catalog);decode_time.stop();if(!seed.ok)return {false,{},seed.error};checkpoint_phase_timing::Scope construct_time(checkpoint_phase_timing::Stage::restore_construct);auto ready=seed_ready(catalog.get(),seed.value);if(!ready.ok)return ready;
    seed.value.common.identity.execution_epoch=0;auto candidate=checkpoint_make_shared<WorldBundle>(std::move(catalog),std::move(seed.value));
    auto hydrated=restore_native_transport_derived_values_in_candidate(*candidate);if(!hydrated.ok)return hydrated;
    auto cache=restore_native_complete_director_wake_cache_in_candidate(*candidate);if(!cache.ok)return cache;
    construct_time.stop();
    checkpoint_phase_timing::Scope validation_time(checkpoint_phase_timing::Stage::restore_validate);
    auto valid=candidate->validate();validation_time.stop();if(!valid.ok)return valid;
    // 冷恢复还验证导出边界元数据，拒绝合法checksum下的重复/缺失字段序；热submit不执行全域export。
    checkpoint_phase_timing::Scope export_time(checkpoint_phase_timing::Stage::restore_export);
    auto exportable=export_world_state(*candidate);export_time.stop();if(!exportable.ok)return {false,{},exportable.error};
    if(continuation){auto bound=validate_continuation_snapshot(*candidate,*continuation);if(!bound.ok)return bound;}
    checkpoint_phase_timing::Scope publish_time(checkpoint_phase_timing::Stage::restore_publish);
    auto epoch=allocate_execution_epoch();if(!epoch.ok)return {false,{},epoch.error};candidate->common.state().identity.execution_epoch=epoch.value;candidate->map_presence_index=build_map_presence_index(candidate->common);candidate->economic_index=rebuild_economic_world_index(*candidate);candidate->fiscal_index=rebuild_economic_fiscal_index(*candidate);candidate->household_provision_index=rebuild_household_provision_index(*candidate);candidate->politics_view_index=make_politics_view_index(candidate->common,candidate->politics,candidate->organization,candidate->catalog->organization?*candidate->catalog->organization:OrganizationCatalog{},candidate->social,&candidate->political_fiscal);candidate->governance_view_index=make_governance_view_index(candidate->common,candidate->politics_governance,candidate->tasks,candidate->governance_tasks);candidate->rebuild_diplomacy_index();
    CheckpointUniquePtr<ContinuationSnapshot> stopped;if(continuation)stopped=checkpoint_make_unique<ContinuationSnapshot>(*continuation);if(stopped){stopped->source_identity=candidate->common.state().identity;stopped->stop=RunStop::restored;}
    impl_->world=std::move(candidate);impl_->run.reset();impl_->plan_run.reset();impl_->run_spend_guard.reset();impl_->restored_continuation.reset();impl_->restored_continuation_memory={};impl_->restored_continuation=std::move(stopped);impl_->used_run_ids.clear();impl_->fail_after_full_advance_once=false;return common_success();
}

// 只采纳原decoder拥有并全验的候选；所有可抛分配都在最后取消CAS之前。
CheckpointJobResult<WorldIdentity> ResidentWorld::install_checkpoint_candidate(WorldRestoreCandidate&& candidate,const std::optional<WorldIdentity>& expected,WorldInstallCancellation& cancellation,CheckpointJobBudget& job){
    try{
        auto retained=impl_->active_storage_restore.lock();
        if(!retained||!retained->belongs_to(job)||retained->world_.get()!=impl_->world.get())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        if(!candidate.validated()||candidate.consumed_||!candidate.memory_.belongs_to(job))return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::reservation_mismatch)};
        const auto same=[](const WorldIdentity&a,const WorldIdentity&b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;};
        if(impl_->world){if(!expected||!same(*expected,impl_->world->common.state().identity))return {false,{},job.capture_failure(common_failure("StaleWorld").error)};}
        else if(expected)return {false,{},job.capture_failure(common_failure("StaleWorld").error)};
        if(cancellation.cancelled())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
        auto resource=checkpoint_memory_resource(job,CheckpointJobUse::candidate);if(!resource.ok)return {false,{},std::move(resource.error)};ScopedCheckpointMemory scope(resource.value);
        auto epoch=allocate_execution_epoch();if(!epoch.ok)return {false,{},job.capture_failure(std::move(epoch.error))};
        auto& next=*candidate.world_;next.common.state().identity.execution_epoch=epoch.value;
        // 派生索引必须取新epoch，不能移用checkpoint外热stamp或旧世界索引。
        next.map_presence_index=build_map_presence_index(next.common);next.economic_index=rebuild_economic_world_index(next);next.fiscal_index=rebuild_economic_fiscal_index(next);next.household_provision_index=rebuild_household_provision_index(next);next.politics_view_index=make_politics_view_index(next.common,next.politics,next.organization,next.catalog->organization?*next.catalog->organization:OrganizationCatalog{},next.social,&next.political_fiscal);next.governance_view_index=make_governance_view_index(next.common,next.politics_governance,next.tasks,next.governance_tasks);next.rebuild_diplomacy_index();
        if(candidate.continuation_){candidate.continuation_->source_identity=next.common.state().identity;candidate.continuation_->stop=RunStop::restored;}
        if(next.checkpoint_candidate_memory.valid()){auto merged=next.checkpoint_candidate_memory.merge(std::move(candidate.memory_));if(!merged.ok)return {false,{},std::move(merged.error)};}else next.checkpoint_candidate_memory=std::move(candidate.memory_);
        const auto identity=next.common.state().identity;auto published=std::move(candidate.world_);
        // shared控制块、索引及所有诊断均已准备；之后只做noexcept拥有权转移。
        auto live=job.reserve(CheckpointJobUse::candidate,0);if(!live.ok)return {false,{},std::move(live.error)};
        if(!cancellation.begin())return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::cancelled)};
        impl_->world=std::move(published);impl_->run.reset();impl_->plan_run.reset();impl_->run_spend_guard.reset();impl_->restored_continuation.reset();impl_->restored_continuation_memory=std::move(candidate.continuation_memory_);impl_->restored_continuation=std::move(candidate.continuation_);impl_->used_run_ids.clear();impl_->fail_after_full_advance_once=false;candidate.consumed_=true;candidate.validated_=false;cancellation.finish();
        return {true,WorldIdentity(identity),{}};
    }catch(checkpoint_job_detail::FailureException& error){return {false,{},std::move(error.error)};}catch(const std::bad_alloc&){return {false,{},job.capture_fixed_failure(CheckpointJobFailureCode::allocation_failed)};}
}
Outcome<std::shared_ptr<const JourneyViewPage>> ResidentWorld::journey_view(const StableId& actor,Integer cursor)const{
    if(!impl_->world)return failure<std::shared_ptr<const JourneyViewPage>>("NoWorld");const auto& w=*impl_->world;auto p=w.common.actor(actor);if(!p)return failure<std::shared_ptr<const JourneyViewPage>>("UnknownActor");
    const auto* action=w.common.action(p->action_id);Integer count=action?1:0;return make_journey_page(w.common.state().identity,actor,cursor,count,[&](Integer)->Outcome<JourneyViewRow>{JourneyViewRow row;row.id=action->id;row.kind=action->kind;row.text=std::to_string(action->remaining_ticks);if(auto* loc=std::get_if<WildernessLocation>(&p->location))row.map_position=loc->point;return {true,std::move(row),{}};});
}
} // begin/resume/poll/cancel另在同单worker许可控制器实现，不引入第二个世界。
