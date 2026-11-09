// clang-format off
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/military/field_battle/field_view_model.hpp"
#include "resident/domains/military/field_battle/field_world.hpp"
#include "resident/domains/military/field_battle/field_result_codec.hpp"
#include "resident/domains/politics/licensing/political_shop_licensing_owned.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/family/child_care_relief_world.hpp"
#include "resident/domains/family/family_birth_world.hpp"
#include "resident/domains/family/voluntary_path_payloads.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_world.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply_world.hpp"
#include "resident/domains/military/military_field_command_profile.hpp"
#include "resident/domains/military/military_field_command_rules.hpp"
#include "resident/domains/military/military_officer_codec.hpp"
#include "resident/domains/military/military_officer_profile.hpp"
#include "resident/domains/military/military_officer_rules.hpp"
#include "resident/domains/military/military_tactic_practice_codec.hpp"
#include "resident/domains/tasks/extensions/task_extension_world.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_boundary.hpp"
#include "resident/domains/tasks/message_returns/task_message_return_world.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_boundary.hpp"
#include "resident/domains/tasks/personal_acceptances/task_personal_acceptance_world.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_boundary.hpp"
#include "resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_boundary.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_world.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_boundary.hpp"
#include "resident/domains/tasks/personal_reports/task_personal_report_world.hpp"
#include "resident/kernel/native_work_lifecycle_command_route.hpp"
#include "resident/kernel/world_timeline_codec.hpp"
// 中央唯一冷命令/报价/回执marshal；只转换typed值，不执行领域guard、规则或第二提交。
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/completion_codec.hpp"
#include "resident/checkpoint/resident_receipt_projection.hpp"
#include "resident/domains/family/family_effect_projection.hpp"
#include "resident/domains/growth/growth_effect_projection.hpp"
#include "resident/domains/growth/growth_tactic_lesson_boundary.hpp"
#include "resident/domains/interaction/interaction_actions.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/local_economy/supply/supply_policy_profile.hpp"
#include "resident/domains/military/battle_codec.hpp"
#include "resident/domains/military/battle_request_codec.hpp"
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/military_tactical_ownership_codec.hpp"
#include "resident/domains/military/training_codec.hpp"
#include "resident/domains/personal/personal_effect_projection.hpp"
#include "resident/domains/player_plan/plan_codec.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_profile.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/dynamic_integration.hpp"
#include "resident/kernel/named_membership.hpp"
#include "resident/kernel/organization_grant_route.hpp"
#include "resident/kernel/siege_integration.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <type_traits>

// clang-format on
namespace hanmo::resident {
// 原域writer的只写字段口；定义由原owner同其JSON encode成对提供。
Status emit_task_effect_fields(ProjectionWriter &, const TaskEffect &);
Status emit_interaction_extra_effect_fields(ProjectionWriter &,
                                            const InteractionEffect &);
Status emit_estate_receipt_fields(ProjectionWriter &, const EstateReceipt &);
Status emit_politics_effects_fields(ProjectionWriter &,
                                    const PoliticsEffects &);
Status emit_political_fiscal_effects_fields(ProjectionWriter &,
                                            const PoliticalFiscalEffects &);
Status emit_economic_configuration_effects_fields(ProjectionWriter &,
                                                  const LocalEconomyEffects &);
Status emit_household_provision_management_effects_fields(
    ProjectionWriter &, const HouseholdProvisionManagementEffects &);
namespace military_request_codec {
Status emit_answer_fields(ProjectionWriter &, const MilitaryRequestAnswer &);
}
namespace training_codec {
Status emit_fields(ProjectionWriter &, const MilitaryTrainingReceipt &);
}
namespace battle_codec {
Status emit_fields(ProjectionWriter &, const MilitaryActorChange &);
}
namespace naval_repair_codec {
Status emit_receipt_fields(ProjectionWriter &, const NavalRepairReceipt &);
}
namespace naval_command_codec {
Status emit_receipt_fields(ProjectionWriter &, const NavalCommandReceipt &);
}
namespace naval_battle_command_codec {
Status emit_receipt_fields(ProjectionWriter &,
                           const NavalBattleCommandReceipt &, Integer);
}
namespace {
using V = BoundaryValue;
using D = V::Dict;
using A = V::Array;
// 复用原 Field 回执分支；新末尾命令不借普通 military 明细或第二种结果封套。
bool field_receipt_kind(const StableId &kind) {
  return kind == "FieldArmyExitSelect" || field_deployment_command_kind(kind);
}
// 新四命令复用原政治明细，但只允许其唯一事实和正式就任原Office来源。
bool valid_political_succession_effect(const StableId &kind,
                                       const PoliticsEffects &e) {
  if (!is_political_succession_command(kind) || !e.appointment_id.empty() ||
      e.fact_ids.size() != 1 || e.fact_ids.front().empty())
    return false;
  return kind == political_succession_accede_command
             ? e.receipt_ids.size() == 1 && !e.receipt_ids.front().empty()
             : e.receipt_ids.empty();
}
// 输出无损性失败是内部/能力错误；禁止吞未知字段后生成看似完整的回执。
struct MarshalFailure {
  RuleError error;
};
[[noreturn]] void failed(const StableId &code, const StableId &path,
                         ErrorKind kind = ErrorKind::internal) {
  throw MarshalFailure{{{code, path}, kind, {}, {}, {}, {}}};
}
V ordered(V value, const FieldOrder &keys) {
  auto status = apply_field_order(value, keys);
  if (!status.ok)
    failed(status.error.original.code, status.error.original.path);
  return value;
}
FieldOrder order(const V &value) {
  auto result = capture_field_order(value);
  if (!result.ok)
    throw MarshalFailure{result.error};
  return std::move(result.value);
}
bool had(const FieldOrder &keys, const StableId &key) {
  return std::find(keys.begin(), keys.end(), key) != keys.end();
}
// 缓存结果仍核命令对应的有限形状，空明细或另一命令来源不能冒充真实整编结果。
bool valid_integration_effect(const StableId &kind,
                              const SiegeIntegrationEffect &e) {
  if (e.proposal_id.empty() || e.proposal_key.empty())
    return false;
  if (kind == "siege_integration_apply")
    return e.status == "completed" && !e.completion_source_id.empty() &&
           e.actor_action_source_id.empty() && e.decision_source_id.empty();
  if (!e.completion_source_id.empty() || e.actor_action_source_id.empty())
    return false;
  if (kind == "siege_integration_decide")
    return !e.decision_source_id.empty() &&
           (e.status == "pending" || e.status == "rejected");
  if (!e.decision_source_id.empty())
    return false;
  if (kind == "siege_integration_cancel")
    return e.status == "cancelled";
  return (kind == "siege_integration_propose" || kind == "siege_strategy") &&
         e.status == "pending";
}
// 本人回应和入职明细有唯一命令归属；空明细及跨命令variant不能充当已接受事实。
bool valid_membership_effect(const StableId &kind,
                             const MembershipOfferEffects &e) {
  if (!named_membership_command_kind(kind) || e.offer_id.empty() ||
      e.fact_ids.empty() || e.receipt_ids.empty())
    return false;
  auto unique = [](const std::vector<StableId> &ids) {
    if (ids.size() > 64)
      return false;
    for (std::size_t i = 0; i < ids.size(); ++i) {
      if (ids[i].empty())
        return false;
      for (std::size_t j = 0; j < i; ++j)
        if (ids[i] == ids[j])
          return false;
    }
    return true;
  };
  if (!unique(e.fact_ids) || !unique(e.receipt_ids))
    return false;
  if (kind == "organization_answer_membership")
    return !e.decision_id.empty();
  return e.decision_id.empty() && e.member_episode_id.empty();
}
bool valid_request_effect(const MilitaryEffect &e) {
  return e.request_answer && e.receipt_id == e.request_answer->receipt_id &&
         e.battle_id == e.request_answer->battle_id &&
         e.already_completed.has_value() && e.action_id.empty() &&
         !e.training_receipt && e.completed_receipt_ids.empty() &&
         e.fact_id.empty() && e.source_id.empty() && !e.return_wait_reason &&
         e.siege_id.empty() && !e.plan_submitted && e.actor_changes.empty() &&
         e.journey_requests.empty() && e.success && e.code.empty() &&
         e.office_id.empty() && e.army_id.empty() && e.order_id.empty() &&
         e.campaign_id.empty() && !e.details_ok && !e.journey_request &&
         !e.integration_request;
}
// 原普通错误只含ok/code；native能力定位独立添加，不替换旧错误类别。
V error_value(const RuleError &error) {
  D out{{"ok", {false}}, {"code", {error.original.code}}};
  if (error.kind != ErrorKind::rule) {
    out.emplace_back("phase", V{error.phase});
    out.emplace_back("domain", V{error.domain});
    out.emplace_back("kind", V{error.command});
    out.emplace_back("source_id", V{error.source_id});
  }
  if (error.battle_id) {
    if (error.battle_id_after_path)
      out.emplace_back("path", V{error.original.path});
    out.emplace_back("battle_id", V{*error.battle_id});
    if (!error.battle_id_after_path)
      out.emplace_back("path", V{error.original.path});
  }
  if (error.army_id)
    out.emplace_back("army_id", V{*error.army_id});
  if (error.reason)
    out.emplace_back("reason", V{*error.reason});
  if (error.limit)
    out.emplace_back("limit", V{*error.limit});
  if (error.maximum)
    out.emplace_back("maximum", V{*error.maximum});
  if (error.feasibility)
    out.emplace_back("feasibility", V{*error.feasibility});
  if (error.new_total)
    out.emplace_back("new_total", V{*error.new_total});
  if (error.skill_id)
    out.emplace_back("skill_id", V{*error.skill_id});
  if (error.required_level)
    out.emplace_back("required_level", V{*error.required_level});
  if (error.actual_level)
    out.emplace_back("actual_level", V{*error.actual_level});
  if (error.actor_id)
    out.emplace_back("actor_id", V{*error.actor_id});
  return {std::move(out)};
}
// 这里只拼接结果明细的已冻结字段；military规则/条款仍归C。
// 平铺军训、原空journey形状及所有存在位沿旧writer，字段名不从payload学习。
Status emit_military_effect_fields(ProjectionWriter &w,
                                   const MilitaryEffect &effect,
                                   const FieldOrder &keys) {
  if (effect.request_answer)
    w.object("request_answer", [&](auto &child) {
      return military_request_codec::emit_answer_fields(child,
                                                        *effect.request_answer);
    });
  if (effect.details_ok)
    w.field("ok", *effect.details_ok);
  if (had(keys, "success"))
    w.field("success", effect.success);
  if (had(keys, "code"))
    w.field("code", effect.code);
  if (had(keys, "already_completed") && effect.already_completed)
    w.field("already_completed", *effect.already_completed);
  if (!effect.action_id.empty() || had(keys, "action_id"))
    w.field("action_id", effect.action_id);
  if (effect.training_receipt) {
    auto status = training_codec::emit_fields(w, *effect.training_receipt);
    if (!status.ok)
      return status;
  }
  for (const auto &item :
       std::initializer_list<std::pair<const char *, const StableId *>>{
           {"battle_id", &effect.battle_id},
           {"siege_id", &effect.siege_id},
           {"office_id", &effect.office_id},
           {"receipt_id", &effect.receipt_id},
           {"army_id", &effect.army_id},
           {"order_id", &effect.order_id},
           {"campaign_id", &effect.campaign_id},
           {"fact_id", &effect.fact_id},
           {"source_id", &effect.source_id}})
    if (!item.second->empty() || had(keys, item.first) ||
        (keys.empty() && std::string_view(item.first) == "source_id" &&
         !effect.fact_id.empty()))
      w.field(item.first, *item.second);
  if (effect.plan_submitted)
    w.field("plan_submitted", *effect.plan_submitted);
  const auto journey = [](ProjectionWriter &child,
                          const MilitaryJourneyRequest &request) {
    child.field("army_id", request.army_id);
    child.field("destination_settlement_id", request.destination_settlement_id);
    if (request.return_garrison_id)
      child.field("return_garrison_id", *request.return_garrison_id);
    return child.order_fields(request.field_order);
  };
  if (effect.journey_request)
    w.object("journey_request", [&](auto &child) {
      return journey(child, *effect.journey_request);
    });
  else if (had(keys, "journey_request") ||
           (keys.empty() && effect.return_wait_reason))
    w.object("journey_request", [](auto &child) { return child.status(); });
  if (effect.return_wait_reason)
    w.field("return_wait_reason", *effect.return_wait_reason);
  if (!effect.completed_receipt_ids.empty() ||
      had(keys, "completed_receipt_ids"))
    w.sequence("completed_receipt_ids", effect.completed_receipt_ids,
               [](auto &child, const auto &id) { return child.field("", id); });
  if (!effect.actor_changes.empty() || had(keys, "actor_changes"))
    w.sequence("actor_changes", effect.actor_changes,
               [](auto &child, const auto &change) {
                 return child.object("", [&](auto &fields) {
                   return battle_codec::emit_fields(fields, change);
                 });
               });
  if (!effect.journey_requests.empty() || had(keys, "journey_requests"))
    w.sequence("journey_requests", effect.journey_requests,
               [&](auto &child, const auto &request) {
                 return child.object("", [&](auto &fields) {
                   return journey(fields, request);
                 });
               });
  return w.order_fields(keys);
}
MilitaryEffect read_military_effect(const V &input, const StableId &path) {
  boundary::fields(input, {},
                   {"ok",
                    "success",
                    "code",
                    "already_completed",
                    "action_id",
                    "office_id",
                    "army_id",
                    "order_id",
                    "campaign_id",
                    "battle_id",
                    "siege_id",
                    "plan_submitted",
                    "receipt_id",
                    "fact_id",
                    "source_id",
                    "return_wait_reason",
                    "completed_receipt_ids",
                    "actor_changes",
                    "journey_requests",
                    "journey_request",
                    "unit_id",
                    "provided_half_days",
                    "fee_paid",
                    "training_gain",
                    "released_unprovided_fee",
                    "request_answer"},
                   path);
  MilitaryEffect result;
  if (auto answer = input.find("request_answer")) {
    result.request_answer.emplace();
    military_request_codec::decode_answer(*answer, *result.request_answer,
                                          path + "/request_answer");
  }
  if (auto x = input.find("ok"))
    result.details_ok = boundary::boolean(*x, path + "/ok");
  if (auto x = input.find("success"))
    result.success = boundary::boolean(*x, path + "/success");
  if (auto x = input.find("code"))
    result.code = boundary::string(*x, path + "/code");
  if (auto x = input.find("already_completed"))
    result.already_completed =
        boundary::boolean(*x, path + "/already_completed");
  for (const auto &field :
       std::initializer_list<std::pair<const char *, StableId *>>{
           {"action_id", &result.action_id},
           {"office_id", &result.office_id},
           {"army_id", &result.army_id},
           {"order_id", &result.order_id},
           {"campaign_id", &result.campaign_id},
           {"battle_id", &result.battle_id},
           {"siege_id", &result.siege_id},
           {"receipt_id", &result.receipt_id},
           {"fact_id", &result.fact_id},
           {"source_id", &result.source_id}})
    if (auto x = input.find(field.first))
      *field.second = boundary::string(*x, path + "/" + field.first);
  if (auto x = input.find("plan_submitted"))
    result.plan_submitted = boundary::boolean(*x, path + "/plan_submitted");
  if (auto x = input.find("return_wait_reason"))
    result.return_wait_reason =
        boundary::string(*x, path + "/return_wait_reason");
  // 抽取原平铺军训字段交唯一owner校验；任一出现就要求完整五字段，禁止半份成功收据。
  const std::vector<StableId> training_keys = {"unit_id", "provided_half_days",
                                               "fee_paid", "training_gain",
                                               "released_unprovided_fee"};
  D training;
  for (const auto &field : boundary::object(input, path))
    if (std::find(training_keys.begin(), training_keys.end(), field.first) !=
        training_keys.end())
      training.push_back(field);
  if (!training.empty()) {
    result.training_receipt.emplace();
    training_codec::decode(V{std::move(training)}, *result.training_receipt,
                           path);
  }
  if (auto x = input.find("completed_receipt_ids"))
    result.completed_receipt_ids =
        boundary::strings(*x, path + "/completed_receipt_ids");
  if (auto x = input.find("actor_changes")) {
    std::size_t index = 0;
    for (const auto &value : boundary::array(*x, path + "/actor_changes")) {
      MilitaryActorChange change;
      battle_codec::decode(value, change,
                           path + "/actor_changes/" + std::to_string(index++));
      result.actor_changes.push_back(std::move(change));
    }
  }
  const auto journey = [](const V &value, const StableId &nested) {
    boundary::fields(value, {"army_id", "destination_settlement_id"},
                     {"return_garrison_id"}, nested);
    MilitaryJourneyRequest request;
    request.army_id = boundary::get_string(value, "army_id", nested);
    request.destination_settlement_id =
        boundary::get_string(value, "destination_settlement_id", nested);
    if (auto x = value.find("return_garrison_id"))
      request.return_garrison_id =
          boundary::string(*x, nested + "/return_garrison_id");
    request.field_order = order(value);
    return request;
  };
  if (auto x = input.find("journey_request")) {
    const auto nested = path + "/journey_request";
    if (!boundary::object(*x, nested).empty())
      result.journey_request = journey(*x, nested);
  }
  if (auto x = input.find("journey_requests")) {
    std::size_t index = 0;
    for (const auto &value : boundary::array(*x, path + "/journey_requests"))
      result.journey_requests.push_back(journey(
          value, path + "/journey_requests/" + std::to_string(index++)));
  }
  return result;
}
// completed_actions逐键保原序，成功/完成字段的存在位也不能归一化。
// 原完成variant优先级和字段存在位不变；每个实际owner发自己的字段。
Status emit_completion_fields(ProjectionWriter &w,
                              const ActionCompletion &completion) {
  if (completion.growth)
    return emit_growth_effect_fields(w, *completion.growth);
  if (completion.family)
    return emit_family_effect_fields(w, *completion.family);
  if (completion.task)
    return emit_task_effect_fields(w, *completion.task);
  w.field("ok", completion.ok);
  if (completion.completed)
    w.field("completed", *completion.completed);
  if (completion.success)
    w.field("success", *completion.success);
  if (completion.code)
    w.field("code", *completion.code);
  if (completion.already_completed)
    w.field("already_completed", *completion.already_completed);
  if (completion.military)
    w.object("details", [&](auto &fields) {
      return emit_military_effect_fields(fields, *completion.military,
                                         completion.military_field_order);
    });
  return w.order_fields(completion.field_order);
}
V completion_value(const ActionCompletion &completion) {
  V value;
  BoundaryProjectionWriter writer(value);
  auto status = writer.object("", [&](auto &fields) {
    return emit_completion_fields(fields, completion);
  });
  if (status.ok)
    status = writer.finish();
  if (!status.ok)
    throw MarshalFailure{status.error};
  return value;
}
ActionCompletion read_completion(const V &value, const StableId &path) {
  if (value.find("skill_id") || value.find("xp_gained") ||
      value.find("mastery")) {
    auto decoded = decode_growth_effect(
        value.find("xp_gained") ? "study_skill" : "practice_skill", value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    ActionCompletion growth;
    growth.growth = std::move(decoded.value);
    return growth;
  }
  if (value.find("actor_id")) {
    auto decoded = decode_family_effect(value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    ActionCompletion family;
    family.family = std::move(decoded.value);
    return family;
  }
  if (value.find("task_id") || value.find("on_unavailable") ||
      value.find("retry")) {
    auto decoded = decode_task_effect(value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    ActionCompletion task;
    task.task = std::move(decoded.value);
    return task;
  }
  boundary::fields(
      value, {"ok"},
      {"completed", "success", "code", "already_completed", "details"}, path);
  ActionCompletion result;
  result.field_order = order(value);
  result.ok = boundary::get_bool(value, "ok", path);
  if (auto x = value.find("completed"))
    result.completed = boundary::boolean(*x, path + "/completed");
  if (auto x = value.find("success"))
    result.success = boundary::boolean(*x, path + "/success");
  if (auto x = value.find("code"))
    result.code = boundary::string(*x, path + "/code");
  if (auto x = value.find("already_completed"))
    result.already_completed =
        boundary::boolean(*x, path + "/already_completed");
  if (auto x = value.find("details")) {
    result.military = read_military_effect(*x, path + "/details");
    result.military_field_order = order(*x);
  }
  return result;
}
// 每一variant仍走原owner同writer；本层只保原组合字段及最终details顺序。
Status emit_details_fields(ProjectionWriter &w, const ReceiptDetails &details) {
  auto status = std::visit(
      [&](const auto &item) -> Status {
        using T = std::decay_t<decltype(item)>;
        auto strings = [&](std::string_view name, const auto &ids) {
          return w.sequence(name, ids, [](auto &child, const auto &id) {
            return child.field("", id);
          });
        };
        if constexpr (std::is_same_v<T, Done>)
          return w.status();
        else if constexpr (std::is_same_v<T, ActionStarted>)
          return w.field("action_id", item.action_id);
        else if constexpr (std::is_same_v<T, AdvanceDetails>) {
          w.object("completed_actions", [&](auto &actions) {
            for (const auto &row : item.completed_actions.rows()) {
              actions.object(row.first, [&](auto &fields) {
                return emit_completion_fields(fields, row.second);
              });
              if (!actions.status().ok)
                break;
            }
            return actions.status();
          });
          strings("suspended_actors", item.suspended_actors);
          return w.order_fields(item.field_order);
        } else if constexpr (std::is_same_v<T, MilitaryEffect>)
          return emit_military_effect_fields(w, item, details.field_order);
        else if constexpr (std::is_same_v<T, InteractionEffect>) {
          if (item.party_id || item.person_id || item.news_ids)
            return emit_interaction_extra_effect_fields(w, item);
          if (!item.encounter_id.empty() ||
              had(details.field_order, "encounter_id"))
            w.field("encounter_id", item.encounter_id);
          if (!item.settlement_result.empty() ||
              had(details.field_order, "settlement_result"))
            w.field("settlement_result", item.settlement_result);
          return w.status();
        } else if constexpr (std::is_same_v<T, FieldBattleReceipt>)
          return emit_field_battle_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, PersonalReceipt>)
          return emit_personal_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, GrowthEffect>)
          return emit_growth_effect_fields(w, item);
        else if constexpr (std::is_same_v<T, FamilyEffect>)
          return emit_family_effect_fields(w, item);
        else if constexpr (std::is_same_v<T, PoliticsEffects>)
          return emit_politics_effects_fields(w, item);
        else if constexpr (std::is_same_v<T, PoliticalFiscalEffects>)
          return emit_political_fiscal_effects_fields(w, item);
        else if constexpr (std::is_same_v<T, NavalRepairReceipt>)
          return naval_repair_codec::emit_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, RepairServiceCommandReceipt>)
          return emit_repair_service_command_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, ContinuationBoundaryReceipt>)
          return emit_continuation_boundary_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, DirectForceIntentReceipt3>)
          return emit_direct_force_intent_receipt3_fields(w, item);
        else if constexpr (std::is_same_v<T, PendingRetirementCancelled>)
          return emit_pending_retirement_cancelled_fields(w, item);
        else if constexpr (std::is_same_v<T, ContinuationBoundaryCancelled>)
          return emit_continuation_boundary_cancelled_fields(w, item);
        else if constexpr (std::is_same_v<T, NavalCommandReceipt>)
          return naval_command_codec::emit_receipt_fields(w, item);
        else if constexpr (std::is_same_v<T, NavalBattleCommandReceipt>)
          return naval_battle_command_codec::emit_receipt_fields(w, item, 2);
        else if constexpr (std::is_same_v<T, LocalEconomyEffects>)
          return emit_economic_configuration_effects_fields(w, item);
        else if constexpr (std::is_same_v<T,
                                          HouseholdProvisionManagementEffects>)
          return emit_household_provision_management_effects_fields(w, item);
        else if constexpr (std::is_same_v<T, MembershipOfferEffects>) {
          w.field("offer_id", item.offer_id);
          w.field("decision_id", item.decision_id);
          w.field("member_episode_id", item.member_episode_id);
          strings("fact_ids", item.fact_ids);
          return strings("receipt_ids", item.receipt_ids);
        } else if constexpr (std::is_same_v<T, SiegeIntegrationEffect>) {
          w.field("proposal_id", item.proposal_id);
          w.field("proposal_key", item.proposal_key);
          w.field("actor_action_source_id", item.actor_action_source_id);
          w.field("decision_source_id", item.decision_source_id);
          w.field("status", item.status);
          return w.field("completion_source_id", item.completion_source_id);
        } else if constexpr (std::is_same_v<T, OrganizationEffects>) {
          strings("receipt_ids", item.receipt_ids);
          return strings("fact_ids", item.fact_ids);
        } else
          return emit_estate_receipt_fields(w, item);
      },
      details.data);
  return status.ok ? w.order_fields(details.field_order) : status;
}
ReceiptDetails read_details(const V &value, const StableId &kind) {
  const auto &dict = boundary::object(value, "receipt/details");
  ReceiptDetails result;
  result.field_order = order(value);
  if (field_receipt_kind(kind)) {
    auto original = decode_field_battle_receipt(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    auto valid = validate_field_battle_receipt(kind, original.value);
    if (!valid.ok)
      throw MarshalFailure{std::move(valid.error)};
    result.data = std::move(original.value);
    return result;
  }
  if (kind == "stabilize_continuation_boundary") {
    auto original = decode_continuation_boundary_receipt(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    result.data = std::move(original.value);
    return result;
  }
  if (kind == "direct_force_intent") {
    auto original = decode_direct_force_intent_receipt3(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    result.data = std::move(original.value);
    return result;
  }
  if (kind == "cancel_pending_retirement") {
    auto original = decode_pending_retirement_cancelled(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    result.data = std::move(original.value);
    return result;
  }
  if (kind == "cancel_continuation_boundary") {
    auto original = decode_continuation_boundary_cancelled(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    result.data = std::move(original.value);
    return result;
  }
  if (repair_service_command_kind(kind)) {
    auto original = decode_repair_service_command_receipt(value);
    if (!original.ok)
      throw MarshalFailure{std::move(original.error)};
    if (original.value.command_kind != kind)
      failed("RepairCommandReceiptKindMismatch", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(original.value);
    return result;
  }
  if (kind == "naval_repair_start" || kind == "naval_repair_cancel") {
    auto r = naval_repair_codec::decode_receipt(value, kind);
    if (!r.ok)
      throw MarshalFailure{r.error};
    result.data = std::move(r.value);
    return result;
  }
  // 计划编辑及无原领域副作用的内部停点仅承认Done，不接受伪造行动或完成集合。
  if (private_shop_daily_rent_command_kind(kind)) {
    boundary::fields(value, {"settlement_result"}, {}, "receipt/details");
    InteractionEffect effect;
    effect.settlement_result =
        boundary::get_string(value, "settlement_result", "receipt/details");
    result.data = std::move(effect);
    auto valid = validate_private_shop_daily_rent_receipt(kind, result);
    if (!valid.ok)
      throw MarshalFailure{valid.error};
    return result;
  }
  if (native_work_lifecycle_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (kind == political_takeover_command_kind) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (player_plan_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (kind == "task_personal_advance") {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (party_participation_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (treaty_passage_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (political_military_appointment_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (political_field_appointment_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (political_shop_licensing_command_kind(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (kind == political_shop_registry_accept_command) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (kind == political_governance_continue_command) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (is_governance_invitation_reply_command(kind)) {
    boundary::fields(value, {}, {}, "receipt/details");
    result.data = Done{};
    return result;
  }
  if (diplomacy_command_kind(kind)) {
    if ((kind == "envoy_deliver" || kind == "envoy_report")) {
      boundary::fields(value, {"action_id"}, {}, "receipt/details");
      auto id = boundary::get_string(value, "action_id", "receipt/details");
      if (id.empty())
        failed("InvalidDiplomacyReceipt", "receipt/details");
      result.data = ActionStarted{std::move(id)};
    } else {
      boundary::fields(value, {}, {}, "receipt/details");
      result.data = Done{};
    }
    return result;
  }
  if (kind == "ask_news") {
    boundary::fields(value, {"news_ids"}, {}, "receipt/details");
    InteractionEffect effect;
    effect.news_ids =
        boundary::strings(boundary::field(value, "news_ids", "receipt/details"),
                          "receipt/details/news_ids");
    result.data = std::move(effect);
    return result;
  }
  if (is_political_succession_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_succession_effect(kind, effect.value))
      failed("InvalidPoliticalSuccessionReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_head_handover_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_head_handover_effect(kind, effect.value))
      failed("InvalidPoliticalHeadHandoverReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_successor_round_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_successor_round_effect(kind, effect.value))
      failed("InvalidPoliticalSuccessorRoundReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_term_handover_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_term_handover_effect(kind, effect.value))
      failed("InvalidPoliticalTermHandoverReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_office_reception_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_office_reception_effect(kind, effect.value))
      failed("InvalidPoliticalOfficeReceptionReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_office_resignation_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_office_resignation_effect(kind, effect.value))
      failed("InvalidPoliticalOfficeResignationReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_subordinate_appointment_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_subordinate_appointment_effect(kind, effect.value))
      failed("InvalidPoliticalSubordinateAppointmentReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_political_governance_agency_command(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    if (!valid_political_governance_agency_effect(kind, effect.value))
      failed("InvalidPoliticalGovernanceAgencyReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect.value);
    return result;
  }
  if (is_politics_command_kind(kind)) {
    auto effect = decode_politics_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    result.data = std::move(effect.value);
    return result;
  }
  if (kind == "configure_local_production" ||
      kind == "political_issue_supply_policy") {
    auto effect = decode_economic_configuration_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    result.data = std::move(effect.value);
    return result;
  }
  if (kind == "political_set_tax" ||
      kind == political_occupation_fiscal_command_kind) {
    auto effect = decode_political_fiscal_effects(value);
    if (!effect.ok)
      throw MarshalFailure{effect.error};
    result.data = std::move(effect.value);
    return result;
  }
  if (named_membership_command_kind(kind)) {
    boundary::fields(value,
                     {"offer_id", "decision_id", "member_episode_id",
                      "fact_ids", "receipt_ids"},
                     {}, "receipt/details");
    MembershipOfferEffects effect;
    effect.offer_id =
        boundary::get_string(value, "offer_id", "receipt/details");
    effect.decision_id =
        boundary::get_string(value, "decision_id", "receipt/details");
    effect.member_episode_id =
        boundary::get_string(value, "member_episode_id", "receipt/details");
    effect.fact_ids =
        boundary::strings(boundary::field(value, "fact_ids", "receipt/details"),
                          "receipt/details/fact_ids");
    effect.receipt_ids = boundary::strings(
        boundary::field(value, "receipt_ids", "receipt/details"),
        "receipt/details/receipt_ids");
    if (!valid_membership_effect(kind, effect))
      failed("InvalidMembershipReceipt", "receipt/details", ErrorKind::rule);
    result.data = std::move(effect);
    return result;
  }
  if (siege_integration_command_kind(kind) ||
      (kind == "siege_strategy" && value.find("proposal_id"))) {
    boundary::fields(value,
                     {"proposal_id", "proposal_key", "actor_action_source_id",
                      "decision_source_id", "status", "completion_source_id"},
                     {}, "receipt/details");
    SiegeIntegrationEffect effect;
    effect.proposal_id =
        boundary::get_string(value, "proposal_id", "receipt/details");
    effect.proposal_key =
        boundary::get_string(value, "proposal_key", "receipt/details");
    effect.actor_action_source_id = boundary::get_string(
        value, "actor_action_source_id", "receipt/details");
    effect.decision_source_id =
        boundary::get_string(value, "decision_source_id", "receipt/details");
    effect.status = boundary::get_string(value, "status", "receipt/details");
    effect.completion_source_id =
        boundary::get_string(value, "completion_source_id", "receipt/details");
    if (!valid_integration_effect(kind, effect))
      failed("InvalidSiegeIntegrationReceipt", "receipt/details",
             ErrorKind::rule);
    result.data = std::move(effect);
    return result;
  }
  if (kind == "military_field_command_assign") {
    boundary::fields(value,
                     {"receipt_id", "source_id", "office_id", "army_id",
                      "already_completed"},
                     {}, "receipt/details");
    result.data = read_military_effect(value, "receipt/details");
    if (!military_field_command_effect_shape(
            std::get<MilitaryEffect>(result.data)))
      failed("MilitaryFieldCommandCachedEffectMismatch", "receipt/details");
    return result;
  }
  if (kind == "military_tactic_practice") {
    boundary::fields(
        value, {"fact_id", "receipt_id", "source_id", "already_completed"}, {},
        "receipt/details");
    result.data = read_military_effect(value, "receipt/details");
    if (!military_tactic_practice_codec::effect_shape(
            std::get<MilitaryEffect>(result.data)))
      failed("MilitaryTacticPracticeCachedEffectMismatch", "receipt/details");
    return result;
  }
  if (kind == "military_officer_assign") {
    boundary::fields(
        value, {"receipt_id", "source_id", "office_id", "already_completed"},
        {}, "receipt/details");
    result.data = read_military_effect(value, "receipt/details");
    if (!military_officer_effect_shape(std::get<MilitaryEffect>(result.data)))
      failed("MilitaryOfficerCachedEffectMismatch", "receipt/details");
    return result;
  }
  if (kind == "military_request") {
    boundary::fields(
        value,
        {"battle_id", "receipt_id", "already_completed", "request_answer"}, {},
        "receipt/details");
    result.data = read_military_effect(value, "receipt/details");
    return result;
  }
  if (kind == "configure_household_provision") {
    auto r = decode_household_provision_management_effects(value);
    if (!r.ok)
      throw MarshalFailure{r.error};
    result.data = std::move(r.value);
    return result;
  }
  if (naval_battle_command_codec::command_kind(kind)) {
    auto r = naval_battle_command_codec::decode_receipt(value, 2);
    if (!r.ok)
      throw MarshalFailure{r.error};
    if (r.value.kind != kind)
      failed("InvalidNavalBattleReceipt", "receipt/details", ErrorKind::rule);
    result.data = std::move(r.value);
    return result;
  }
  if (kind == "naval_form" || kind == "naval_disband") {
    auto r = naval_command_codec::decode_receipt(value);
    if (!r.ok)
      throw MarshalFailure{r.error};
    if (r.value.kind != kind)
      failed("InvalidNavalReceipt", "receipt/details", ErrorKind::rule);
    result.data = std::move(r.value);
    return result;
  }
  if (dict.empty()) {
    result.data = Done{};
    return result;
  }
  if (kind == "organization_accept_offer") {
    boundary::fields(value, {"action_id"}, {}, "receipt/details");
    result.data = ActionStarted{
        boundary::get_string(value, "action_id", "receipt/details")};
  } else if (dynamic_command_kind(kind)) {
    boundary::fields(value, {"receipt_ids", "fact_ids"}, {}, "receipt/details");
    OrganizationEffects effect;
    effect.receipt_ids = boundary::strings(
        boundary::field(value, "receipt_ids", "receipt/details"),
        "receipt_ids");
    effect.fact_ids = boundary::strings(
        boundary::field(value, "fact_ids", "receipt/details"), "fact_ids");
    result.data = std::move(effect);
  } else if (value.find("completed_actions")) {
    boundary::fields(value, {"completed_actions", "suspended_actors"}, {},
                     "receipt/details");
    AdvanceDetails advance;
    advance.field_order = result.field_order;
    for (const auto &row : boundary::object(
             boundary::field(value, "completed_actions", "receipt/details"),
             "completed_actions"))
      if (!advance.completed_actions.insert(
              row.first,
              read_completion(row.second, "completed_actions/" + row.first)))
        failed("DuplicateReceiptAction", row.first, ErrorKind::rule);
    advance.suspended_actors = boundary::strings(
        boundary::field(value, "suspended_actors", "receipt/details"),
        "suspended_actors");
    result.data = std::move(advance);
  } else if (kind == "duel_start" || kind == "encounter_fight" ||
             kind == "battle_plan" || kind == "battle_close") {
    auto decoded = decode_personal_receipt(kind, value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    result.data = std::move(decoded.value);
  }
  // 作者对白duel_start清掉会话后保留原切磋明细，外层回执仍叫dialogue_confirm。
  else if (kind == "dialogue_confirm" && value.find("battle_id")) {
    auto decoded = decode_personal_receipt("duel_start", value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    result.data = std::move(decoded.value);
  } else if (growth_command_kind(kind) &&
             (!growth_action_kind(kind) || dict.size() != 1 ||
              !value.find("action_id"))) {
    auto decoded = decode_growth_effect(kind, value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    result.data = std::move(decoded.value);
  } else if (kind == "campaign_accept" || kind == "campaign_depart" ||
             kind == "campaign_return" || kind == "campaign_cancel" ||
             kind == "campaign_deliver" || kind == "campaign_report" ||
             kind == "office_resign" || kind == "army_pledge" ||
             kind == "military_plan" || kind == "military_close" ||
             kind == "siege_start" || kind == "siege_join" ||
             kind == "siege_plan" || kind == "siege_strategy" ||
             kind == "siege_close" || kind == "military_train_cancel" ||
             kind == "military_drill_draft" ||
             kind == "military_drill_confirm" ||
             kind == "military_drill_cancel_pending" ||
             kind == "military_drill_exit")
    result.data = read_military_effect(value, "receipt/details");
  else if (value.find("action_id")) {
    boundary::fields(value, {"action_id"}, {}, "receipt/details");
    result.data = ActionStarted{
        boundary::get_string(value, "action_id", "receipt/details")};
  }

  else if (kind == "join_party" || kind == "dismiss_companion") {
    boundary::fields(value, {"party_id", "person_id"}, {}, "receipt/details");
    InteractionEffect effect;
    effect.party_id =
        boundary::get_string(value, "party_id", "receipt/details");
    effect.person_id =
        boundary::get_string(value, "person_id", "receipt/details");
    result.data = std::move(effect);
  } else if (is_visit_appointment_command(kind) ||
             kind == "cancel_visit_appointment" ||
             visit_reply_command_kind(kind) ||
             visit_message_command_kind(kind) ||
             kind == household_purchase_policy_command_kind ||
             kind == household_purchase_plan_command_kind ||
             kind == household_report_command_kind ||
             kind == task_extension_command_kind ||
             kind == "personal_appointment_accept" ||
             kind == "personal_appointment_cancel") {
    boundary::fields(value, {"settlement_result"}, {}, "receipt/details");
    InteractionEffect effect;
    effect.settlement_result =
        boundary::get_string(value, "settlement_result", "receipt/details");
    if (effect.settlement_result.empty())
      failed("InvalidVisitMessageReceipt", "receipt/details");
    result.data = std::move(effect);
  } else if (kind == "encounter_leave") {
    boundary::fields(value, {"encounter_id", "settlement_result"}, {},
                     "receipt/details");
    result.data = InteractionEffect{
        boundary::get_string(value, "encounter_id", "receipt/details"),
        boundary::get_string(value, "settlement_result", "receipt/details")};
  } else if (kind.rfind("family_", 0) == 0) {
    auto decoded = decode_family_effect(value);
    if (!decoded.ok)
      throw MarshalFailure{decoded.error};
    result.data = std::move(decoded.value);
  } else if (kind.rfind("estate_", 0) == 0) {
    boundary::fields(value, {"estate_id", "fact_id"}, {"actor_ids"},
                     "receipt/details");
    EstateReceipt estate{
        boundary::get_string(value, "estate_id", "receipt/details"),
        boundary::get_string(value, "fact_id", "receipt/details"),
        {}};
    if (auto *people = value.find("actor_ids"))
      estate.actor_ids =
          boundary::strings(*people, "receipt/details/actor_ids");
    result.data = std::move(estate);
  } else if (kind == "campaign_accept" || kind == "office_accept" ||
             kind == "army_form" || kind == "campaign_depart")
    result.data = read_military_effect(value, "receipt/details");
  else
    failed("UnsupportedNativeCapability", "receipt/details/" + kind,
           ErrorKind::unsupported_capability);
  return result;
}
// 原地形报价是有限已计划路线，世界state从不混入返回值。
V travel_quote(const TravelQuote &quote) {
  if (quote.road) {
    D out{{"ok", {true}},
          {"path", boundary::string_array(quote.road->path)},
          {"duration", {quote.duration}},
          {"cost", {quote.cost}}};
    if (quote.road->returning)
      out.emplace_back("returning", V{*quote.road->returning});
    else
      out.emplace_back("payer_wallet_id", V{quote.payer_wallet_id});
    return {std::move(out)};
  }
  A points;
  for (const auto &point : quote.points)
    points.push_back({A{V{point.x}, V{point.y}}});
  const V destination = std::visit(
      [](const auto &x) -> V {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, StableId>)
          return {x};
        else
          return {A{V{x.x}, V{x.y}}};
      },
      quote.destination);
  return {D{{"ok", {true}},
            {"duration", {quote.duration}},
            {"cost", {quote.cost}},
            {"terrain", {true}},
            {"map_id", {quote.map_id}},
            {"points", {std::move(points)}},
            {"destination", destination},
            {"origin", {quote.origin}},
            {"mode", {quote.mode}},
            {"budget", {quote.budget}},
            {"starting_load", {quote.starting_load}},
            {"payer_wallet_id", {quote.payer_wallet_id}},
            {"money_per_tick", {quote.money_per_tick}},
            {"fatigue_per_tick", {quote.fatigue_per_tick}}}};
}
// 顺序跟随原命令的字面构造；open_event沿用talk报价，不额外添加event_id。
V interaction_quote(const StableId &kind, const InteractionQuote &quote) {
  D result{{"ok", {true}}};
  if (kind == "dialogue_next" || kind == "dialogue_choose") {
    result.emplace_back("next", V{quote.next});
    result.emplace_back("duration", V{quote.duration});
  } else if (kind == "encounter_talk" || kind == "encounter_leave") {
    result.emplace_back("duration", V{quote.duration});
    result.emplace_back("encounter_id", V{quote.encounter_id});
    if (kind == "encounter_talk") {
      result.emplace_back("dialogue_id", V{quote.dialogue_id});
      if (!quote.context)
        failed("MissingInteractionQuote", "context");
      result.emplace_back("context",
                          interaction_codec::encode_context(*quote.context));
    }
  } else if (kind == "talk" || kind == "talk_actor" || kind == "open_event") {
    if (!quote.context)
      failed("MissingInteractionQuote", "context");
    result.emplace_back("context",
                        interaction_codec::encode_context(*quote.context));
    result.emplace_back("dialogue_id", V{quote.dialogue_id});
    result.emplace_back("duration", V{quote.duration});
  } else
    return encode_interaction_extra_quote(kind, quote);
  return {std::move(result)};
}
// checkpoint字段序由本层唯一receipt类型维护；值全为typed，不保存JSON快照作缓存。
Status visit_order(CodecVisitor &c, FieldOrder &keys) {
  return c.sequence("field_order", keys, [](auto &codec, auto &key) {
    return codec.field("key", key);
  });
}
Status visit_effect(CodecVisitor &c, MilitaryEffect &e) {
  // 新军事能力才扩共同回执；旧3/4连optional存在位也不得写入。
  if (c.schema_features & military_request_codec::checkpoint_feature)
    c.optional("request_answer", e.request_answer,
               military_request_codec::visit_answer);
  else if (e.request_answer)
    return c.reject("UnexpectedMilitaryRequestAnswer");
  c.field("success", e.success);
  c.field("code", e.code);
  c.field("office_id", e.office_id);
  c.field("army_id", e.army_id);
  c.field("order_id", e.order_id);
  c.field("campaign_id", e.campaign_id);
  c.optional("details_ok", e.details_ok,
             [](auto &v, auto &x) { return v.field("value", x); });
  c.optional("already_completed", e.already_completed,
             [](auto &v, auto &x) { return v.field("value", x); });
  const auto journey = [](CodecVisitor &visitor,
                          MilitaryJourneyRequest &request) {
    visitor.field("army_id", request.army_id);
    visitor.field("destination_settlement_id",
                  request.destination_settlement_id);
    visitor.optional(
        "return_garrison_id", request.return_garrison_id,
        [](auto &nested, auto &id) { return nested.field("value", id); });
    return visit_order(visitor, request.field_order);
  };
  c.optional("journey_request", e.journey_request, journey);
  c.field("action_id", e.action_id);
  c.optional("training_receipt", e.training_receipt,
             [](auto &visitor, auto &receipt) {
               return training_codec::visit(visitor, receipt);
             });
  c.field("receipt_id", e.receipt_id);
  c.field("fact_id", e.fact_id);
  c.field("source_id", e.source_id);
  c.field("battle_id", e.battle_id);
  c.field("siege_id", e.siege_id);
  c.optional(
      "plan_submitted", e.plan_submitted,
      [](auto &visitor, auto &value) { return visitor.field("value", value); });
  c.optional(
      "return_wait_reason", e.return_wait_reason,
      [](auto &visitor, auto &value) { return visitor.field("value", value); });
  c.sequence("completed_receipt_ids", e.completed_receipt_ids,
             [](auto &visitor, auto &id) { return visitor.field("id", id); });
  c.sequence("actor_changes", e.actor_changes, [](auto &visitor, auto &change) {
    return visitor.object("change", [&](auto &nested) {
      return battle_codec::visit(nested, change);
    });
  });
  c.sequence("journey_requests", e.journey_requests,
             [&](auto &visitor, auto &request) {
               return visitor.object("request", [&](auto &nested) {
                 return journey(nested, request);
               });
             });
  return c.status();
}
Status visit_completion(CodecVisitor &c, ActionCompletion &x) {
  c.field("ok", x.ok);
  c.optional("completed", x.completed,
             [](auto &v, auto &a) { return v.field("value", a); });
  c.optional("success", x.success,
             [](auto &v, auto &a) { return v.field("value", a); });
  c.optional("code", x.code,
             [](auto &v, auto &a) { return v.field("value", a); });
  c.optional("already_completed", x.already_completed,
             [](auto &v, auto &a) { return v.field("value", a); });
  c.optional("task", x.task, visit_task_effect);
  c.optional("family", x.family, visit_family_effect);
  c.optional("growth", x.growth, visit_growth_effect);
  c.optional("military", x.military, visit_effect);
  c.object("military_order",
           [&](auto &v) { return visit_order(v, x.military_field_order); });
  return visit_order(c, x.field_order);
}
Status visit_details(CodecVisitor &c, ReceiptDetails &x, const StableId &kind) {
  if ((kind == "envoy_report" || kind == "diplomatic_delegate_envoy") &&
      !(c.schema_features & 2048u))
    return c.reject("DiplomaticReportVersionRequired");
  Integer tag = x.data.index();
  c.field("variant", tag);
  if (native_work_lifecycle_command_kind(kind) &&
      (c.task_work_execution_codec_version != 6 || tag != 0))
    return c.reject("WorkLifecycleCommandReceiptProfileRequired");
  if (private_shop_daily_rent_command_kind(kind) &&
      (!(c.schema_features & private_shop_daily_rent_checkpoint_feature) ||
       tag != 4))
    return c.reject("InvalidPrivateShopRentReceipt");
  if (is_governance_invitation_reply_command(kind) &&
      (!(c.schema_features & governance_invitation_reply_feature) || tag != 0))
    return c.reject("InvalidGovernanceInvitationReceipt");
  if (((kind == "private_shop_appoint_executor" ||
        kind == "private_shop_accept_executorship" ||
        kind == "private_shop_revoke_executor" ||
        kind == "private_shop_estate_close") ||
       kind == task_shop_order_estate_cancel_command) &&
      (!(c.schema_features & private_shop_liquidation_checkpoint_feature) ||
       tag != 4))
    return c.reject("InvalidPrivateShopLiquidationReceipt");
  if (kind == political_governance_continue_command &&
      (!(c.schema_features & political_governance_continuation_feature) ||
       tag != 0))
    return c.reject("InvalidGovernanceContinuationReceipt");
  if (kind == political_shop_registry_accept_command &&
      (!(c.schema_features &
         political_shop_registry_succession_checkpoint_feature) ||
       tag != 0))
    return c.reject("InvalidPoliticalShopRegistryReceipt");
  // 采购命令仍沿原 InteractionEffect 原结果槽保存实际 S5 fact
  // ID，不引入第二结算表。
  if ((kind == household_purchase_policy_command_kind ||
       kind == household_purchase_plan_command_kind) &&
      (tag != 4 || (c.household_purchase_codec_version != 2 &&
                    c.household_purchase_codec_version != 3)))
    return c.reject("InvalidHouseholdPurchaseCommandReceipt");
  if (kind == household_report_command_kind &&
      (tag != 4 || c.face_to_face_codec_version != 3))
    return c.reject("HouseholdReportReceiptFace3Required");
  if (private_shop_command_kind(kind) || kind == "task_shop_order_accept" ||
      kind == "task_shop_order_deliver" || kind == "task_shop_order_cancel") {
    const bool action = kind == "private_shop_restock" ||
                        kind == "private_shop_operate" ||
                        kind == "task_shop_order_deliver";
    if (!(c.schema_features & private_shop_checkpoint_feature) ||
        tag != (action ? 1 : 4))
      return c.reject("InvalidPrivateShopReceipt");
  }
  if (kind == "military_field_command_assign" &&
      (tag != 3 ||
       !(c.schema_features & military_field_command_checkpoint_feature)))
    return c.reject("MilitaryFieldCommandCachedEffectMismatch");
  if (kind == "military_officer_assign" &&
      (tag != 3 || !(c.schema_features & military_officer_checkpoint_feature)))
    return c.reject("MilitaryOfficerCachedEffectMismatch");
  if (political_military_appointment_command_kind(kind) &&
      (!(c.schema_features & military_officer_checkpoint_feature) || tag != 0))
    return c.reject("InvalidPoliticalMilitaryAppointmentReceipt");
  if (political_field_appointment_command_kind(kind) &&
      (!(c.schema_features & military_field_command_checkpoint_feature) ||
       tag != 0))
    return c.reject("InvalidPoliticalFieldAppointmentReceipt");
  if (political_shop_licensing_command_kind(kind) &&
      (!(c.schema_features & political_shop_licensing_checkpoint_feature) ||
       tag != 0))
    return c.reject("InvalidPoliticalShopLicensingReceipt");
  if (treaty_passage_command_kind(kind) &&
      (!(c.schema_features & treaty_passage_checkpoint_feature) || tag != 0))
    return c.reject("InvalidTreatyPassageReceipt");
  if (kind == political_takeover_command_kind && tag != 0)
    return c.reject("InvalidPoliticalTakeoverReceipt");
  if (kind == "task_personal_advance" && tag != 0)
    return c.reject("InvalidTaskAdvanceReceipt");
  if (party_participation_command_kind(kind) && tag != 0)
    return c.reject("InvalidPartyParticipationReceipt");
  if (kind == "military_request" &&
      (!(c.schema_features & military_request_codec::checkpoint_feature) ||
       tag != 3))
    return c.reject("InvalidMilitaryRequestReceipt");
  if (diplomacy_command_kind(kind) &&
      tag != ((kind == "envoy_deliver" || kind == "envoy_report") ? 1 : 0))
    return c.reject("InvalidDiplomacyReceipt");
  if (player_plan_command_kind(kind) && tag != 0)
    return c.reject("InvalidPlayerPlanReceipt");
  if (named_membership_command_kind(kind) && tag != 11)
    return c.reject("InvalidMembershipReceipt");
  if (tag == 11 &&
      (!named_membership_command_kind(kind) ||
       !(c.schema_features & common_checkpoint::named_membership_receipts)))
    return c.reject("InvalidMembershipReceipt");
  if (kind == "ask_news" && tag != 4)
    return c.reject("InvalidNewsReceipt");
  if (siege_integration_command_kind(kind) && tag != 10)
    return c.reject("InvalidSiegeIntegrationReceipt");
  if (tag == 10 &&
      !(siege_integration_command_kind(kind) || kind == "siege_strategy"))
    return c.reject("InvalidSiegeIntegrationReceipt");
  if (is_political_succession_command(kind) &&
      (c.political_succession_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalSuccessionReceipt");
  if (is_political_head_handover_command(kind) &&
      (c.political_head_handover_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalHeadHandoverReceipt");
  if (is_political_term_handover_command(kind) &&
      (c.political_term_handover_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalTermHandoverReceipt");
  if (is_political_office_reception_command(kind) &&
      (c.political_office_reception_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalOfficeReceptionReceipt");
  if (is_political_office_resignation_command(kind) &&
      ((c.political_office_resignation_codec_version != 1 &&
        c.political_office_resignation_codec_version != 2) ||
       tag != 12))
    return c.reject("InvalidPoliticalOfficeResignationReceipt");
  if (is_political_subordinate_appointment_command(kind) &&
      (c.political_subordinate_appointment_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalSubordinateAppointmentReceipt");
  if (is_political_governance_agency_command(kind) &&
      ((c.political_governance_agency_codec_version != 1 &&
        c.political_governance_agency_codec_version != 2) ||
       tag != 12))
    return c.reject("InvalidPoliticalGovernanceAgencyReceipt");
  if (is_political_successor_round_command(kind) &&
      (c.political_successor_round_codec_version != 1 || tag != 12))
    return c.reject("InvalidPoliticalSuccessorRoundReceipt");
  if ((is_politics_command_kind(kind) && tag != 12) ||
      (tag == 12 &&
       !((is_politics_command_kind(kind) &&
          (c.schema_features & common_checkpoint::politics_receipts)) ||
         (is_political_head_handover_command(kind) &&
          c.political_head_handover_codec_version == 1) ||
         (is_political_term_handover_command(kind) &&
          c.political_term_handover_codec_version == 1) ||
         (is_political_office_reception_command(kind) &&
          c.political_office_reception_codec_version == 1) ||
         (is_political_office_resignation_command(kind) &&
          (c.political_office_resignation_codec_version == 1 ||
           c.political_office_resignation_codec_version == 2)) ||
         (is_political_subordinate_appointment_command(kind) &&
          c.political_subordinate_appointment_codec_version == 1) ||
         (is_political_governance_agency_command(kind) &&
          (c.political_governance_agency_codec_version == 1 ||
           c.political_governance_agency_codec_version == 2)) ||
         (is_political_successor_round_command(kind) &&
          c.political_successor_round_codec_version == 1) ||
         (is_political_succession_command(kind) &&
          c.political_succession_codec_version == 1))))
    return c.reject("InvalidPoliticsReceipt");
  if (((kind == "political_set_tax" ||
        kind == political_occupation_fiscal_command_kind) &&
       tag != 13) ||
      (tag == 13 &&
       ((kind != "political_set_tax" &&
         kind != political_occupation_fiscal_command_kind) ||
        !(c.schema_features & common_checkpoint::political_fiscal_receipts) ||
        (kind == political_occupation_fiscal_command_kind &&
         !(c.schema_features & occupation_fiscal_checkpoint_feature)))))
    return c.reject("InvalidPoliticalFiscalReceipt");
  if ((kind == "configure_local_production" ||
       kind == "political_issue_supply_policy") &&
      tag != 14)
    return c.reject("InvalidEconomicCommandReceipt");
  if (tag == 14 &&
      !((kind == "configure_local_production" &&
         (c.schema_features &
          common_checkpoint::production_configuration_receipts)) ||
        (kind == "political_issue_supply_policy" &&
         (c.schema_features & economic_supply_policy_checkpoint_feature))))
    return c.reject("InvalidEconomicCommandReceipt");
  if (((kind == "naval_form" || kind == "naval_disband") && tag != 15) ||
      (tag == 15 &&
       (!(kind == "naval_form" || kind == "naval_disband") ||
        !(c.schema_features & naval_command_codec::checkpoint_feature))))
    return c.reject("InvalidNavalReceipt");
  if ((kind == "configure_household_provision" && tag != 16) ||
      (tag == 16 &&
       (kind != "configure_household_provision" ||
        !(c.schema_features &
          common_checkpoint::household_provision_configuration_receipts))))
    return c.reject("InvalidHouseholdProvisionReceipt");
  if ((naval_battle_command_codec::command_kind(kind) && tag != 17) ||
      (tag == 17 &&
       (!naval_battle_command_codec::command_kind(kind) ||
        (c.naval_codec_version != 2 && c.naval_codec_version != 3 &&
         c.naval_codec_version != 4 && c.naval_codec_version != 5 &&
         c.naval_codec_version != 6))))
    return c.reject("InvalidNavalBattleReceipt");
  if (((kind == "naval_repair_start" || kind == "naval_repair_cancel") &&
       tag != 18) ||
      (tag == 18 &&
       ((kind != "naval_repair_start" && kind != "naval_repair_cancel") ||
        (c.naval_codec_version != 3 && c.naval_codec_version != 4 &&
         c.naval_codec_version != 5 && c.naval_codec_version != 6))))
    return c.reject("InvalidNavalRepairReceipt");
  if ((repair_service_command_kind(kind) && tag != 19) ||
      (tag == 19 && (!repair_service_command_kind(kind) ||
                     c.item_instances_codec_version != 3)))
    return c.reject("RepairServiceReceiptProfileRequired");
  if ((kind == "stabilize_continuation_boundary" && tag != 20) ||
      (tag == 20 && (kind != "stabilize_continuation_boundary" ||
                     c.control_continuation_codec_version != 3)))
    return c.reject("ContinuationBoundaryReceiptProfileRequired");
  if ((kind == "direct_force_intent" && tag != 21) ||
      (tag == 21 && (kind != "direct_force_intent" ||
                     (c.dynamic_military_codec_version != 3 &&
                      c.dynamic_military_codec_version != 4))))
    return c.reject("DirectForceIntentReceiptProfileRequired");
  if ((kind == "cancel_pending_retirement" && tag != 22) ||
      (tag == 22 && (kind != "cancel_pending_retirement" ||
                     c.control_continuation_codec_version != 3)))
    return c.reject("RetirementCancellationReceiptProfileRequired");
  if ((kind == "cancel_continuation_boundary" && tag != 23) ||
      (tag == 23 && (kind != "cancel_continuation_boundary" ||
                     c.control_continuation_codec_version != 3)))
    return c.reject("ContinuationCancellationReceiptProfileRequired");
  // 既有 Field 型追加唯一 tag 24；不重排旧 0..23，也不由 payload 猜选择。
  if ((field_receipt_kind(kind) && tag != 24) ||
      (tag == 24 && (!field_receipt_kind(kind) ||
                     c.field_battle_codec_version != 1)))
    return c.reject("FieldBattleReceiptProfileMismatch");
  if (tag < 0 || tag > 24 ||
      ((tag == 9 || tag == 10) &&
       !(c.schema_features & common_checkpoint::typed_resource_references)))
    return c.reject("InvalidReceiptVariant");
  if (c.reading() && c.status().ok) {
    switch (tag) {
    case 0:
      x.data = Done{};
      break;
    case 1:
      x.data = ActionStarted{};
      break;
    case 2:
      x.data = AdvanceDetails{};
      break;
    case 3:
      x.data = MilitaryEffect{};
      break;
    case 4:
      x.data = InteractionEffect{};
      break;
    case 5:
      x.data = EstateReceipt{};
      break;
    case 6:
      x.data = FamilyEffect{};
      break;
    case 7:
      x.data = PersonalReceipt{};
      break;
    case 8:
      x.data = GrowthEffect{};
      break;
    case 9:
      x.data = OrganizationEffects{};
      break;
    case 10:
      x.data = SiegeIntegrationEffect{};
      break;
    case 11:
      x.data = MembershipOfferEffects{};
      break;
    case 12:
      x.data = PoliticsEffects{};
      break;
    case 13:
      x.data = PoliticalFiscalEffects{};
      break;
    case 14:
      x.data = LocalEconomyEffects{};
      break;
    case 15:
      x.data = NavalCommandReceipt{};
      break;
    case 16:
      x.data = HouseholdProvisionManagementEffects{};
      break;
    case 17:
      x.data = NavalBattleCommandReceipt{};
      break;
    case 18:
      x.data = NavalRepairReceipt{};
      break;
    case 19:
      x.data = RepairServiceCommandReceipt{};
      break;
    case 20:
      x.data = ContinuationBoundaryReceipt{};
      break;
    case 21:
      x.data = DirectForceIntentReceipt3{};
      break;
    case 22:
      x.data = PendingRetirementCancelled{};
      break;
    case 23:
      x.data = ContinuationBoundaryCancelled{};
      break;
    case 24:
      x.data = FieldBattleReceipt{};
      break;
    }
  }
  std::visit(
      [&](auto &value) -> Status {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, ActionStarted>)
          return c.field("action_id", value.action_id);
        else if constexpr (std::is_same_v<T, AdvanceDetails>) {
          auto rows = value.completed_actions.rows();
          c.sequence("completed_actions", rows, [](auto &v, auto &row) {
            v.field("id", row.first);
            return v.object("completion", [&](auto &child) {
              return visit_completion(child, row.second);
            });
          });
          if (c.reading() && c.status().ok) {
            value.completed_actions = {};
            for (auto &row : rows)
              if (!value.completed_actions.insert(row.first,
                                                  std::move(row.second)))
                return c.reject("DuplicateReceiptAction");
          }
          c.sequence("suspended", value.suspended_actors,
                     [](auto &v, auto &id) { return v.field("id", id); });
          return visit_order(c, value.field_order);
        } else if constexpr (std::is_same_v<T, MilitaryEffect>) {
          auto status = visit_effect(c, value);
          if (!status.ok)
            return status;
          if (kind == "military_field_command_assign" &&
              (!(c.schema_features &
                 military_field_command_checkpoint_feature) ||
               !military_field_command_effect_shape(value)))
            return c.reject("MilitaryFieldCommandCachedEffectMismatch");
          if (kind == "military_tactic_practice" &&
              !military_tactic_practice_codec::effect_shape(value))
            return c.reject("MilitaryTacticPracticeCachedEffectMismatch");
          if (kind == "military_officer_assign" &&
              (!(c.schema_features & military_officer_checkpoint_feature) ||
               !military_officer_effect_shape(value)))
            return c.reject("MilitaryOfficerCachedEffectMismatch");
          if ((kind == "military_request") != value.request_answer.has_value())
            return c.reject("InvalidMilitaryRequestReceipt");
          if (value.request_answer && !valid_request_effect(value))
            return c.reject("InvalidMilitaryRequestReceipt");
          return c.status();
        } else if constexpr (std::is_same_v<T, InteractionEffect>) {
          c.field("encounter_id", value.encounter_id);
          c.field("settlement_result", value.settlement_result);
          c.optional("party_id", value.party_id,
                     [](auto &v, auto &id) { return v.field("value", id); });
          c.optional("person_id", value.person_id,
                     [](auto &v, auto &id) { return v.field("value", id); });
          if (kind == "ask_news") {
            if (!value.encounter_id.empty() ||
                !value.settlement_result.empty() || value.party_id ||
                value.person_id)
              return c.reject("InvalidNewsReceipt");
            if (c.reading())
              value.news_ids.emplace();
            if (!value.news_ids)
              return c.reject("MissingNewsReceipt");
            return common_checkpoint::strings(c, "news_ids", *value.news_ids);
          }
          if (value.news_ids)
            return c.reject("UnexpectedNewsReceipt");
          if ((is_visit_appointment_command(kind) ||
               kind == "cancel_visit_appointment" ||
               visit_reply_command_kind(kind) ||
               visit_message_command_kind(kind) ||
               kind == household_purchase_policy_command_kind ||
               kind == household_purchase_plan_command_kind ||
               kind == household_report_command_kind ||
               kind == task_extension_command_kind ||
               kind == "personal_appointment_accept" ||
               kind == "personal_appointment_cancel") &&
              (!value.encounter_id.empty() || value.settlement_result.empty() ||
               value.party_id || value.person_id))
            return c.reject("InvalidVisitMessageReceipt");
          if (private_shop_daily_rent_command_kind(kind)) {
            auto checked = validate_private_shop_daily_rent_receipt(kind, x);
            if (!checked.ok)
              return c.reject("InvalidPrivateShopRentReceipt");
          }
          return c.status();
        } else if constexpr (std::is_same_v<T, FieldBattleReceipt>) {
          auto valid = visit_field_battle_receipt(c, value);
          if (!valid.ok)
            return valid;
          valid = validate_field_battle_receipt(kind, value);
          return valid.ok ? c.status()
                          : c.reject(valid.error.original.code,
                                     valid.error.original.path);
        } else if constexpr (std::is_same_v<T, PersonalReceipt>)
          return visit_personal_receipt(c, value);
        else if constexpr (std::is_same_v<T, GrowthEffect>)
          return visit_growth_effect(c, value);
        else if constexpr (std::is_same_v<T, FamilyEffect>)
          return visit_family_effect(c, value);
        else if constexpr (std::is_same_v<T, PoliticsEffects>) {
          auto r = visit_politics_effects(c, value);
          if (!r.ok)
            return r;
          if (political_office_resignation_effect_uses_sources(value) &&
              !is_political_office_resignation_command(kind))
            return c.reject("InvalidPoliticalOfficeResignationReceipt");
          if (political_subordinate_appointment_effect_uses_sources(value) &&
              !is_political_subordinate_appointment_command(kind))
            return c.reject("InvalidPoliticalSubordinateAppointmentReceipt");
          if (political_governance_agency_effect_uses_sources(value) &&
              !is_political_governance_agency_command(kind))
            return c.reject("InvalidPoliticalGovernanceAgencyReceipt");
          if (is_political_succession_command(kind) &&
              !valid_political_succession_effect(kind, value))
            return c.reject("InvalidPoliticalSuccessionReceipt");
          if (is_political_head_handover_command(kind) &&
              !valid_political_head_handover_effect(kind, value))
            return c.reject("InvalidPoliticalHeadHandoverReceipt");
          if (is_political_term_handover_command(kind) &&
              !valid_political_term_handover_effect(kind, value))
            return c.reject("InvalidPoliticalTermHandoverReceipt");
          if (is_political_office_reception_command(kind) &&
              !valid_political_office_reception_effect(kind, value))
            return c.reject("InvalidPoliticalOfficeReceptionReceipt");
          if (is_political_office_resignation_command(kind) &&
              !valid_political_office_resignation_effect(kind, value))
            return c.reject("InvalidPoliticalOfficeResignationReceipt");
          if (is_political_subordinate_appointment_command(kind) &&
              !valid_political_subordinate_appointment_effect(kind, value))
            return c.reject("InvalidPoliticalSubordinateAppointmentReceipt");
          if (is_political_governance_agency_command(kind) &&
              !valid_political_governance_agency_effect(kind, value))
            return c.reject("InvalidPoliticalGovernanceAgencyReceipt");
          if (is_political_successor_round_command(kind) &&
              !valid_political_successor_round_effect(kind, value))
            return c.reject("InvalidPoliticalSuccessorRoundReceipt");
          return c.status();
        } else if constexpr (std::is_same_v<T, PoliticalFiscalEffects>)
          return visit_political_fiscal_effects(c, value);
        else if constexpr (std::is_same_v<T, NavalBattleCommandReceipt>) {
          auto r = naval_battle_command_codec::visit_receipt(
              c, value, c.naval_codec_version);
          if (!r.ok)
            return r;
          return value.kind == kind ? c.status()
                                    : c.reject("InvalidNavalBattleReceipt");
        } else if constexpr (std::is_same_v<T, NavalRepairReceipt>) {
          auto r = naval_repair_codec::visit_receipt(c, value);
          if (!r.ok)
            return r;
          r = naval_repair_codec::validate_receipt(value, kind);
          return r.ok ? c.status()
                      : c.reject(r.error.original.code, r.error.original.path);
        } else if constexpr (std::is_same_v<T, RepairServiceCommandReceipt>) {
          auto r = visit_repair_service_command_receipt(c, value);
          if (!r.ok)
            return r;
          return value.command_kind == kind
                     ? c.status()
                     : c.reject("RepairCommandReceiptKindMismatch");
        } else if constexpr (std::is_same_v<T, ContinuationBoundaryReceipt>)
          return visit_continuation_boundary_receipt(c, value);
        else if constexpr (std::is_same_v<T, DirectForceIntentReceipt3>)
          return visit_direct_force_intent_receipt3(c, value);
        else if constexpr (std::is_same_v<T, PendingRetirementCancelled>)
          return visit_pending_retirement_cancelled(c, value);
        else if constexpr (std::is_same_v<T, ContinuationBoundaryCancelled>)
          return visit_continuation_boundary_cancelled(c, value);
        else if constexpr (std::is_same_v<T, NavalCommandReceipt>) {
          auto r = naval_command_codec::visit_receipt(c, value);
          if (!r.ok)
            return r;
          return value.kind == kind ? c.status()
                                    : c.reject("InvalidNavalReceipt");
        } else if constexpr (std::is_same_v<T, LocalEconomyEffects>)
          return visit_economic_configuration_effects(c, value);
        else if constexpr (std::is_same_v<T,
                                          HouseholdProvisionManagementEffects>)
          return visit_household_provision_management_effects(c, value);
        else if constexpr (std::is_same_v<T, MembershipOfferEffects>) {
          c.field("offer_id", value.offer_id);
          c.field("decision_id", value.decision_id);
          c.field("member_episode_id", value.member_episode_id);
          common_checkpoint::strings(c, "fact_ids", value.fact_ids);
          return common_checkpoint::strings(c, "receipt_ids",
                                            value.receipt_ids);
        } else if constexpr (std::is_same_v<T, SiegeIntegrationEffect>) {
          c.field("proposal_id", value.proposal_id);
          c.field("proposal_key", value.proposal_key);
          c.field("actor_action_source_id", value.actor_action_source_id);
          c.field("decision_source_id", value.decision_source_id);
          c.field("status", value.status);
          return c.field("completion_source_id", value.completion_source_id);
        } else if constexpr (std::is_same_v<T, OrganizationEffects>) {
          common_checkpoint::strings(c, "receipt_ids", value.receipt_ids);
          return common_checkpoint::strings(c, "fact_ids", value.fact_ids);
        } else if constexpr (std::is_same_v<T, EstateReceipt>) {
          c.field("estate_id", value.estate_id);
          c.field("fact_id", value.fact_id);
          return c.optional(
              "actor_ids", value.actor_ids, [](auto &child, auto &ids) {
                return child.sequence("actors", ids, [](auto &node, auto &id) {
                  return node.field("actor_id", id);
                });
              });
        } else
          return c.status();
      },
      x.data);
  if (c.status().ok)
    if (const auto *effect = std::get_if<MembershipOfferEffects>(&x.data))
      if (!valid_membership_effect(kind, *effect))
        return c.reject("InvalidMembershipReceipt");
  if (c.status().ok)
    if (const auto *effect = std::get_if<SiegeIntegrationEffect>(&x.data))
      if (!valid_integration_effect(kind, *effect))
        return c.reject("InvalidSiegeIntegrationReceipt");
  auto result = visit_order(c, x.field_order);
  if (result.ok && player_plan_command_kind(kind) && !x.field_order.empty())
    return c.reject("InvalidPlayerPlanReceipt");
  return result;
}
Status validate_original_exact_receipt_envelope(const CommitResult &seal,
                                                const ReceiptDetails *details) {
  const auto *field =
      details ? std::get_if<FieldBattleReceipt>(&details->data) : nullptr;
  if (field_receipt_kind(seal.kind) != (field != nullptr))
    return common_failure("FieldBattleReceiptKindMismatch", "receipt/details");
  if (field) {
    auto valid = validate_field_battle_receipt(seal.kind, *field);
    if (!valid.ok)
      return valid;
  }
  if (organization_grant_command_kind(seal.kind)) {
    const auto *effect =
        details ? std::get_if<OrganizationEffects>(&details->data) : nullptr;
    if (!effect || !effect->receipt_ids.empty() ||
        effect->fact_ids.size() != 1 || effect->fact_ids.front().empty())
      return common_failure("InvalidOrganizationGrantReceipt",
                            "receipt/details");
  }
  if (native_work_lifecycle_command_kind(seal.kind) &&
      (!details || !std::holds_alternative<Done>(details->data)))
    return common_failure("WorkLifecycleCommandReceiptMismatch",
                          "receipt/details");
  const auto *continuation =
      details ? std::get_if<ContinuationBoundaryReceipt>(&details->data)
              : nullptr;
  if ((seal.kind == "stabilize_continuation_boundary") !=
      (continuation != nullptr))
    return common_failure("ContinuationBoundaryReceiptKindMismatch",
                          "receipt/details");
  if (continuation) {
    auto shape = validate_continuation_boundary_receipt_shape(*continuation);
    if (!shape.ok)
      return shape;
    const auto &source = continuation->result.boundary_work_ref;
    const auto &before =
        continuation->request.boundary_ref.candidate.source_identity;
    if (source.domain != "common" || source.kind != "command_result" ||
        source.id != seal.result_id || seal.revision < 1 ||
        before.revision != seal.revision - 1 || before.tick != seal.tick ||
        continuation->actual_at.half_day_tick != seal.tick)
      return common_failure("ContinuationBoundaryCommandSealMismatch",
                            "receipt/details");
  }
  const auto *direct =
      details ? std::get_if<DirectForceIntentReceipt3>(&details->data)
              : nullptr;
  if ((seal.kind == "direct_force_intent") != (direct != nullptr))
    return common_failure("DirectForceIntentReceiptKindMismatch",
                          "receipt/details");
  if (direct && direct->accepted_at.half_day_tick != seal.tick)
    return common_failure("DirectForceIntentCommandSealMismatch",
                          "receipt/details");
  const auto *withdrawal =
      details ? std::get_if<PendingRetirementCancelled>(&details->data)
              : nullptr;
  if ((seal.kind == "cancel_pending_retirement") != (withdrawal != nullptr))
    return common_failure("RetirementCancellationReceiptKindMismatch",
                          "receipt/details");
  if (withdrawal) {
    auto shape = validate_pending_retirement_cancelled_shape(*withdrawal);
    if (!shape.ok)
      return shape;
  }
  const auto *cancelled =
      details ? std::get_if<ContinuationBoundaryCancelled>(&details->data)
              : nullptr;
  if ((seal.kind == "cancel_continuation_boundary") != (cancelled != nullptr))
    return common_failure("ContinuationCancellationReceiptKindMismatch",
                          "receipt/details");
  if (cancelled) {
    auto shape = validate_continuation_boundary_cancelled_shape(*cancelled);
    if (!shape.ok)
      return shape;
  }
  return common_success();
}
} // namespace
// The original S7 reader extracts only this actual typed result; no fact ID,
// actor authority or assent is synthesized at the shared boundary.
const RepairServiceCommandReceipt *
repair_service_command_receipt(const ReceiptDetails &details) noexcept {
  return std::get_if<RepairServiceCommandReceipt>(&details.data);
}
// 与原Dialogue.resolve一致：明确success优先，否则回到ok；不额外合取两字段。
bool action_completion_succeeded(const ActionCompletion &value) {
  if (value.growth)
    return value.growth->success;
  if (value.task)
    return value.task->success;
  if (value.family)
    return value.family->success.value_or(true);
  return value.success.value_or(value.ok);
}
Status emit_action_completion_fields(ProjectionWriter &writer,
                                     const ActionCompletion &value) {
  try {
    return emit_completion_fields(writer, value);
  } catch (MarshalFailure &e) {
    return {false, {}, std::move(e.error)};
  } catch (boundary::DecodeFailure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<BoundaryValue> encode_action_completion(const ActionCompletion &value) {
  try {
    return {true, completion_value(value), {}};
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<std::shared_ptr<const ActionCompletion>>
decode_action_completion(const BoundaryValue &value) {
  try {
    return {true,
            checkpoint_make_shared<const ActionCompletion>(
                read_completion(value, "outcome")),
            {}};
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status visit_action_completion(CodecVisitor &codec,
                               std::shared_ptr<const ActionCompletion> &ptr) {
  bool present = bool(ptr);
  codec.field("present", present);
  if (!codec.status().ok)
    return codec.status();
  if (!present) {
    if (codec.reading())
      ptr.reset();
    return codec.status();
  }
  auto value = ptr ? *ptr : ActionCompletion{};
  auto result = visit_completion(codec, value);
  if (result.ok && codec.reading())
    ptr = checkpoint_make_shared<const ActionCompletion>(std::move(value));
  return result;
}
// 原命令与variant的全部失败门先于发射；成功回执借真实typed值，不复制CommitResult。
Status emit_resident_receipt(ProjectionWriter &writer, const StableId &name,
                             const CommitResult &r) {
  try {
    auto exact = validate_original_exact_receipt_envelope(r, r.details.get());
    if (!exact.ok)
      throw MarshalFailure{std::move(exact.error)};
    const auto *repair =
        r.details ? repair_service_command_receipt(*r.details) : nullptr;
    if (repair_service_command_kind(r.kind) != (repair != nullptr) ||
        (repair && repair->command_kind != r.kind))
      failed("RepairCommandReceiptKindMismatch", "receipt/details",
             ErrorKind::rule);
    if (repair) {
      auto valid = validate_repair_service_command_receipt_shape(*repair);
      if (!valid.ok)
        throw MarshalFailure{std::move(valid.error)};
    }
    if (private_shop_daily_rent_command_kind(r.kind)) {
      if (!r.details)
        failed("InvalidPrivateShopRentReceipt", "receipt/details");
      auto valid = validate_private_shop_daily_rent_receipt(r.kind, *r.details);
      if (!valid.ok)
        throw MarshalFailure{valid.error};
    }
    if (r.details)
      if (const auto *e = std::get_if<PoliticsEffects>(&r.details->data);
          e && political_governance_agency_effect_uses_sources(*e) &&
          !is_political_governance_agency_command(r.kind))
        failed("InvalidPoliticalGovernanceAgencyReceipt", "receipt/details",
               ErrorKind::rule);
    if (r.details)
      if (const auto *e = std::get_if<PoliticsEffects>(&r.details->data);
          e && political_office_resignation_effect_uses_sources(*e) &&
          !is_political_office_resignation_command(r.kind))
        failed("InvalidPoliticalOfficeResignationReceipt", "receipt/details",
               ErrorKind::rule);
    if (r.details)
      if (const auto *e = std::get_if<PoliticsEffects>(&r.details->data);
          e && political_subordinate_appointment_effect_uses_sources(*e) &&
          !is_political_subordinate_appointment_command(r.kind))
        failed("InvalidPoliticalSubordinateAppointmentReceipt",
               "receipt/details", ErrorKind::rule);
    if (is_politics_command_kind(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data)))
      failed("InvalidPoliticsReceipt", "receipt/details", ErrorKind::rule);
    if (is_political_head_handover_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_head_handover_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalHeadHandoverReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_successor_round_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_successor_round_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalSuccessorRoundReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_term_handover_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_term_handover_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalTermHandoverReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_office_reception_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_office_reception_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalOfficeReceptionReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_office_resignation_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_office_resignation_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalOfficeResignationReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_subordinate_appointment_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_subordinate_appointment_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalSubordinateAppointmentReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_governance_agency_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_governance_agency_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalGovernanceAgencyReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_political_succession_command(r.kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticsEffects>(r.details->data) ||
         !valid_political_succession_effect(
             r.kind, std::get<PoliticsEffects>(r.details->data))))
      failed("InvalidPoliticalSuccessionReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == "configure_household_provision" &&
        (!r.details ||
         !std::holds_alternative<HouseholdProvisionManagementEffects>(
             r.details->data)))
      failed("InvalidHouseholdProvisionReceipt", "receipt/details",
             ErrorKind::rule);
    if ((r.kind == "configure_local_production" ||
         r.kind == "political_issue_supply_policy") &&
        (!r.details ||
         !std::holds_alternative<LocalEconomyEffects>(r.details->data)))
      failed("InvalidProductionConfigurationReceipt", "receipt/details",
             ErrorKind::rule);
    if (treaty_passage_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidTreatyPassageReceipt", "receipt/details", ErrorKind::rule);
    if (political_military_appointment_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPoliticalMilitaryAppointmentReceipt", "receipt/details",
             ErrorKind::rule);
    if (political_field_appointment_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPoliticalFieldAppointmentReceipt", "receipt/details",
             ErrorKind::rule);
    if (political_shop_licensing_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPoliticalShopLicensingReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == political_shop_registry_accept_command &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPoliticalShopRegistryReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == political_governance_continue_command &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidGovernanceContinuationReceipt", "receipt/details",
             ErrorKind::rule);
    if (is_governance_invitation_reply_command(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidGovernanceInvitationReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == political_takeover_command_kind &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPoliticalTakeoverReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == "task_personal_advance" &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidTaskAdvanceReceipt", "receipt/details", ErrorKind::rule);
    if (r.kind == task_message_return_collect_command &&
        (!r.details ||
         !std::holds_alternative<ActionStarted>(r.details->data) ||
         std::get<ActionStarted>(r.details->data).action_id.empty()))
      failed("InvalidTaskMessageReturnReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == task_personal_message_command &&
        (!r.details ||
         !std::holds_alternative<ActionStarted>(r.details->data) ||
         std::get<ActionStarted>(r.details->data).action_id.empty()))
      failed("InvalidTaskPersonalMessageReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == task_personal_report_command &&
        (!r.details ||
         !std::holds_alternative<ActionStarted>(r.details->data) ||
         std::get<ActionStarted>(r.details->data).action_id.empty()))
      failed("InvalidTaskPersonalReportReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == task_personal_acceptance_command &&
        (!r.details ||
         !std::holds_alternative<ActionStarted>(r.details->data) ||
         std::get<ActionStarted>(r.details->data).action_id.empty()))
      failed("InvalidTaskPersonalAcceptanceReceipt", "receipt/details",
             ErrorKind::rule);
    if (r.kind == task_personal_cancellation_command) {
      const auto *effect =
          r.details ? std::get_if<InteractionEffect>(&r.details->data)
                    : nullptr;
      if (!effect ||
          effect->settlement_result.rfind(
              StableId{task_personal_cancelled_kind} + ":", 0) != 0 ||
          !effect->encounter_id.empty() || effect->party_id ||
          effect->person_id || effect->news_ids)
        failed("InvalidTaskPersonalCancellationReceipt", "receipt/details",
               ErrorKind::rule);
    }
    if (party_participation_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPartyParticipationReceipt", "receipt/details",
             ErrorKind::rule);
    if (diplomacy_command_kind(r.kind) &&
        (!r.details ||
         ((r.kind == "envoy_deliver" || r.kind == "envoy_report")
              ? !std::holds_alternative<ActionStarted>(r.details->data)
              : !std::holds_alternative<Done>(r.details->data))))
      failed("InvalidDiplomacyReceipt", "receipt/details", ErrorKind::rule);
    if ((r.kind == "political_set_tax" ||
         r.kind == political_occupation_fiscal_command_kind) &&
        (!r.details ||
         !std::holds_alternative<PoliticalFiscalEffects>(r.details->data)))
      failed("InvalidPoliticalFiscalReceipt", "receipt/details",
             ErrorKind::rule);
    if (player_plan_command_kind(r.kind) &&
        (!r.details || !std::holds_alternative<Done>(r.details->data)))
      failed("InvalidPlayerPlanReceipt", "receipt/details", ErrorKind::rule);
    if (named_membership_command_kind(r.kind) &&
        (!r.details ||
         !std::holds_alternative<MembershipOfferEffects>(r.details->data) ||
         !valid_membership_effect(
             r.kind, std::get<MembershipOfferEffects>(r.details->data))))
      failed("InvalidMembershipReceipt", "receipt/details", ErrorKind::rule);
    if (siege_integration_command_kind(r.kind) &&
        (!r.details ||
         !std::holds_alternative<SiegeIntegrationEffect>(r.details->data) ||
         !valid_integration_effect(
             r.kind, std::get<SiegeIntegrationEffect>(r.details->data))))
      failed("InvalidSiegeIntegrationReceipt", "receipt/details",
             ErrorKind::rule);
    if (naval_battle_command_codec::command_kind(r.kind)) {
      const auto *effect =
          r.details ? std::get_if<NavalBattleCommandReceipt>(&r.details->data)
                    : nullptr;
      if (!effect || effect->kind != r.kind)
        failed("InvalidNavalBattleReceipt", "receipt/details", ErrorKind::rule);
    }
    if (r.kind == "naval_form" || r.kind == "naval_disband") {
      const auto *effect =
          r.details ? std::get_if<NavalCommandReceipt>(&r.details->data)
                    : nullptr;
      if (!effect || effect->kind != r.kind)
        failed("InvalidNavalReceipt", "receipt/details", ErrorKind::rule);
    }
    if (r.kind == "military_field_command_assign") {
      const auto *effect =
          r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
      if (!effect || !military_field_command_effect_shape(*effect))
        failed("MilitaryFieldCommandCachedEffectMismatch", "receipt/details",
               ErrorKind::rule);
    }
    if (r.kind == "military_tactic_practice") {
      const auto *effect =
          r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
      if (!effect || !military_tactic_practice_codec::effect_shape(*effect))
        failed("MilitaryTacticPracticeCachedEffectMismatch", "receipt/details",
               ErrorKind::rule);
    }
    if (r.kind == "military_officer_assign") {
      const auto *effect =
          r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
      if (!effect || !military_officer_effect_shape(*effect))
        failed("MilitaryOfficerCachedEffectMismatch", "receipt/details",
               ErrorKind::rule);
    }
    if (r.kind == "military_request") {
      const auto *effect =
          r.details ? std::get_if<MilitaryEffect>(&r.details->data) : nullptr;
      if (!effect || !valid_request_effect(*effect))
        failed("InvalidMilitaryRequestReceipt", "receipt/details",
               ErrorKind::rule);
    }
    if (r.kind == "ask_news") {
      const auto *effect =
          r.details ? std::get_if<InteractionEffect>(&r.details->data)
                    : nullptr;
      if (!effect || !effect->news_ids || !effect->encounter_id.empty() ||
          !effect->settlement_result.empty() || effect->party_id ||
          effect->person_id)
        failed("InvalidNewsReceipt", "receipt/details", ErrorKind::rule);
    }
    if (is_visit_appointment_command(r.kind) ||
        r.kind == "cancel_visit_appointment" ||
        visit_reply_command_kind(r.kind) ||
        visit_message_command_kind(r.kind) ||
        r.kind == household_purchase_policy_command_kind ||
        r.kind == household_purchase_plan_command_kind ||
        r.kind == household_report_command_kind ||
        r.kind == task_extension_command_kind ||
        r.kind == "personal_appointment_accept" ||
        r.kind == "personal_appointment_cancel") {
      const auto *effect =
          r.details ? std::get_if<InteractionEffect>(&r.details->data)
                    : nullptr;
      if (!effect || effect->settlement_result.empty() ||
          !effect->encounter_id.empty() || effect->party_id ||
          effect->person_id || effect->news_ids)
        failed("InvalidVisitMessageReceipt", "receipt/details",
               ErrorKind::rule);
    }
    auto status = writer.object(name, r.field_order, [&](auto &fields) {
      fields.field("ok", r.ok);
      fields.field("kind", r.kind);
      fields.field("tick", r.tick);
      fields.field("revision", r.revision);
      fields.field("result_id", r.result_id);
      return fields.object("details", [&](auto &detail) {
        return r.details ? emit_details_fields(detail, *r.details)
                         : detail.status();
      });
    });
    // 原中央ordered门将无损字段顺序失败归内部marshal错误。
    if (!status.ok &&
        (status.error.original.code == "BoundaryFieldOrderMismatch" ||
         status.error.original.code == "DuplicateBoundaryKey"))
      status.error.kind = ErrorKind::internal;
    return status;
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<BoundaryValue> encode_resident_receipt(
    const Outcome<std::shared_ptr<const CommitResult>> &input) {
  try {
    if (!input.ok)
      return {true, error_value(input.error), {}};
    if (!input.value)
      failed("MissingResidentReceipt", "receipt");
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    auto status = emit_resident_receipt(writer, "", *input.value);
    if (status.ok)
      status = writer.finish();
    if (!status.ok)
      return {false, {}, status.error};
    return {true, std::move(value), {}};
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Outcome<std::shared_ptr<const CommitResult>>
decode_resident_receipt(const BoundaryValue &input) {
  try {
    boundary::fields(input,
                     {"ok", "kind", "tick", "revision", "result_id", "details"},
                     {}, "receipt");
    auto result = checkpoint_make_shared<CommitResult>();
    result->ok = boundary::get_bool(input, "ok", "receipt");
    if (!result->ok)
      failed("InvalidCachedReceipt", "receipt/ok", ErrorKind::rule);
    result->kind = boundary::get_string(input, "kind", "receipt");
    result->tick = boundary::get_int(input, "tick", "receipt");
    result->revision = boundary::get_int(input, "revision", "receipt");
    result->result_id = boundary::get_string(input, "result_id", "receipt");
    result->field_order = order(input);
    result->details = checkpoint_make_shared<ReceiptDetails>(read_details(
        boundary::field(input, "details", "receipt"), result->kind));
    auto exact = validate_original_exact_receipt_envelope(
        *result, result->details.get());
    if (!exact.ok)
      throw MarshalFailure{std::move(exact.error)};
    if (const auto *e = std::get_if<PoliticsEffects>(&result->details->data);
        e && political_governance_agency_effect_uses_sources(*e) &&
        !is_political_governance_agency_command(result->kind))
      failed("InvalidPoliticalGovernanceAgencyReceipt", "receipt/details",
             ErrorKind::rule);
    if (const auto *e = std::get_if<PoliticsEffects>(&result->details->data);
        e && political_office_resignation_effect_uses_sources(*e) &&
        !is_political_office_resignation_command(result->kind))
      failed("InvalidPoliticalOfficeResignationReceipt", "receipt/details",
             ErrorKind::rule);
    if (const auto *e = std::get_if<PoliticsEffects>(&result->details->data);
        e && political_subordinate_appointment_effect_uses_sources(*e) &&
        !is_political_subordinate_appointment_command(result->kind))
      failed("InvalidPoliticalSubordinateAppointmentReceipt", "receipt/details",
             ErrorKind::rule);
    return {true, std::move(result), {}};
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status visit_resident_receipt(CodecVisitor &c,
                              std::shared_ptr<const CommitResult> &ptr) {
  bool present = static_cast<bool>(ptr);
  c.field("present", present);
  if (!c.status().ok)
    return c.status();
  if (!present) {
    if (c.reading())
      ptr.reset();
    return c.status();
  }
  CommitResult value = ptr ? *ptr : CommitResult{};
  c.field("ok", value.ok);
  c.field("kind", value.kind);
  c.field("tick", value.tick);
  c.field("revision", value.revision);
  c.field("result_id", value.result_id);
  if (field_receipt_kind(value.kind) &&
      c.field_battle_codec_version != 1)
    return c.reject("FieldBattleReceiptProfileMismatch");
  if ((value.kind == "naval_form" || value.kind == "naval_disband") &&
      (!(c.schema_features & naval_command_codec::checkpoint_feature) ||
       !value.details && !c.reading()))
    return c.reject("UnexpectedNavalReceipt");
  if (native_work_lifecycle_command_kind(value.kind) &&
      c.task_work_execution_codec_version != 6)
    return c.reject("WorkLifecycleCommandReceiptProfileRequired");
  if (repair_service_command_kind(value.kind) &&
      c.item_instances_codec_version != 3)
    return c.reject("RepairServiceReceiptProfileRequired");
  if (value.kind == "stabilize_continuation_boundary" &&
      c.control_continuation_codec_version != 3)
    return c.reject("ContinuationBoundaryReceiptProfileRequired");
  if (value.kind == "direct_force_intent" &&
      c.dynamic_military_codec_version != 3 &&
      c.dynamic_military_codec_version != 4)
    return c.reject("DirectForceIntentReceiptProfileRequired");
  if (value.kind == "cancel_pending_retirement" &&
      c.control_continuation_codec_version != 3)
    return c.reject("RetirementCancellationReceiptProfileRequired");
  if (value.kind == "cancel_continuation_boundary" &&
      c.control_continuation_codec_version != 3)
    return c.reject("ContinuationCancellationReceiptProfileRequired");
  if (value.kind == "configure_household_provision" &&
      !(c.schema_features &
        common_checkpoint::household_provision_configuration_receipts))
    return c.reject("UnexpectedHouseholdProvisionReceipt");
  if (value.kind == "military_field_command_assign" &&
      !(c.schema_features & military_field_command_checkpoint_feature))
    return c.reject("MilitaryFieldCommandProfileRequired");
  if (value.kind == "military_officer_assign" &&
      !(c.schema_features & military_officer_checkpoint_feature))
    return c.reject("MilitaryOfficerAssignmentProfileRequired");
  if (value.kind == "military_request" &&
      !(c.schema_features & military_request_codec::checkpoint_feature))
    return c.reject("UnexpectedMilitaryRequestReceipt");
  if (naval_battle_command_codec::command_kind(value.kind) &&
      (c.naval_codec_version != 2 && c.naval_codec_version != 3 &&
       c.naval_codec_version != 4 && c.naval_codec_version != 5 &&
       c.naval_codec_version != 6))
    return c.reject("NavalCombatVersionRequired");
  if (value.kind == political_occupation_fiscal_command_kind &&
      !(c.schema_features & occupation_fiscal_checkpoint_feature))
    return c.reject("UnexpectedOccupationFiscalReceipt");
  if (political_military_appointment_command_kind(value.kind) &&
      !(c.schema_features & military_officer_checkpoint_feature))
    return c.reject("PoliticalMilitaryAppointmentProfileRequired");
  if (political_field_appointment_command_kind(value.kind) &&
      !(c.schema_features & military_field_command_checkpoint_feature))
    return c.reject("PoliticalFieldAppointmentProfileRequired");
  if (political_shop_licensing_command_kind(value.kind) &&
      !(c.schema_features & political_shop_licensing_checkpoint_feature))
    return c.reject("PoliticalShopLicensingProfileRequired");
  if (value.kind == political_shop_registry_accept_command &&
      !(c.schema_features &
        political_shop_registry_succession_checkpoint_feature))
    return c.reject("PoliticalShopRegistrySuccessionProfileRequired");
  if (value.kind == political_governance_continue_command &&
      !(c.schema_features & political_governance_continuation_feature))
    return c.reject("GovernanceContinuationProfileRequired");
  if (is_governance_invitation_reply_command(value.kind) &&
      !(c.schema_features & governance_invitation_reply_feature))
    return c.reject("GovernanceInvitationProfileRequired");
  if (is_political_head_handover_command(value.kind) &&
      c.political_head_handover_codec_version != 1)
    return c.reject("PoliticalHeadHandoverProfileRequired");
  if (is_political_successor_round_command(value.kind) &&
      c.political_successor_round_codec_version != 1)
    return c.reject("PoliticalSuccessorRoundProfileRequired");
  if (is_political_term_handover_command(value.kind) &&
      c.political_term_handover_codec_version != 1)
    return c.reject("PoliticalTermHandoverProfileRequired");
  if (is_political_office_reception_command(value.kind) &&
      c.political_office_reception_codec_version != 1)
    return c.reject("PoliticalOfficeReceptionProfileRequired");
  if (is_political_office_resignation_command(value.kind) &&
      (c.political_office_resignation_codec_version != 1 &&
       c.political_office_resignation_codec_version != 2))
    return c.reject("PoliticalOfficeResignationProfileRequired");
  if (is_political_subordinate_appointment_command(value.kind) &&
      c.political_subordinate_appointment_codec_version != 1)
    return c.reject("PoliticalSubordinateAppointmentProfileRequired");
  if (is_political_governance_agency_command(value.kind) &&
      (c.political_governance_agency_codec_version != 1 &&
       c.political_governance_agency_codec_version != 2))
    return c.reject("PoliticalGovernanceAgencyProfileRequired");
  if (is_political_succession_command(value.kind) &&
      c.political_succession_codec_version != 1)
    return c.reject("PoliticalSuccessionProfileRequired");
  if (treaty_passage_command_kind(value.kind) &&
      !(c.schema_features & treaty_passage_checkpoint_feature))
    return c.reject("TreatyPassageProfileRequired");
  if ((value.kind == "naval_repair_start" ||
       value.kind == "naval_repair_cancel") &&
      (c.naval_codec_version != 3 && c.naval_codec_version != 4 &&
       c.naval_codec_version != 5 && c.naval_codec_version != 6))
    return c.reject("NavalRepairProfileRequired");
  if (private_shop_daily_rent_command_kind(value.kind) &&
      !(c.schema_features & private_shop_daily_rent_checkpoint_feature))
    return c.reject("PrivateShopDailyRentProfileRequired");
  bool has_details = static_cast<bool>(value.details);
  c.field("has_details", has_details);
  if (field_receipt_kind(value.kind) && !has_details)
    return c.reject("MissingFieldBattleReceipt");
  if (repair_service_command_kind(value.kind) && !has_details)
    return c.reject("MissingRepairServiceCommandReceipt");
  if (is_political_head_handover_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalHeadHandoverReceipt");
  if (is_political_term_handover_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalTermHandoverReceipt");
  if (is_political_office_reception_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalOfficeReceptionReceipt");
  if (is_political_office_resignation_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalOfficeResignationReceipt");
  if (is_political_subordinate_appointment_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalSubordinateAppointmentReceipt");
  if (is_political_governance_agency_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalGovernanceAgencyReceipt");
  if (is_political_successor_round_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalSuccessorRoundReceipt");
  if (is_political_succession_command(value.kind) && !has_details)
    return c.reject("MissingPoliticalSuccessionReceipt");
  if (private_shop_daily_rent_command_kind(value.kind) && !has_details)
    return c.reject("InvalidPrivateShopRentReceipt");
  if (is_governance_invitation_reply_command(value.kind) && !has_details)
    return c.reject("InvalidGovernanceInvitationReceipt");
  if (value.kind == political_governance_continue_command && !has_details)
    return c.reject("InvalidGovernanceContinuationReceipt");
  if (value.kind == political_shop_registry_accept_command && !has_details)
    return c.reject("InvalidPoliticalShopRegistryReceipt");
  if ((value.kind == "naval_repair_start" ||
       value.kind == "naval_repair_cancel") &&
      !has_details)
    return c.reject("MissingNavalRepairReceipt");
  if (value.kind == "military_field_command_assign" && !has_details)
    return c.reject("MilitaryFieldCommandCachedEffectMismatch");
  if (political_military_appointment_command_kind(value.kind) && !has_details)
    return c.reject("MissingPoliticalMilitaryAppointmentReceipt");
  if (political_field_appointment_command_kind(value.kind) && !has_details)
    return c.reject("MissingPoliticalFieldAppointmentReceipt");
  if (political_shop_licensing_command_kind(value.kind) && !has_details)
    return c.reject("MissingPoliticalShopLicensingReceipt");
  if (value.kind == "military_tactic_practice" && !has_details)
    return c.reject("MilitaryTacticPracticeCachedEffectMismatch");
  if (value.kind == "military_officer_assign" && !has_details)
    return c.reject("MilitaryOfficerCachedEffectMismatch");
  if (treaty_passage_command_kind(value.kind) && !has_details)
    return c.reject("MissingTreatyPassageReceipt");
  if (value.kind == political_takeover_command_kind && !has_details)
    return c.reject("MissingPoliticalTakeoverReceipt");
  if (naval_battle_command_codec::command_kind(value.kind) && !has_details)
    return c.reject("MissingNavalBattleReceipt");
  if (value.kind == "task_personal_advance" && !has_details)
    return c.reject("MissingTaskAdvanceReceipt");
  if ((value.kind == "naval_form" || value.kind == "naval_disband") &&
      !has_details)
    return c.reject("MissingNavalReceipt");
  if (party_participation_command_kind(value.kind) && !has_details)
    return c.reject("MissingPartyParticipationReceipt");
  if (value.kind == "military_request" && !has_details)
    return c.reject("MissingMilitaryRequestReceipt");
  if (diplomacy_command_kind(value.kind) && !has_details)
    return c.reject("MissingDiplomacyReceipt");
  if (value.kind == "configure_household_provision" && !has_details)
    return c.reject("MissingHouseholdProvisionReceipt");
  if ((value.kind == "configure_local_production" ||
       value.kind == "political_issue_supply_policy") &&
      !has_details)
    return c.reject("MissingProductionConfigurationReceipt");
  if ((is_politics_command_kind(value.kind) ||
       value.kind == "political_set_tax" ||
       value.kind == political_occupation_fiscal_command_kind) &&
      !has_details)
    return c.reject("MissingPoliticsReceipt");
  if (player_plan_command_kind(value.kind) && !has_details)
    return c.reject("MissingPlayerPlanReceipt");
  if (value.kind == "ask_news" && !has_details)
    return c.reject("MissingNewsReceipt");
  if (siege_integration_command_kind(value.kind) && !has_details)
    return c.reject("MissingSiegeIntegrationReceipt");
  ReceiptDetails details = value.details ? *value.details : ReceiptDetails{};
  if (has_details)
    c.object("details", [&](auto &child) {
      return visit_details(child, details, value.kind);
    });
  visit_order(c, value.field_order);
  if (c.status().ok) {
    auto exact = validate_original_exact_receipt_envelope(
        value, has_details ? &details : nullptr);
    if (!exact.ok)
      return c.reject(exact.error.original.code, exact.error.original.path);
  }
  if (c.reading() && c.status().ok) {
    value.details =
        has_details ? checkpoint_make_shared<ReceiptDetails>(std::move(details))
                    : nullptr;
    ptr = checkpoint_make_shared<CommitResult>(std::move(value));
  }
  return c.status();
}
// 类型不符用B已批准的无效ID/超长via标记，仍由原领域guard位置返回首错。
Outcome<CommandPayload> decode_resident_command(const StableId &kind,
                                                const BoundaryValue &params) {
  if (kind == "FieldArmyExitSelect") {
    try {
      // 冷入口只做形状解码；原预览/提交继续核真实主将及出口，重复键不能借首值蒙混。
      const auto &object = boundary::object(params, kind);
      for (std::size_t i = 0; i < object.size(); ++i)
        for (std::size_t j = i + 1; j < object.size(); ++j)
          if (object[i].first == object[j].first)
            return {false, {}, common_failure("DuplicateFieldArmyExitSelectKey", object[i].first).error};
      boundary::fields(params, {"battle_id", "army_id", "exit_cell"}, {}, kind);
      FieldArmyExitSelect command{
          boundary::get_string(params, "battle_id", kind),
          boundary::get_string(params, "army_id", kind),
          boundary::get_int(params, "exit_cell", kind)};
      if (command.battle_id.empty() || command.army_id.empty() ||
          command.exit_cell < 0)
        return {false, {}, common_failure("InvalidFieldArmyExitSelect").error};
      return {true, {kind, std::move(command)}, {}};
    } catch (const boundary::DecodeFailure &failure) {
      return {false, {}, failure.error};
    }
  }
  if (kind == household_report_command_kind) {
    auto decoded = decode_tell_household_purchase_market_report(params);
    if (!decoded.ok)
      return {false, {}, std::move(decoded.error)};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == household_purchase_policy_command_kind) {
    auto decoded = decode_household_purchase_policy_command1(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true,
            {kind, HouseholdPurchaseOriginalCommand{std::move(decoded.value)}},
            {}};
  }
  if (kind == household_purchase_plan_command_kind) {
    auto decoded = decode_household_purchase_plan_command1(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true,
            {kind, HouseholdPurchaseOriginalCommand{std::move(decoded.value)}},
            {}};
  }
  if (native_work_lifecycle_command_kind(kind))
    return decode_native_work_lifecycle_command(kind, params);
  if (kind == "stabilize_continuation_boundary") {
    auto original = decode_stabilize_continuation_boundary(params);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, {kind, std::move(original.value)}, {}};
  }
  if (kind == "direct_force_intent") {
    auto original = decode_direct_force_intent3(params);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, {kind, std::move(original.value)}, {}};
  }
  if (kind == "cancel_pending_retirement") {
    auto original = decode_cancel_pending_retirement(params);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, {kind, std::move(original.value)}, {}};
  }
  if (kind == "cancel_continuation_boundary") {
    auto original = decode_cancel_continuation_boundary(params);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, {kind, std::move(original.value)}, {}};
  }
  if (repair_service_command_kind(kind)) {
    auto original = decode_repair_service_command_request(kind, params);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    return {true, {kind, std::move(original.value)}, {}};
  }
  if ((kind == "private_shop_appoint_executor" ||
       kind == "private_shop_accept_executorship" ||
       kind == "private_shop_revoke_executor" ||
       kind == "private_shop_estate_close")) {
    auto q = decode_private_shop_liquidation_command(kind, params);
    return q.ok ? Outcome<CommandPayload>{true, {kind, std::move(q.value)}, {}}
                : Outcome<CommandPayload>{false, {}, q.error};
  }
  if (kind == task_shop_order_estate_cancel_command) {
    auto q = decode_task_shop_order_estate_command(kind, params);
    return q.ok ? Outcome<CommandPayload>{true, {kind, std::move(q.value)}, {}}
                : Outcome<CommandPayload>{false, {}, q.error};
  }
  if (private_shop_daily_rent_command_kind(kind)) {
    auto r = decode_private_shop_daily_rent_command(kind, params);
    return r.ok ? Outcome<CommandPayload>{true, {kind, std::move(r.value)}, {}}
                : Outcome<CommandPayload>{false, {}, r.error};
  }
  if (private_shop_command_kind(kind)) {
    auto r = decode_private_shop_command(kind, params);
    return r.ok ? Outcome<CommandPayload>{true, {kind, std::move(r.value)}, {}}
                : Outcome<CommandPayload>{false, {}, r.error};
  }
  if (kind == "task_shop_order_accept" || kind == "task_shop_order_deliver" ||
      kind == "task_shop_order_cancel") {
    auto r = decode_task_shop_order_command(kind, params);
    return r.ok ? Outcome<CommandPayload>{true, {kind, std::move(r.value)}, {}}
                : Outcome<CommandPayload>{false, {}, r.error};
  }
  if (kind == "naval_repair_start" || kind == "naval_repair_cancel") {
    auto r = naval_repair_codec::decode_command(kind, params);
    if (!r.ok)
      return {false, {}, r.error};
    return {true, {kind, std::move(r.value)}, {}};
  }
  if (!params.as<D>())
    return {false,
            {},
            {{"InvalidCommand", "params"}, ErrorKind::rule, {}, {}, {}, {}}};
  // 内部step由该owner decoder明确拒绝；外部只接六种编辑和本人预算批准。
  if (kind == "configure_household_provision") {
    auto decoded = decode_configure_household_provision(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (naval_battle_command_codec::command_kind(kind)) {
    if (params.find("kind"))
      return {false, {}, common_failure("InvalidNavalBattleCommand").error};
    auto full = params;
    full.as<D>()->emplace_back("kind", V{kind});
    auto decoded = naval_battle_command_codec::decode_command(full, 2);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "naval_form" || kind == "naval_disband") {
    if (params.find("kind"))
      return {false, {}, common_failure("InvalidNavalCommand").error};
    auto full = params;
    full.as<D>()->emplace_back("kind", V{kind});
    auto decoded = naval_command_codec::decode_command(full);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (party_participation_command_kind(kind)) {
    auto decoded = decode_party_participation_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (treaty_passage_command_kind(kind)) {
    auto decoded = decode_treaty_passage_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (political_military_appointment_command_kind(kind)) {
    auto decoded = decode_political_military_appointment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (political_field_appointment_command_kind(kind)) {
    auto decoded = decode_political_field_appointment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (political_shop_licensing_command_kind(kind)) {
    auto decoded = decode_political_shop_licensing_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == political_shop_registry_accept_command) {
    auto decoded = decode_political_accept_shop_registry(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == political_governance_continue_command) {
    auto decoded = decode_political_continue_governance_command(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_governance_invitation_reply_command(kind)) {
    auto decoded = decode_governance_invitation_reply_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_head_handover_command(kind)) {
    auto decoded = decode_political_head_handover_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_successor_round_command(kind)) {
    auto decoded = decode_political_successor_round_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_term_handover_command(kind)) {
    auto decoded = decode_political_term_handover_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_office_reception_command(kind)) {
    auto decoded = decode_political_office_reception_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_office_resignation_command(kind)) {
    auto decoded = decode_political_office_resignation_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_subordinate_appointment_command(kind)) {
    auto decoded =
        decode_political_subordinate_appointment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_governance_agency_command(kind)) {
    auto decoded = decode_political_governance_agency_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_political_succession_command(kind)) {
    auto decoded = decode_political_succession_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (diplomacy_command_kind(kind)) {
    auto decoded = decode_diplomacy_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == political_occupation_fiscal_command_kind) {
    auto decoded = decode_political_set_occupation_tax(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == political_takeover_command_kind) {
    auto decoded = decode_political_takeover_command_params(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (governance_command_kind(kind)) {
    auto decoded = decode_governance_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_politics_command_kind(kind)) {
    auto decoded = decode_politics_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "configure_local_production") {
    auto decoded = decode_configure_local_production(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "political_set_tax") {
    auto decoded = decode_political_set_tax(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (player_plan_command_kind(kind)) {
    auto decoded = decode_player_plan_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (siege_integration_command_kind(kind)) {
    try {
      using namespace boundary;
      if (kind == "siege_integration_propose") {
        fields(params, {"siege_id", "terms"}, {}, "params");
        auto terms =
            decode_siege_integration_terms(field(params, "terms", "params"));
        if (!terms.ok)
          return {false, {}, terms.error};
        return {true,
                {kind, SiegeIntegrationCommand{SiegeIntegrationPropose{
                           get_string(params, "siege_id", "params"),
                           std::move(terms.value), boundary::order(params)}}},
                {}};
      }
      if (kind == "siege_integration_decide") {
        fields(params, {"proposal_id", "proposal_key", "decision"}, {},
               "params");
        return {true,
                {kind, SiegeIntegrationCommand{SiegeIntegrationDecide{
                           get_string(params, "proposal_id", "params"),
                           get_string(params, "proposal_key", "params"),
                           get_string(params, "decision", "params"),
                           boundary::order(params)}}},
                {}};
      }
      fields(params, {"proposal_id", "proposal_key"}, {}, "params");
      auto id = get_string(params, "proposal_id", "params"),
           key = get_string(params, "proposal_key", "params");
      if (kind == "siege_integration_cancel")
        return {true,
                {kind, SiegeIntegrationCommand{SiegeIntegrationCancel{
                           id, key, boundary::order(params)}}},
                {}};
      return {true, {kind, SiegeIntegrationApply{id, key}}, {}};
    } catch (const boundary::DecodeFailure &error) {
      return {false, {}, error.error};
    }
  }
  if (population_military_command_kind(kind)) {
    auto decoded = decode_population_military_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (dynamic_command_kind(kind))
    return decode_dynamic_command(kind, params);
  if (kind == "duel_start" || kind == "encounter_fight" ||
      kind == "battle_plan" || kind == "battle_close") {
    auto decoded = decode_personal_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (growth_command_kind(kind)) {
    auto decoded = decode_growth_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "advance") {
    if (const auto *target = params.find("requested_to")) {
      try {
        boundary::fields(params, {"requested_to"}, {}, "params");
        if (params.as<D>()->size() != 1)
          boundary::fail("DuplicateExactAdvanceField", "params");
        auto decoded = decode_world_moment2(*target);
        if (!decoded.ok)
          return {false, {}, decoded.error};
        return {true, {kind, AdvanceCommand{std::move(decoded.value)}}, {}};
      } catch (const boundary::DecodeFailure &error) {
        return {false, {}, error.error};
      }
    }
    return {true, {kind, AdvanceCommand{}}, {}};
  }
  if (kind == "wait") {
    if (!params.as<D>()->empty())
      return {false,
              {},
              {{"UnsupportedNativeCapability", "params"},
               ErrorKind::unsupported_capability,
               "command",
               "common",
               "wait",
               "params"}};
    return {true, {kind, WaitCommand{}}, {}};
  }
  if (kind == "travel" || kind == "reroute") {
    // 道路会保留原始开工参数；未声明键不能被typed边界静默丢掉后冒充精确成功。
    if (kind == "travel")
      for (const auto &field : *params.as<D>())
        if (field.first != "destination" && field.first != "via")
          return {false,
                  {},
                  {{"UnsupportedNativeCapability", "params/" + field.first},
                   ErrorKind::unsupported_capability,
                   "command",
                   "travel",
                   kind,
                   field.first}};
    TravelDestination destination = StableId{};
    if (const auto *raw = params.find("destination")) {
      if (auto value = raw->as<StableId>())
        destination = *value;
      else if (auto values = raw->as<A>(); values && values->size() == 2 &&
                                           (*values)[0].as<Integer>() &&
                                           (*values)[1].as<Integer>())
        destination =
            PointI{*(*values)[0].as<Integer>(), *(*values)[1].as<Integer>()};
    }
    std::vector<StableId> via;
    if (const auto *raw = params.find("via")) {
      const auto *values = raw->as<A>();
      if (!values || values->size() > 8)
        via.resize(9);
      else
        for (const auto &value : *values)
          via.push_back(value.as<StableId>() ? *value.as<StableId>()
                                             : StableId{});
    }
    TravelCommand request{destination, via};
    request.has_destination = params.find("destination") != nullptr;
    request.has_via = params.find("via") != nullptr;
    for (const auto &field : *params.as<D>())
      if (field.first == "destination" || field.first == "via")
        request.field_order.push_back(field.first);
    TravelCommandPayload command =
        kind == "travel"
            ? TravelCommandPayload{std::move(request)}
            : TravelCommandPayload{RerouteCommand{destination, via}};
    return {true, {kind, std::move(command)}, {}};
  }
  if (kind == "pause_travel")
    return {true, {kind, TravelCommandPayload{PauseTravelCommand{}}}, {}};
  if (kind == "resume_travel")
    return {true, {kind, TravelCommandPayload{ResumeTravelCommand{}}}, {}};
  if (kind == "travel_wait")
    return {true, {kind, TravelCommandPayload{TravelWaitCommand{}}}, {}};
  if (kind == "return")
    return {true, {kind, TravelCommandPayload{ReturnTravelCommand{}}}, {}};
  if (kind == "rescue")
    return {true, {kind, TravelCommandPayload{RescueCommand{}}}, {}};
  if (kind == "move_local") {
    const auto *raw = params.find("place_id");
    StableId id =
        raw && raw->as<StableId>() ? *raw->as<StableId>() : StableId{};
    return {true, {kind, TravelCommandPayload{MoveLocalCommand{id}}}, {}};
  }
  if (kind == "military_tactic_practice" ||
      kind == "military_field_command_assign" ||
      kind == "military_officer_assign" || kind == "military_request" ||
      kind == "office_accept" || kind == "campaign_accept" ||
      kind == "army_form" || kind == "campaign_depart" ||
      kind == "military_plan" || kind == "military_close" ||
      kind == "siege_start" || kind == "siege_join" || kind == "siege_plan" ||
      kind == "siege_strategy" || kind == "siege_close" ||
      kind == "campaign_return" || kind == "campaign_cancel" ||
      kind == "campaign_deliver" || kind == "campaign_report" ||
      kind == "army_return" || kind == "army_resupply" ||
      kind == "office_resign" || kind == "army_pledge" ||
      kind == "military_train" || kind == "military_train_cancel" ||
      kind == "military_drill_draft" || kind == "military_drill_confirm" ||
      kind == "military_drill_cancel_pending" ||
      kind == "military_drill_exit") {
    auto decoded = decode_military_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "ask_news" || kind == "talk_actor" || kind == "inspect" ||
      kind == "discover_offer" || kind == "join_party" ||
      kind == "dismiss_companion" || kind == "encounter_talk" ||
      kind == "encounter_leave" || kind == "talk" || kind == "open_event" ||
      kind == "dialogue_next" || kind == "dialogue_choose" ||
      kind == "end_interaction" || kind == "dialogue_confirm" ||
      kind == "dialogue_cancel" || kind == "close_trade" ||
      kind == "open_trade") {
    auto decoded = decode_interaction_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "political_issue_supply_policy") {
    auto d = decode_issue_economic_supply_policy(params);
    if (!d.ok)
      return {false, {}, d.error};
    return {true, {kind, std::move(d.value)}, {}};
  }
  if (kind.rfind("estate_employment_", 0) == 0) {
    auto decoded = decode_estate_employment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (visit_message_command_kind(kind)) {
    auto decoded = decode_visit_message_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_extension_command_kind) {
    auto decoded = decode_task_extension_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_personal_appointment_command(kind)) {
    auto decoded = decode_personal_appointment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == contact_inquiry_action_kind) {
    auto decoded = decode_contact_inquiry_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_message_return_collect_command) {
    auto decoded = decode_task_message_return_collect(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_personal_message_command) {
    auto decoded = decode_task_personal_message_deliver(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_personal_report_command) {
    auto decoded = decode_task_personal_report(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_personal_acceptance_command) {
    auto decoded = decode_task_personal_acceptance(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == task_personal_cancellation_command) {
    auto decoded = decode_task_personal_cancel(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (visit_reply_command_kind(kind)) {
    auto decoded = decode_visit_reply_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_visit_appointment_command(kind)) {
    auto decoded = decode_visit_appointment_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (is_visit_appointment_meeting_command(kind)) {
    auto decoded = decode_visit_appointment_meeting_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == authored_visit_appointment_command)
    return {false, {}, common_failure("VisitAppointmentNpcOnlyCommand").error};
  if (kind == "open_born_personal_dialogue") {
    auto decoded = decode_born_personal_dialogue_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "open_known_personal_dialogue") {
    auto decoded = decode_known_personal_dialogue_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == authored_visit_reply_command)
    return {false, {}, common_failure("VisitReplyNpcOnlyCommand").error};
  if (child_care_relief_command_kind(kind)) {
    auto decoded = decode_child_care_relief_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind.rfind("tactic_lesson_", 0) == 0) {
    auto decoded = decode_growth_tactic_lesson_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (family_birth_command_kind(kind)) {
    auto decoded = decode_family_birth_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind.rfind("family_apprenticeship_", 0) == 0) {
    auto decoded = decode_apprenticeship_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_marriage_receive" || kind == "family_marriage_reply" ||
      kind == "family_marriage_dissolve") {
    auto decoded = decode_family_marriage_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_bequest_disclose" ||
      kind == "family_inheritance_accept" ||
      kind == "family_inheritance_settle") {
    auto decoded = decode_family_inheritance_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_bequest_declare" || kind == "family_bequest_revoke") {
    auto decoded = decode_family_bequest_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind.rfind("family_voluntary_path_", 0) == 0) {
    auto decoded = decode_voluntary_path_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_collateral_recognize" ||
      kind == "family_collateral_accept" ||
      kind == "family_collateral_revoke") {
    auto decoded = decode_collateral_control_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_biological_accept" ||
      kind == "family_biological_revoke") {
    auto decoded = decode_biological_control_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_legacy_accept" || kind == "family_legacy_revoke" ||
      kind == "family_legacy_activate") {
    auto decoded = decode_family_legacy_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "family_adopt" || kind == "family_designate" ||
      kind == "family_retire" || kind == "family_adopt_existing" ||
      kind == "family_agree_succession" || kind == "family_cancel_succession" ||
      kind == "family_continue_as") {
    auto decoded = decode_family_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "task_personal_advance") {
    auto decoded = decode_task_personal_advance_command(params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "accept_offer" || kind == "report_offer" ||
      kind == "deliver_message" || kind == "cancel_task" ||
      kind == "decline_offer" || kind == "decline_runtime_offer") {
    auto decoded = decode_task_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  if (kind == "estate_fund" || kind == "estate_appoint" ||
      kind == "estate_assign" || kind == "estate_withdraw" ||
      kind == "estate_deposit" || kind == "estate_build" ||
      kind == "estate_muster" || kind == "estate_return") {
    auto decoded = decode_estate_command(kind, params);
    if (!decoded.ok)
      return {false, {}, decoded.error};
    return {true, {kind, std::move(decoded.value)}, {}};
  }
  return {false,
          {},
          {{"UnsupportedAction", "command/kind"},
           ErrorKind::rule,
           {},
           {},
           kind,
           {}}};
}
// preview转换只输出当前报价并最后附revision，不执行世界预演或更改命令守卫时点。
Outcome<BoundaryValue> encode_resident_preview(
    const Outcome<std::shared_ptr<const PreviewResult>> &input,
    Integer revision) {
  try {
    V value;
    if (!input.ok)
      value = error_value(input.error);
    else {
      if (!input.value || !input.value->quote)
        failed("MissingResidentQuote", "preview");
      const auto &preview = *input.value;
      value = std::visit(
          [&](const auto &quote) -> V {
            using T = std::decay_t<decltype(quote)>;
            if constexpr (std::is_same_v<T, DurationQuote>) {
              D out{{"ok", {true}}};
              if (quote.duration)
                out.emplace_back("duration", V{*quote.duration});
              return {std::move(out)};
            } else if constexpr (std::is_same_v<T, NavalRepairQuote>)
              return naval_repair_codec::encode_quote(quote);
            else if constexpr (std::is_same_v<T, NavalCommandQuote>)
              return naval_command_codec::encode_quote(quote);
            else if constexpr (std::is_same_v<T, NavalBattleCommandQuote>)
              return naval_battle_command_codec::encode_quote(quote, 2);
            else if constexpr (std::is_same_v<T, TaskPersonalAdvanceQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"task_personal_advance",
                    encode_task_personal_advance_quote(quote)}});
            else if constexpr (std::is_same_v<T, PartyParticipationQuote>)
              return boundary::obj({{"ok", V{true}},
                                    {"duration", V{Integer{0}}},
                                    {"party_participation",
                                     encode_party_participation_quote(quote)}});
            else if constexpr (std::is_same_v<T, TreatyPassageQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"treaty_passage", encode_treaty_passage_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, PoliticalMilitaryAppointmentQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_military_appointment",
                    encode_political_military_appointment_quote(quote)}});
            else if constexpr (std::is_same_v<T,
                                              PoliticalFieldAppointmentQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_field_appointment",
                    encode_political_field_appointment_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, PoliticalShopRegistryAcceptanceQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_shop_registry_successions",
                    encode_political_shop_registry_acceptance_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, PoliticalGovernanceContinuationQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_governance_continuations",
                    encode_political_governance_continuation_quote(quote)}});
            else if constexpr (std::is_same_v<T,
                                              GovernanceInvitationReplyQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"governance_invitation_replies",
                    encode_governance_invitation_reply_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalHeadHandoverQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_head_handovers",
                    encode_political_head_handover_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalSuccessorRoundQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_successor_rounds",
                    encode_political_successor_round_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalTermHandoverQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_term_handovers",
                    encode_political_term_handover_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalOfficeReceptionQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_office_receptions",
                    encode_political_office_reception_quote(quote)}});
            else if constexpr (std::is_same_v<T,
                                              PoliticalOfficeResignationQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_office_resignations",
                    encode_political_office_resignation_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, PoliticalSubordinateAppointmentQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_subordinate_appointments",
                    encode_political_subordinate_appointment_quote(quote)}});
            else if constexpr (std::is_same_v<T,
                                              PoliticalGovernanceAgencyQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_governance_agencies",
                    encode_political_governance_agency_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalSuccessionQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_successions",
                    encode_political_succession_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, PoliticalShopLicensingPreparedQuote>) {
              if (!quote)
                boundary::fail("PoliticalShopOwnedQuoteRequired",
                               "/quote/political_shop_licensing");
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_shop_licensing",
                    encode_political_shop_licensing_quote(quote->quote())}});
            } else if constexpr (std::is_same_v<T, PoliticalShopLicensingQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"political_shop_licensing",
                    encode_political_shop_licensing_quote(quote)}});
            else if constexpr (std::is_same_v<T, DiplomacyQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"diplomacy", encode_diplomacy_quote(quote)}});
            else if constexpr (std::is_same_v<T, OccupationFiscalQuote>)
              return boundary::obj({{"ok", V{true}},
                                    {"duration", V{Integer{0}}},
                                    {"occupation_fiscal",
                                     encode_occupation_fiscal_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalTakeoverQuote>)
              return boundary::obj({{"ok", V{true}},
                                    {"duration", V{Integer{0}}},
                                    {"political_takeover",
                                     encode_political_takeover_quote(quote)}});
            else if constexpr (std::is_same_v<T, GovernanceQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"governance", encode_governance_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticsQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"politics", encode_politics_quote(quote)}});
            else if constexpr (std::is_same_v<
                                   T, HouseholdProvisionConfigurationQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"household_provision_configuration",
                    encode_household_provision_configuration_quote(quote)}});
            else if constexpr (std::is_same_v<T, ProductionConfigurationQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"production_configuration",
                    encode_production_configuration_quote(quote)}});
            else if constexpr (std::is_same_v<T, PoliticalFiscalQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"political_fiscal", encode_political_fiscal_quote(quote)}});
            else if constexpr (std::is_same_v<T, PlayerPlanQuote>) {
              D out{{"ok", V{true}}, {"duration", V{Integer{0}}}};
              if (quote.terms)
                out.emplace_back("terms",
                                 encode_player_plan_terms(*quote.terms));
              return V{std::move(out)};
            } else if constexpr (std::is_same_v<T, TravelQuote>)
              return travel_quote(quote);
            else if constexpr (std::is_same_v<T, PreparedTravelOp>) {
              if (quote.quote)
                return travel_quote(*quote.quote);
              D out{{"ok", {true}}};
              if (std::holds_alternative<PauseTravelCommand>(quote.command))
                out.emplace_back("duration", V{Integer{0}});
              else if (std::holds_alternative<TravelWaitCommand>(quote.command))
                out.emplace_back("duration", V{Integer{1}});
              return {std::move(out)};
            } else if constexpr (std::is_same_v<T, InteractionQuote>)
              return interaction_quote(preview.quote->kind, quote);
            else if constexpr (std::is_same_v<T, MilitaryQuote>) {
              if (!preview.operation)
                failed("MissingPreparedCommand", "preview");
              const auto *operation = preview.operation.get();
              while (operation->nested)
                operation = operation->nested.get();
              const auto *command =
                  std::get_if<MilitaryCommand>(&operation->command.data);
              if (!command)
                failed("InvalidPreparedCommand", "preview");
              auto encoded = encode_military_quote(quote, *command);
              if (!encoded.ok)
                throw MarshalFailure{encoded.error};
              return std::move(encoded.value);
            } else if constexpr (std::is_same_v<T, MembershipOfferQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{quote.duration}},
                   {"offer_id", V{quote.offer_id}},
                   {"offer_revision", V{quote.offer_revision}},
                   {"terms_id", V{quote.terms_id}},
                   {"grant_id", V{quote.grant_id}},
                   {"expires_tick", V{quote.expires_tick}},
                   {"next_review_tick", V{quote.next_review_tick}},
                   {"first_period_due", V{quote.first_period_due}},
                   {"reason", V{quote.reason}}});
            else if constexpr (std::is_same_v<T, SiegeIntegrationQuote>) {
              D value{{"ok", V{true}},
                      {"proposal_id", V{quote.proposal_id}},
                      {"proposal_key", V{quote.proposal_key}},
                      {"status", V{quote.status}}};
              if (quote.proposal)
                value.emplace_back(
                    "proposal",
                    encode_siege_integration_proposal(*quote.proposal));
              return V{std::move(value)};
            } else if constexpr (std::is_same_v<T,
                                                SiegeIntegrationExecutionQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"proposal_id", V{quote.proposal_id}},
                   {"proposal_key", V{quote.proposal_key}},
                   {"receiving_weight_before",
                    V{quote.assets.receiving_weight_before}},
                   {"receiving_weight_after",
                    V{quote.assets.receiving_weight_after}},
                   {"receiving_capacity", V{quote.assets.receiving_capacity}},
                   {"transfers", encode_military_integration_facts(
                                     {quote.assets.transfer})}});
            else if constexpr (std::is_same_v<T, PopulationMilitaryQuote>)
              return encode_population_military_quote(quote);
            else if constexpr (std::is_same_v<T, PersonalQuote>)
              return encode_personal_quote(quote);
            else if constexpr (std::is_same_v<T, GrowthQuote>) {
              auto encoded = encode_growth_quote(quote);
              if (!encoded.ok)
                throw MarshalFailure{encoded.error};
              return std::move(encoded.value);
            } else if constexpr (std::is_same_v<T, FamilyQuote>)
              return encode_family_quote(quote);
            else if constexpr (std::is_same_v<T, EconomicSupplyPolicyQuote>)
              return encode_economic_supply_policy_quote(quote);
            else if constexpr (std::is_same_v<T, EstateEmploymentQuote>)
              return encode_estate_employment_quote(quote);
            else if constexpr (std::is_same_v<T, PrivateShopLiquidationQuote>)
              return encode_private_shop_liquidation_quote(quote);
            else if constexpr (std::is_same_v<
                                   T, TaskShopOrderEstateCancellationQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"terms", encode_task_shop_order_estate_quote(quote)}});
            else if constexpr (std::is_same_v<T, PrivateShopDailyRentQuote>)
              return encode_private_shop_daily_rent_quote(quote);
            else if constexpr (std::is_same_v<
                                   T, NativeHouseholdReportCommandQuote>) {
              V value;
              BoundaryProjectionWriter writer(value);
              auto status = writer.object("", [&](auto &fields) {
                fields.field("ok", true);
                fields.text("kind", household_report_command_kind);
                fields.field("actor_id", quote.actor_id);
                fields.field("duration", Integer{0});
                return fields.object("params", [&](auto &params) {
                  return emit_tell_household_purchase_market_report_fields(
                      params, quote.command);
                });
              });
              if (status.ok)
                status = writer.finish();
              if (!status.ok)
                throw MarshalFailure{status.error};
              return value;
            } else if constexpr (std::is_same_v<
                                     T, NativeHouseholdPurchaseCommandQuote>) {
              const bool policy =
                  std::holds_alternative<HouseholdPurchasePolicyCommand1>(
                      quote.command);
              V value;
              BoundaryProjectionWriter writer(value);
              auto status = writer.object("", [&](auto &fields) {
                fields.field("ok", true);
                fields.text("kind", policy
                                        ? household_purchase_policy_command_kind
                                        : household_purchase_plan_command_kind);
                fields.field("actor_id", quote.actor_id);
                fields.field("duration", Integer{0});
                return fields.object("params", [&](auto &params) {
                  return std::visit(
                      [&](const auto &command) -> Status {
                        using C = std::decay_t<decltype(command)>;
                        if constexpr (std::is_same_v<
                                          C, HouseholdPurchasePolicyCommand1>)
                          return emit_household_purchase_policy_command1_fields(
                              params, command);
                        else
                          return emit_household_purchase_plan_command1_fields(
                              params, command);
                      },
                      quote.command);
                });
              });
              if (status.ok)
                status = writer.finish();
              if (!status.ok)
                throw MarshalFailure{status.error};
              return value;
            } else if constexpr (std::is_same_v<T, PrivateShopCommandQuote>)
              return encode_private_shop_quote(quote);
            else if constexpr (std::is_same_v<T, TaskShopOrderQuote>)
              return encode_task_shop_order_quote(quote);
            else if constexpr (std::is_same_v<T,
                                              GrowthTacticLessonCommandQuote>)
              return encode_growth_tactic_lesson_quote(quote);
            else if constexpr (std::is_same_v<T, ChildCareReliefCommandQuote>)
              return encode_child_care_relief_quote(quote);
            else if constexpr (std::is_same_v<T, FamilyBirthQuote>)
              return encode_family_birth_quote(quote);
            else if constexpr (std::is_same_v<T, VisitMessageQuote>)
              return encode_visit_message_quote(quote);
            else if constexpr (std::is_same_v<T, TaskExtensionQuote>)
              return encode_task_extension_quote(quote);
            else if constexpr (std::is_same_v<T, PersonalAppointmentQuote>)
              return encode_personal_appointment_quote(
                  quote,
                  preview.quote->kind == personal_appointment_action_kind);
            else if constexpr (std::is_same_v<T, ApprenticeshipQuote>)
              return encode_apprenticeship_quote(quote);
            else if constexpr (std::is_same_v<T, ContactInquiryQuote>)
              return encode_contact_inquiry_quote(quote);
            else if constexpr (std::is_same_v<T, TaskMessageReturnQuote>)
              return encode_task_message_return_quote(quote);
            else if constexpr (std::is_same_v<T, TaskPersonalMessageQuote>)
              return encode_task_personal_message_quote(quote);
            else if constexpr (std::is_same_v<T, TaskPersonalReportQuote>)
              return encode_task_personal_report_quote(quote);
            else if constexpr (std::is_same_v<T, TaskPersonalAcceptanceQuote>)
              return encode_task_personal_acceptance_quote(quote);
            else if constexpr (std::is_same_v<T, TaskPersonalCancellationQuote>)
              return encode_task_personal_cancellation_quote(quote);
            else if constexpr (std::is_same_v<T, VisitReplyQuote>)
              return encode_visit_reply_quote(quote);
            else if constexpr (std::is_same_v<T, VisitAppointmentQuote>)
              return encode_visit_appointment_quote(quote);
            else if constexpr (std::is_same_v<T, AuthoredVisitAppointmentQuote>)
              return encode_visit_appointment_quote(quote.invitation);
            else if constexpr (std::is_same_v<T, VisitAppointmentMeetingQuote>)
              return encode_visit_appointment_meeting_quote(
                  quote,
                  preview.quote->kind == visit_appointment_meeting_action_kind);
            else if constexpr (std::is_same_v<T, BornPersonalDialogueQuote>)
              return encode_born_personal_dialogue_quote(quote);
            else if constexpr (std::is_same_v<T, KnownPersonalDialogueQuote>)
              return encode_known_personal_dialogue_quote(quote);
            else if constexpr (std::is_same_v<T, AuthoredVisitReplyQuote>)
              return encode_visit_reply_quote(quote.reply);
            else if constexpr (std::is_same_v<T, FamilyMarriageQuote>)
              return encode_family_marriage_quote(quote);
            else if constexpr (std::is_same_v<T, FamilyInheritanceQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"scope", V{StableId{"known_personal_bequest_only"}}}});
            else if constexpr (std::is_same_v<T, FamilyBequestQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"scope", V{StableId{"declaration_only_no_transfer"}}}});
            else if constexpr (std::is_same_v<T, VoluntaryPathQuote>)
              return encode_voluntary_path_quote(quote);
            else if constexpr (std::is_same_v<T, CollateralControlQuote>)
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"scope",
                    V{StableId{quote.recognition
                                   ? "mutual_collateral_recognition_only"
                                   : "own_life_control_only"}}}});
            else if constexpr ((std::is_same_v<T, FamilyLegacyQuote> ||
                                std::is_same_v<T, LegacyControlDecisionQuote>))
              return boundary::obj(
                  {{"ok", V{true}},
                   {"duration", V{Integer{0}}},
                   {"scope", V{StableId{"own_life_control_only"}}}});
            else if constexpr (std::is_same_v<T, TaskQuote>)
              return encode_task_quote(quote);
            else {
              auto encoded = encode_estate_quote(quote);
              if (!encoded.ok)
                throw MarshalFailure{encoded.error};
              return std::move(encoded.value);
            }
          },
          preview.quote->data);
    }
    auto *out = value.as<D>();
    if (!out)
      failed("InvalidResidentQuote", "preview");
    out->emplace_back("revision", V{revision});
    return {true, std::move(value), {}};
  } catch (const MarshalFailure &e) {
    return {false, {}, e.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
