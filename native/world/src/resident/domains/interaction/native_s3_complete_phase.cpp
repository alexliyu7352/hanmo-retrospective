#include "native_s3_complete_phase.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/interaction/communication_links.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/social/social_period_sources.hpp"
#include "resident/domains/social/social_time2.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
#include <string_view>
#include <type_traits>

namespace hanmo::resident {
namespace {
// 专业 Probe 与拘押请求是原合同独立模块，不等同 AskNews、普通问讯或军务看管。
bool unselected_kind(std::string_view kind) {
  return kind == "intelligence_probe" ||
         kind.starts_with("intelligence_probe_") ||
         kind.starts_with("probe_") || kind == "custody_contact" ||
         kind.starts_with("custody_contact_") ||
         kind.starts_with("custody_request") ||
         kind.starts_with("custody_audience");
}
bool unselected_command(std::string_view kind) {
  return unselected_kind(kind) || kind == "begin_probe" ||
         kind == "cancel_probe" || kind == "request_custody_contact" ||
         kind == "queue_custody_contact_request" ||
         kind == "begin_custody_audience" || kind == "reply_custody_request" ||
         kind == "answer_custody_proposal" ||
         kind == "withdraw_custody_request" || kind == "BeginProbe" ||
         kind == "CancelProbe" || kind == "RequestCustodyContact" ||
         kind == "QueueCustodyContactRequest" ||
         kind == "BeginCustodyAudience" || kind == "ReplyCustodyRequest" ||
         kind == "AnswerCustodyProposal" || kind == "WithdrawCustodyRequest";
}
bool unselected_source(const SourceRef &source) {
  return source.domain == "custody_contact" ||
         source.domain == "intelligence_probe" || unselected_kind(source.kind);
}
Status absent_source(const SourceRef &source) {
  return unselected_source(source)
             ? common_failure("UnselectedS3ProfessionalSource", source.id,
                              ErrorKind::internal)
             : common_success();
}
Status calendar_boundary(const WorldBundle &w, const ExactPeriodContext &p,
                         const ExactPhaseWriteContext &writes) {
  if (!w.world_timeline || p.period_id.empty() ||
      w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      w.world_timeline->period_id != p.period_id ||
      w.active_exact_write_context() != &writes ||
      !w.common.actor_condition_capture_identity() ||
      p.opening_at.half_day_tick != w.common.tick() ||
      p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      p.through_tick != p.opening_at.half_day_tick + 1)
    return common_failure("S3OriginalCalendarBoundaryRequired", p.period_id,
                          ErrorKind::internal);
  auto integral = compare_fractions(p.opening_at.sub_half_day, {});
  if (!integral.ok)
    return {false, {}, integral.error};
  if (integral.value)
    return common_failure("S3OriginalCalendarBoundaryRequired", p.period_id,
                          ErrorKind::internal);
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value, WorldMoment{p.through_tick, {}});
  if (!same.ok)
    return {false, {}, same.error};
  return same.value ? common_failure("S3OriginalCalendarMomentMismatch",
                                     p.period_id, ErrorKind::internal)
                    : common_success();
}
// 只核原 due writer 已提交的消息及本人知识，不再次调用递送、收信或回信命令。
Status reply_receipt_barrier(const WorldBundle &w,
                             const ExactPeriodContext &p) {
  if (!w.catalog || !w.social_period_sources)
    return common_failure("S3OriginalReplyReceiptSourcesRequired", p.period_id,
                          ErrorKind::internal);
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto valid = validate_social_due_period_completion(
      w.common, w.social, *w.social_period_sources, p.period_id, now.value,
      w.catalog->identity);
  if (!valid.ok)
    return valid;
  // INT-3 当面投递/本人回复由原命令产生，此处只反读其原结果和未选反覆盖。
  valid = validate_visit_message_world(w);
  if (!valid.ok)
    return valid;
  valid = validate_visit_message_receipt_world(w);
  if (!valid.ok)
    return valid;
  valid = validate_visit_reply_world(w);
  if (!valid.ok)
    return valid;
  auto original = capture_exact_result_time_read(w);
  if (!original.ok)
    return {false, {}, original.error};
  return validate_social_time2(w.common, w.social, *original.value,
                               original.value->stamp(), w.catalog->identity);
}
} // namespace

Status validate_native_s3_unselected_phase_sources(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->interaction || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time))
    return common_failure("S3CompleteOriginalSelectionRequired", {},
                          ErrorKind::unsupported_capability);
  for (const auto &module : w.catalog->identity.module_versions)
    if (module.first == "custody_contact" ||
        module.first == "intelligence_probe")
      return common_failure("NOT_IMPLEMENTED", module.first,
                            ErrorKind::unsupported_capability);
  for (const auto &action :
       w.catalog->exact_time->complete->action_time_profiles)
    if (unselected_command(action.action_kind) ||
        action.profile_id == "intelligence_probe" ||
        action.profile_id == "custody_contact")
      return common_failure("NOT_IMPLEMENTED", action.action_kind,
                            ErrorKind::unsupported_capability);
  // 目前 Interaction/World 结构没有这两专业 owning 槽。若日后安装，必须同时
  // 替换此明确未选组合；不得拿当前没有头或没有消息当作已实现专业来源。
  for (const auto &row : w.common.state().facts.rows())
    if (row.second.domain == "custody_contact" ||
        row.second.domain == "intelligence_probe" ||
        unselected_kind(row.second.kind))
      return common_failure("UnselectedS3ProfessionalFact", row.first,
                            ErrorKind::internal);
  for (const auto &row : w.common.state().actions.rows())
    if (unselected_command(row.second.kind))
      return common_failure("UnselectedS3ProfessionalAction", row.first,
                            ErrorKind::internal);
  for (const auto &row : w.common.state().history)
    if (unselected_command(row.kind))
      return common_failure("UnselectedS3ProfessionalHistory", row.result_id,
                            ErrorKind::internal);
  for (const auto &row : w.common.state().results.rows())
    if (row.second && unselected_command(row.second->kind))
      return common_failure("UnselectedS3ProfessionalResult", row.first,
                            ErrorKind::internal);
  for (const auto &dialogue : w.catalog->interaction->dialogues)
    for (const auto &node : dialogue.nodes)
      if (node.command && unselected_command(node.command->kind))
        return common_failure("NOT_IMPLEMENTED", node.command->kind,
                              ErrorKind::unsupported_capability);
  if (w.interaction.communication_links) {
    const auto &links = *w.interaction.communication_links;
    for (const auto &row : links.links.rows()) {
      auto valid = absent_source(row.second.original_business_fact_ref);
      if (!valid.ok)
        return valid;
      for (const auto &ref : row.second.route_source_refs) {
        valid = absent_source(ref);
        if (!valid.ok)
          return valid;
      }
      for (const auto &ref : row.second.dossier_source_refs) {
        valid = absent_source(ref);
        if (!valid.ok)
          return valid;
      }
      if (row.second.cancellation_source) {
        valid = absent_source(row.second.cancellation_source->source_ref);
        if (!valid.ok)
          return valid;
      }
    }
    for (const auto &row : links.deliveries.rows()) {
      auto valid = absent_source(row.second.arrival_ref);
      if (!valid.ok)
        return valid;
      valid = absent_source(row.second.authority_source);
      if (!valid.ok)
        return valid;
    }
    for (const auto &row : links.dossier_accesses.rows()) {
      auto valid = absent_source(row.second.original_access_ref);
      if (!valid.ok)
        return valid;
      valid = absent_source(row.second.authority_source);
      if (!valid.ok)
        return valid;
    }
  }
  if (w.social.dynamic) {
    const auto &social = *w.social.dynamic;
    for (const auto &row : social.reports.rows())
      if (unselected_kind(row.second.payload_kind) ||
          unselected_kind(row.second.observation.kind))
        return common_failure("UnselectedS3ProfessionalReport", row.first,
                              ErrorKind::internal);
    for (const auto &row : social.pending_messages.rows())
      if (row.second.address_binding) {
        auto valid = absent_source(
            row.second.address_binding->original_business_fact_ref);
        if (!valid.ok)
          return valid;
      }
    if (social.time2) {
      for (const auto &row : social.time2->source_time_bindings.rows()) {
        auto valid = absent_source(row.second.actual_source_ref);
        if (!valid.ok)
          return valid;
        if (const auto *source =
                std::get_if<SocialExactSource>(&row.second.time_source)) {
          valid = absent_source(source->original_timed_source_ref.source_ref);
          if (!valid.ok)
            return valid;
        }
        if (row.second.learned_after_common_fact) {
          valid = absent_source(*row.second.learned_after_common_fact);
          if (!valid.ok)
            return valid;
        }
      }
      for (const auto &row : social.time2->message_execution_bindings.rows()) {
        if (const auto *source =
                std::get_if<SocialS3BusinessLink>(&row.second.source_binding)) {
          auto valid = absent_source(source->original_business_fact_ref);
          if (!valid.ok)
            return valid;
        }
        for (const auto *source : {&row.second.actual_delivery_ref,
                                   &row.second.actual_dossier_access_ref})
          if (*source) {
            auto valid = absent_source(**source);
            if (!valid.ok)
              return valid;
          }
      }
      for (const auto &row : social.time2->propagation_decisions.rows())
        for (const auto &ref : row.second.causal_source_refs) {
          auto valid = absent_source(ref);
          if (!valid.ok)
            return valid;
        }
    }
  }
  if (w.action_elapsed) {
    for (const auto &row : w.action_elapsed->terminal_bindings) {
      if (unselected_command(row.action_kind))
        return common_failure("UnselectedS3ProfessionalTerminal",
                              row.action_ref, ErrorKind::internal);
      auto valid = absent_source(row.original_execution_source_ref);
      if (!valid.ok)
        return valid;
      for (const auto *time :
           {&row.started_source_ref, &row.ended_source_ref}) {
        valid = std::visit(
            [](const auto &source) { return absent_source(source.source_ref); },
            *time);
        if (!valid.ok)
          return valid;
      }
    }
    for (const auto &row : w.action_elapsed->fixed_service_transitions) {
      auto valid = absent_source(row.execution_source_ref);
      if (!valid.ok)
        return valid;
      valid = std::visit(
          [](const auto &cause) {
            using T = std::decay_t<decltype(cause)>;
            if constexpr (std::is_same_v<T, FixedOwnerTerminated>)
              return absent_source(cause.owner_result_ref);
            else if constexpr (std::is_same_v<T, FixedEligibilityChanged>)
              return absent_source(cause.original_cause_ref);
            else
              return absent_source(cause.owner_source_ref);
          },
          row.cause);
      if (!valid.ok)
        return valid;
      for (const auto *inputs :
           {&row.input_basis_refs, &row.previous_input_basis_refs})
        for (const auto &input : *inputs)
          if (const auto *source = std::get_if<FixedAppliedInput>(&input)) {
            valid = absent_source(source->source_ref);
            if (!valid.ok)
              return valid;
          }
    }
  }
  if (w.director.subject2) {
    for (const auto &row : w.director.subject2->wake_sources.rows()) {
      auto valid = absent_source(row.second.wake.actual_source);
      if (!valid.ok)
        return valid;
    }
    for (const auto &row : w.director.subject2->subject_plan_bindings.rows()) {
      auto valid = absent_source(row.second.policy_source_ref);
      if (!valid.ok)
        return valid;
      if (row.second.continuation_ref) {
        valid = absent_source(*row.second.continuation_ref);
        if (!valid.ok)
          return valid;
      }
      valid = std::visit(
          [](const auto &binding) {
            using T = std::decay_t<decltype(binding)>;
            Status valid;
            if constexpr (std::is_same_v<T, ActorExecutionBinding>)
              valid = absent_source(binding.original_business_ref);
            else if constexpr (std::is_same_v<T, CohortPurchaseBinding>)
              valid = absent_source(binding.purchase_plan_ref);
            else if constexpr (std::is_same_v<T, CohortHouseholdRoutineBinding>)
              valid = absent_source(binding.need_source_ref);
            else if constexpr (std::is_same_v<T, InstitutionalExecutionBinding>)
              valid = absent_source(binding.approved_responsibility_ref);
            else
              return absent_source(binding.approved_goal_ref);
            if (!valid.ok)
              return valid;
            if constexpr (!std::is_same_v<T, AwaitOfficerBinding>)
              if (binding.execution_ref)
                return absent_source(*binding.execution_ref);
            return common_success();
          },
          row.second.binding);
      if (!valid.ok)
        return valid;
    }
    for (const auto &row : w.director.subject2->subject_continuations.rows()) {
      auto valid = absent_source(row.second.original_continuation_ref);
      if (!valid.ok)
        return valid;
    }
    for (const auto &row : w.director.subject2->adoption_results.rows()) {
      for (const auto &ref : row.second.actual_outcome_refs) {
        auto valid = absent_source(ref);
        if (!valid.ok)
          return valid;
      }
      if (row.second.accepted_command) {
        const auto &command = *row.second.accepted_command;
        if (unselected_command(command.command_kind))
          return common_failure("UnselectedS3ProfessionalAcceptedCommand",
                                command.result_id, ErrorKind::internal);
        auto valid = absent_source(command.actual_effect.source_ref);
        if (!valid.ok)
          return valid;
      }
    }
  }
  return common_success();
}

Status reject_native_custody_contact_request(const WorldBundle &w) {
  auto valid = validate_native_s3_unselected_phase_sources(w);
  return valid.ok ? common_failure("NOT_IMPLEMENTED", "custody_contact",
                                   ErrorKind::unsupported_capability)
                  : valid;
}
Status reject_native_intelligence_probe_request(const WorldBundle &w) {
  auto valid = validate_native_s3_unselected_phase_sources(w);
  return valid.ok ? common_failure("NOT_IMPLEMENTED", "intelligence_probe",
                                   ErrorKind::unsupported_capability)
                  : valid;
}

ExactPhaseRegistration make_native_s3_complete_phase_registration(
    Integer initial_tick, SocialCalendarDispatchFunction dispatch) {
  auto registration =
      make_social_exact_phase_registration(initial_tick, dispatch);
  for (const auto stage :
       {ExactStage::probe_window_close, ExactStage::probe_next_open,
        ExactStage::reply_receipts,
        ExactStage::custody_response_deadline_close}) {
    registration.stages.push_back(stage);
    registration.calendar_stages.push_back(stage);
  }
  registration.causal_edges.push_back(
      {ExactStage::due_message_receipt, ExactStage::reply_receipts});
  registration.validate_author_selection = [initial_tick](
                                               const WorldBundle &w) {
    auto valid = validate_native_s3_unselected_phase_sources(w);
    return valid.ok ? validate_original_social_phase_selection(w, initial_tick)
                    : valid;
  };
  registration.validate_world_sources = [initial_tick](const WorldBundle &w,
                                                       const WorldMoment &at) {
    auto valid = validate_native_s3_unselected_phase_sources(w);
    return valid.ok
               ? validate_original_social_phase_sources(w, at, initial_tick)
               : valid;
  };
  registration.apply_calendar_stage = [dispatch](
                                          WorldBundle &w, ExactStage stage,
                                          const ExactPeriodContext &p,
                                          ExactPhaseWriteContext &writes) {
    auto valid = validate_native_s3_unselected_phase_sources(w);
    if (!valid.ok)
      return valid;
    if (stage == ExactStage::probe_window_close ||
        stage == ExactStage::probe_next_open ||
        stage == ExactStage::reply_receipts ||
        stage == ExactStage::custody_response_deadline_close) {
      valid = calendar_boundary(w, p, writes);
      if (!valid.ok)
        return valid;
      return stage == ExactStage::reply_receipts ? reply_receipt_barrier(w, p)
                                                 : common_success();
    }
    return apply_original_social_calendar_stage(w, stage, p, writes, dispatch);
  };
  return registration;
}
} // namespace hanmo::resident
