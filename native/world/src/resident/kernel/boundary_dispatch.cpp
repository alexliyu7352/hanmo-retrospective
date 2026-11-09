#include "world_checkpoint_write_scope.hpp"
#include "checkpoint_phase_timing.hpp"
// CLI和Godot共用同一常驻世界；冷JSON只用于装载/命令DTO/诊断，绝不每半日重建世界。
#include "boundary_dispatch.hpp"
#include "continuation_binding.hpp"
#include "exact_time_profile.hpp"
#include "exact_time_world.hpp"
#include "json_decoded_world_seed.hpp"
#include "live_view_queries.hpp"
#include "named_membership.hpp"
#include "native_catalog_decode.hpp"
#include "native_initial_state.hpp"
#include "resident/checkpoint/checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include "resident/domains/player_plan/plan_page.hpp"
#include "resident/views/common_presentation.hpp"
#include "resident/views/journey_interaction.hpp"
#include "resident/views/player_frame_seed.hpp"
#include "resident/views/scene_presentation.hpp"
#include "world_impl.hpp"
#include "world_restore_candidate.hpp"
#include "world_restore_retention.hpp"
#include "world_timeline_codec.hpp"
#include <chrono>
#include <picosha2.h>
#include <set>
#include <stdexcept>
namespace hanmo::resident {
namespace {
using Clock = std::chrono::steady_clock;
std::uint64_t elapsed(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                               start)
      .count();
}
template <class T> Outcome<T> failure(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
using V = BoundaryValue;
V live_identity(const WorldIdentity &id) {
  std::string hex;
  static constexpr char digits[] = "0123456789abcdef";
  for (auto byte : id.world_id) {
    hex += digits[byte >> 4];
    hex += digits[byte & 15];
  }
  return boundary::obj({{"world_id", V{hex}},
                        {"execution_epoch", V{id.execution_epoch}},
                        {"revision", V{id.revision}},
                        {"tick", V{id.tick}}});
}
V live_error(const RuleError &error) {
  auto result = boundary::obj(
      {{"ok", V{false}},
       {"code", V{error.original.code}},
       {"path", V{error.original.path}},
       {"kind", V{std::string{error.kind == ErrorKind::rule ? "rule"
                              : error.kind == ErrorKind::internal
                                  ? "internal"
                                  : "unsupported_capability"}}},
       {"phase", V{error.phase}},
       {"domain", V{error.domain}},
       {"command", V{error.command}},
       {"source_id", V{error.source_id}}});
  auto *fields = result.as<V::Dict>();
  if (error.army_id)
    fields->emplace_back("army_id", V{*error.army_id});
  if (error.reason)
    fields->emplace_back("reason", V{*error.reason});
  if (error.limit)
    fields->emplace_back("limit", V{*error.limit});
  if (error.maximum)
    fields->emplace_back("maximum", V{*error.maximum});
  if (error.feasibility)
    fields->emplace_back("feasibility", V{*error.feasibility});
  if (error.new_total)
    fields->emplace_back("new_total", V{*error.new_total});
  if (error.skill_id)
    fields->emplace_back("skill_id", V{*error.skill_id});
  if (error.required_level)
    fields->emplace_back("required_level", V{*error.required_level});
  if (error.actual_level)
    fields->emplace_back("actual_level", V{*error.actual_level});
  if (error.actor_id)
    fields->emplace_back("actor_id", V{*error.actor_id});
  if (error.battle_id)
    fields->emplace_back("battle_id", V{*error.battle_id});
  return result;
}
// 停止码投影只有声明过的13种，未知值不索引数组。
const char *live_stop_name(RunStop stop) {
  static constexpr const char *names[] = {
      "awaiting_permit", "awaiting_input",
      "arrived",         "paused",
      "stranded",        "blocked",
      "cancelled",       "budget_exhausted",
      "failed",          "money_budget_exhausted",
      "action_replaced", "restored",
      "completed"};
  const auto index = static_cast<unsigned>(stop);
  return index < sizeof(names) / sizeof(names[0]) ? names[index] : "failed";
}
V live_progress(const Outcome<RunProgress> &value) {
  if (!value.ok)
    return live_error(value.error);
  const auto &p = value.value;

  auto result =
      boundary::obj({{"ok", V{true}},
                     {"identity", live_identity(p.identity)},
                     {"run_id", V{p.run_id}},
                     {"committed_half_days", V{p.committed_half_days}},
                     {"permit_seq", V{p.permit_seq}},
                     {"stop", V{std::string{live_stop_name(p.stop)}}}});
  auto &fields = *result.as<V::Dict>();
  fields.emplace_back("gross_outflow", V{p.gross_outflow});
  fields.emplace_back("remaining_half_days", V{p.remaining_half_days});
  fields.emplace_back("remaining_gross_outflow", V{p.remaining_gross_outflow});
  fields.emplace_back("actor_id", V{p.actor_id});
  fields.emplace_back("action_id", V{p.action_id});
  fields.emplace_back("action_kind", V{p.action_kind});
  fields.emplace_back("wallet_id", V{p.wallet_id});
  fields.emplace_back("max_half_days", V{p.max_half_days});
  fields.emplace_back("max_gross_outflow", V{p.max_gross_outflow});
  // 精确进度三件必须来自同一稳定身份；缺件不能补零或回退成整数成功。
  if (p.exact_source_moment || p.exact_spent_units || p.exact_remaining_units) {
    if (!p.exact_source_moment || !p.exact_spent_units ||
        !p.exact_remaining_units)
      return live_error(
          common_failure("InvalidExactRunProgress", {}, ErrorKind::internal)
              .error);
    auto moment = encode_world_moment2(*p.exact_source_moment);
    if (!moment.ok)
      return live_error(moment.error);
    auto spent = encode_work_units2(*p.exact_spent_units);
    if (!spent.ok)
      return live_error(spent.error);
    auto remaining = encode_work_units2(*p.exact_remaining_units);
    if (!remaining.ok)
      return live_error(remaining.error);
    fields.emplace_back("exact_source_moment", std::move(moment.value));
    fields.emplace_back("exact_spent_units", std::move(spent.value));
    fields.emplace_back("exact_remaining_units", std::move(remaining.value));
  }
  if (!p.plan_id.empty())
    fields.emplace_back("plan_id", V{p.plan_id});
  if (p.error)
    fields.emplace_back("error", live_error(*p.error));
  return result;
}
// RUN-DRIVE/2明确目标联合，旧混合参数不能猜成新批准范围。
RunDriveTarget read_run_drive_target(const V &value) {
  using namespace boundary;
  const auto kind = get_string(value, "kind", "run_target");
  if (kind == "action") {
    fields(value, {"kind", "action_id", "action_kind"}, {}, "run_target");
    return ActionRunDriveTarget{get_string(value, "action_id", "run_target"),
                                get_string(value, "action_kind", "run_target")};
  }
  if (kind == "plan") {
    fields(value, {"kind", "plan_id", "approved_scope_revision"}, {},
           "run_target");
    return PlanRunDriveTarget{
        get_string(value, "plan_id", "run_target"),
        get_int(value, "approved_scope_revision", "run_target")};
  }
  fail("UnsupportedRunDriveTarget", "run_target/kind");
  return ActionRunDriveTarget{};
}
// 输出同一深拥有gate，完整精确数值复用唯一codec，读面不承载可执行命令。
V live_run_drive_gate(const Outcome<RunDriveGate> &value) {
  if (!value.ok)
    return live_error(value.error);
  const auto &gate = value.value;
  V target;
  if (const auto *action = std::get_if<ActionRunDriveTarget>(&gate.target))
    target = boundary::obj({{"kind", V{std::string{"action"}}},
                            {"action_id", V{action->action_id}},
                            {"action_kind", V{action->action_kind}}});
  else {
    const auto &plan = std::get<PlanRunDriveTarget>(gate.target);
    target = boundary::obj(
        {{"kind", V{std::string{"plan"}}},
         {"plan_id", V{plan.plan_id}},
         {"approved_scope_revision", V{plan.approved_scope_revision}}});
  }
  const char *next =
      gate.next == RunDriveNext::original_command ? "original_command"
      : gate.next == RunDriveNext::world_half_day ? "world_half_day"
                                                  : "none";
  auto result =
      boundary::obj({{"ok", V{true}},
                     {"version", V{Integer{2}}},
                     {"target", std::move(target)},
                     {"source_identity", live_identity(gate.source_identity)},
                     {"next", V{std::string{next}}},
                     {"stop", V{std::string{live_stop_name(gate.stop)}}},
                     {"reason", V{gate.reason}},
                     {"budget_projection",
                      live_progress({true, gate.budget_projection, {}})}});
  auto &fields = *result.as<V::Dict>();
  if (gate.exact_source_moment) {
    auto moment = encode_world_moment2(*gate.exact_source_moment);
    if (!moment.ok)
      return live_error(moment.error);
    fields.emplace_back("exact_source_moment", std::move(moment.value));
  }
  if (gate.step_ref)
    fields.emplace_back("step_ref", V{*gate.step_ref});
  if (gate.bound_action_ref)
    fields.emplace_back("bound_action_ref", V{*gate.bound_action_ref});
  return result;
}
// 仅投影本人停驻条款；不含旧run ID或可再次执行的permit。
V live_continuation(const Outcome<std::optional<ContinuationSnapshot>> &value) {
  if (!value.ok)
    return live_error(value.error);
  V projected;
  if (value.value) {
    const auto &s = *value.value;
    projected = boundary::obj(
        {{"source_identity", live_identity(s.source_identity)},
         {"actor_id", V{s.actor_id}},
         {"action_id", V{s.action_id}},
         {"action_kind", V{s.action_kind}},
         {"wallet_id", V{s.wallet_id}},
         {"action_start_tick", V{s.action_start_tick}},
         {"started_tick", V{s.started_tick}},
         {"stop", V{std::string{live_stop_name(s.stop)}}},
         {"max_half_days", V{s.max_half_days}},
         {"max_gross_outflow", V{s.max_gross_outflow}},
         {"committed_half_days", V{s.committed_half_days}},
         {"gross_outflow", V{s.gross_outflow}},
         {"remaining_half_days", V{s.max_half_days - s.committed_half_days}},
         {"remaining_gross_outflow",
          V{s.max_gross_outflow - s.gross_outflow}}});
  }
  if (value.value) {
    const auto &s = *value.value;
    const bool any = s.exact_source_moment || s.exact_started_at ||
                     s.exact_committed_duration || s.action_started_source;
    if (s.continuation_version == 2) {
      if (!s.exact_source_moment || !s.exact_started_at ||
          !s.exact_committed_duration || !s.action_started_source)
        return live_error(
            common_failure("InvalidExactContinuation", {}, ErrorKind::internal)
                .error);
      auto moment = encode_world_moment2(*s.exact_source_moment);
      if (!moment.ok)
        return live_error(moment.error);
      auto start = encode_world_moment2(*s.exact_started_at);
      if (!start.ok)
        return live_error(start.error);
      auto duration = encode_half_day_duration2(*s.exact_committed_duration);
      if (!duration.ok)
        return live_error(duration.error);
      const auto &source = *s.action_started_source;
      auto &fields = *projected.as<V::Dict>();
      fields.emplace_back("continuation_version", V{Integer{2}});
      fields.emplace_back("exact_source_moment", std::move(moment.value));
      fields.emplace_back("exact_started_at", std::move(start.value));
      fields.emplace_back("exact_committed_duration",
                          std::move(duration.value));
      fields.emplace_back("action_started_source",
                          boundary::obj({{"domain", V{source.domain}},
                                         {"kind", V{source.kind}},
                                         {"id", V{source.id}},
                                         {"schema", V{source.schema}}}));
    } else if (s.continuation_version != 1 || any)
      return live_error(
          common_failure("InvalidContinuationVersion", {}, ErrorKind::internal)
              .error);
  }
  return boundary::obj({{"ok", V{true}},
                        {"available", V{value.value.has_value()}},
                        {"continuation", std::move(projected)}});
}

// 当前本人确知的未决邀请只作停驻说明；原条款地点不代表发出者现在仍在场。
V live_pending_decisions(const WorldBundle &world) {
  V::Array rows;
  if (!world.named_membership_enabled())
    return V{std::move(rows)};
  const auto &actor = world.common.state().metadata.player_actor_id;
  const auto tick = world.common.tick();
  CommonPresentation presentation(world.common.state(), *world.catalog->common);
  for (const auto &item : world.organization.membership_offers.rows()) {
    const auto &offer = item.second;
    if (offer.target_actor_id != actor ||
        offer.status != MembershipOfferStatus::pending ||
        offer.created_tick > tick || offer.expires_tick <= tick ||
        !membership_fact_known(world.common, world.social, actor,
                               offer.source_fact_id))
      continue;
    auto person = presentation.display_name(offer.issuer_actor_id);
    if (!person.ok || person.value.empty())
      continue;
    StableId place_id, place_name;
    const auto *terms =
        world.catalog->organization->membership_terms.find(offer.terms_id);
    if (terms && world.catalog->interaction->scene) {
      const auto *place =
          world.catalog->interaction->scene->places.find(terms->site_id);
      if (place) {
        auto name = presentation.text(place->name_key);
        if (name.ok && !name.value.empty()) {
          place_id = place->id;
          place_name = std::move(name.value);
        }
      }
    }
    rows.push_back(
        boundary::obj({{"kind", V{StableId{"membership_invitation"}}},
                       {"id", V{offer.id}},
                       {"actor_id", V{actor}},
                       {"issuer_actor_id", V{offer.issuer_actor_id}},
                       {"display_name", V{std::move(person.value)}},
                       {"organization_id", V{offer.organization_id}},
                       {"source_fact_id", V{offer.source_fact_id}},
                       {"status", V{StableId{"pending"}}},
                       {"known_place_id", V{std::move(place_id)}},
                       {"known_place_name", V{std::move(place_name)}},
                       {"expires_tick", V{offer.expires_tick}}}));
    // 页面是有限提示，不成为可枚举全世界或持久保存的第二收件箱。
    if (rows.size() == 8)
      break;
  }
  return V{std::move(rows)};
}

} // namespace
// 冷入口的期望身份逐项读取；缺摘要/版本不是通配符，模块重复或额外字段明确拒绝。
CatalogIdentity read_catalog_identity(const BoundaryValue &value) {
  using namespace boundary;
  fields(value,
         {"content_id", "content_version", "content_digest", "ruleset_id",
          "schema_sha256", "artifact_sha256", "module_versions"},
         {"pack_set_digest"}, "catalog_identity");
  CatalogIdentity identity;
  identity.content_id = get_string(value, "content_id", "catalog_identity");
  identity.content_version =
      get_string(value, "content_version", "catalog_identity");
  identity.content_digest =
      get_string(value, "content_digest", "catalog_identity");
  identity.ruleset_id = get_string(value, "ruleset_id", "catalog_identity");
  identity.schema_sha256 =
      get_string(value, "schema_sha256", "catalog_identity");
  identity.artifact_sha256 =
      get_string(value, "artifact_sha256", "catalog_identity");
  if (const auto *digest = value.find("pack_set_digest"))
    identity.pack_set_digest =
        string(*digest, "catalog_identity/pack_set_digest");
  std::set<StableId> names;
  for (const auto &version :
       array(field(value, "module_versions", "catalog_identity"),
             "module_versions")) {
    fields(version, {"name", "version"}, {}, "module_versions");
    auto name = get_string(version, "name", "module_versions");
    if (!names.insert(name).second)
      fail("DuplicateModuleVersion", name);
    identity.module_versions.emplace_back(
        name, get_string(version, "version", "module_versions"));
  }
  return identity;
}
CheckpointJobResult<std::shared_ptr<const WorldRestoreRetention>>
ResidentReplaySession::retain_storage_world(const CapacityProfile &,
                                            const SlotCapacityProfile &,
                                            CheckpointJobBudget &job) {
  return {false,
          {},
          job.capture_failure(common_failure("UnsupportedStorageRetention", {},
                                             ErrorKind::unsupported_capability)
                                  .error)};
}
CheckpointJobResult<std::shared_ptr<const PreparedCatalog>>
ResidentReplaySession::prepare_storage_catalog(const BoundaryValue &,
                                               const StableId &,
                                               const CatalogIdentity &,
                                               CheckpointJobBudget &job) const {
  return {false,
          {},
          job.capture_failure(common_failure("UnsupportedStorageCatalog", {},
                                             ErrorKind::unsupported_capability)
                                  .error)};
}
CheckpointJobResult<WorldIdentity>
ResidentReplaySession::install_storage_candidate(
    WorldRestoreCandidate &&, const std::optional<WorldIdentity> &,
    WorldInstallCancellation &, CheckpointJobBudget &job) {
  return {false,
          {},
          job.capture_failure(
              common_failure("UnsupportedStorageCheckpointInstallation", {},
                             ErrorKind::unsupported_capability)
                  .error)};
}
CheckpointJobResult<CommittedProfileDeliveries>
ResidentReplaySession::capture_profile_deliveries(
    const CapacityProfile &, CheckpointJobBudget &job) const {
  return {
      false,
      {},
      job.capture_failure(common_failure("UnsupportedProfileDeliveryProjection",
                                         {}, ErrorKind::unsupported_capability)
                              .error)};
}
CheckpointJobResult<std::shared_ptr<const WorldCheckpointCapture>>
ResidentReplaySession::capture_storage_checkpoint(const CapacityProfile &,
                                                  const SlotCapacityProfile &,
                                                  CheckpointJobBudget &job) {
  return {false,
          {},
          job.capture_failure(common_failure("UnsupportedStorageCapture", {},
                                             ErrorKind::unsupported_capability)
                                  .error)};
}
class NativeBoundarySession final : public ResidentReplaySession {
public:
  explicit NativeBoundarySession(std::shared_ptr<const FieldBattleResourceRead> resources = {})
      : field_resources_(std::move(resources)) {}
  CheckpointJobResult<std::shared_ptr<const WorldRestoreRetention>>
  retain_storage_world(const CapacityProfile &capacity,
                       const SlotCapacityProfile &slot,
                       CheckpointJobBudget &job) override {
    try {
      if (!active_storage_restore_.expired())
        return {false,
                {},
                job.capture_failure(common_failure("StorageRestoreBusy", {},
                                                   ErrorKind::internal)
                                        .error)};
      if (world_) {
        auto retained = world_->retain_storage_world(capacity, slot, job);
        if (retained.ok)
          active_storage_restore_ = retained.value;
        return retained;
      }
      auto valid = job.check_profile(capacity);
      if (!valid.ok)
        return {false, {}, std::move(valid.error)};
      valid = job.check_profile(slot);
      if (!valid.ok)
        return {false, {}, std::move(valid.error)};
      auto resource =
          checkpoint_memory_resource(job, CheckpointJobUse::candidate);
      if (!resource.ok)
        return {false, {}, std::move(resource.error)};
      ScopedCheckpointMemory scope(resource.value);
      auto token = job.reserve(CheckpointJobUse::candidate, 0);
      if (!token.ok)
        return {false, {}, std::move(token.error)};
      auto lease = checkpoint_make_shared<WorldRestoreRetention>(
          WorldRestoreRetention::ConstructionKey{}, std::move(token.value),
          std::shared_ptr<const WorldBundle>{}, std::optional<WorldIdentity>{});
      active_storage_restore_ = lease;
      std::shared_ptr<const WorldRestoreRetention> result = std::move(lease);
      return {true, std::move(result), {}};
    } catch (checkpoint_job_detail::FailureException &error) {
      return {false, {}, std::move(error.error)};
    } catch (const std::bad_alloc &) {
      return {false,
              {},
              job.capture_fixed_failure(
                  CheckpointJobFailureCode::allocation_failed)};
    }
  }
  CheckpointJobResult<std::shared_ptr<const WorldCheckpointCapture>>
  capture_storage_checkpoint(const CapacityProfile &capacity,
                             const SlotCapacityProfile &slot,
                             CheckpointJobBudget &job) override {
    if (!world_)
      return {false, {}, job.capture_failure(common_failure("NoWorld").error)};
    return world_->capture_storage_checkpoint(capacity, slot, job);
  }
  CheckpointJobResult<std::shared_ptr<const PreparedCatalog>>
  prepare_storage_catalog(const BoundaryValue &source, const StableId &scenario,
                          const CatalogIdentity &expected,
                          CheckpointJobBudget &job) const override {
    try {
      auto prepared =
          prepare_native_catalog_for_install(source, scenario, expected, job, field_resources_.get());
      if (!prepared.ok)
        return {false, {}, job.capture_failure(std::move(prepared.error))};
      return {true, std::move(prepared.value), {}};
    } catch (checkpoint_job_detail::FailureException &error) {
      return {false, {}, std::move(error.error)};
    } catch (const boundary::DecodeFailure &error) {
      return {false, {}, job.capture_failure(RuleError(error.error))};
    } catch (const std::bad_alloc &) {
      return {false,
              {},
              job.capture_fixed_failure(
                  CheckpointJobFailureCode::allocation_failed)};
    }
  }
  CheckpointJobResult<CommittedProfileDeliveries>
  capture_profile_deliveries(const CapacityProfile &capacity,
                             CheckpointJobBudget &job) const override {
    if (!world_)
      return {false, {}, job.capture_failure(common_failure("NoWorld").error)};
    return world_->capture_profile_deliveries(capacity, job);
  }
  CheckpointJobResult<WorldIdentity>
  install_storage_candidate(WorldRestoreCandidate &&candidate,
                            const std::optional<WorldIdentity> &expected,
                            WorldInstallCancellation &cancellation,
                            CheckpointJobBudget &job) override {
    try {
      auto retained = active_storage_restore_.lock();
      if (!retained || !retained->belongs_to(job))
        return {false,
                {},
                job.capture_fixed_failure(
                    CheckpointJobFailureCode::reservation_mismatch)};
      if (!candidate.validated() || !candidate.memory_.belongs_to(job))
        return {false,
                {},
                job.capture_fixed_failure(
                    CheckpointJobFailureCode::reservation_mismatch)};
      // 目录取实际已验候选，不能从槽外换目录。新ResidentWorld和Impl在原job实际new收费。
      auto prepared = candidate.world_->catalog;
      auto resource =
          checkpoint_memory_resource(job, CheckpointJobUse::candidate);
      if (!resource.ok)
        return {false, {}, std::move(resource.error)};
      ScopedCheckpointMemory scope(resource.value);
      std::unique_ptr<ResidentWorld> initial;
      if (!world_)
        initial = std::make_unique<ResidentWorld>();
      auto *destination = world_ ? world_.get() : initial.get();
      if (initial)
        initial->impl_->active_storage_restore = retained;
      auto installed = destination->install_checkpoint_candidate(
          std::move(candidate), expected, cancellation, job);
      if (!installed.ok)
        return installed;
      // 世界已经唯一发布；这里仅移动原目录/容器拥有者，无可抛分配或第二次restore。
      if (initial)
        world_ = std::move(initial);
      catalog_ = std::move(prepared);
      return installed;
    } catch (checkpoint_job_detail::FailureException &error) {
      return {false, {}, std::move(error.error)};
    } catch (const std::bad_alloc &) {
      return {false,
              {},
              job.capture_fixed_failure(
                  CheckpointJobFailureCode::allocation_failed)};
    }
  }
  // 只读当前唯一world；三类旧取得provider与本工程全部模块/状态都须实际关闭。
  Outcome<ProfileDeliveryAbsenceProof>
  inspect_profile_delivery_absence() const override {
    if (!world_)
      return failure<ProfileDeliveryAbsenceProof>("NoWorld");
    const auto &w = *world_->impl_->world;
    if (!w.catalog->exact_time ||
        w.catalog->exact_time->profile_id != exact_rest_profile_id ||
        !w.catalog->growth || w.catalog->growth->cards.enabled ||
        w.catalog->growth->insights.enabled ||
        w.catalog->growth->actor_start.cards.enabled || w.growth.cards)
      return {false,
              {},
              common_failure("UnsupportedProfileDeliveryProjection", {},
                             ErrorKind::unsupported_capability)
                  .error};
    auto valid = validate_exact_rest_world_scope(w);
    if (!valid.ok)
      return {false, {}, valid.error};
    return {true,
            {w.common.state().identity, w.catalog->identity,
             w.catalog->exact_time->profile_id},
            {}};
  }
  Outcome<BoundaryValue>
  inspect_checkpoint_selection(const Bytes &bytes) override {
    if (world_)
      return failure<BoundaryValue>("AlreadyInstalled");
    return inspect_world_checkpoint_selection(bytes);
  }
  Status initialize(const BoundaryValue &catalog,
                    const BoundaryValue &state) override {
    if (!active_storage_restore_.expired())
      return common_failure("StorageRestoreBusy");
    if (world_)
      return common_failure("AlreadyInstalled");
    // Trusted engineering limits, never limits supplied by the JSON archive.
    const auto &capacity = engineering_checkpoint_capacity_profile();
    auto created = CheckpointJobBudget::create(
        capacity, engineering_slot_capacity_profile());
    if (!created.ok)
      throw checkpoint_job_detail::FailureException{std::move(created.error)};
    auto &job = created.value;
    try {
      CodecDecodeBudget catalog_budget(capacity, job);
      CodecDecodeBudget state_budget(capacity, job);
      auto resource =
          checkpoint_memory_resource(job, CheckpointJobUse::candidate);
      if (!resource.ok)
        throw checkpoint_job_detail::FailureException{
            std::move(resource.error)};
      ScopedCheckpointMemory memory(resource.value);
      Outcome<std::shared_ptr<const PreparedCatalog>> prepared;
      Outcome<JsonDecodedWorldSeedOwner> seed;
      {
        ScopedCheckpointBoundaryDecode boundary(catalog_budget, job,
                                                resource.value);
        prepared = prepare_catalog(catalog, state);
        if (!prepared.ok)
          return {false, {}, std::move(prepared.error)};
        prepared.value = seal_json_decoded_catalog(std::move(prepared.value),
                                                   catalog_budget, job);
      }
      {
        ScopedCheckpointBoundaryDecode boundary(state_budget, job,
                                                resource.value);
        seed = decode_world_seed_owned(state, prepared.value, capacity,
                                       state_budget, job);
        if (!seed.ok)
          return {false, {}, std::move(seed.error)};
      }
      // Each actual catalog alias retains its catalog token independently
      // of the seed. The seed keeps its separate state token through the
      // original install, validation and roundtrip; no raw seed escapes.
      auto candidate = std::make_unique<ResidentWorld>();
      auto installed =
          candidate->install_json_seed(prepared.value, std::move(seed.value));
      if (!installed.ok)
        return installed;
      // 首次冷装载独立完整回导，未知/遗漏字段、数值类型和顺序不能静默进入受支持profile。
      auto actual = export_world_state(*candidate->impl_->world);
      if (!actual.ok)
        return {false, {}, std::move(actual.error)};
      auto compared = compare_boundary(state, actual.value);
      if (!compared.exact()) {
        auto difference = compared.value_difference ? compared.value_difference
                                                    : compared.order_difference;
        return common_failure("ResidentSeedRoundtripMismatch",
                              difference ? difference->path : "",
                              ErrorKind::unsupported_capability);
      }
      catalog_ = std::move(prepared.value);
      world_ = std::move(candidate);
      return common_success();
    } catch (const std::bad_alloc &) {
      throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(
          CheckpointJobFailureCode::allocation_failed)};
    } catch (const std::length_error &) {
      throw checkpoint_job_detail::FailureException{
          job.capture_fixed_failure(CheckpointJobFailureCode::size_overflow)};
    }
  }
  // 当前目录/情景必须在纯原生工厂中匹配，失败不安装临时seed或分配可见身份。
  Status initialize_native(const BoundaryValue &catalog,
                           const BoundaryValue &raw) override {
    try {
      if (!active_storage_restore_.expired())
        return common_failure("StorageRestoreBusy");
      if (world_)
        return common_failure("AlreadyInstalled");
      using namespace boundary;
      fields(raw, {"scenario_id", "world_seed", "expected_catalog_identity"},
             {"founder_selection", "appearance_request", "unlocked_actor_ids",
              "character_creation_request"},
             "request");
      NativeInitRequest request;
      request.scenario_id = get_string(raw, "scenario_id", "request");
      request.world_seed = get_string(raw, "world_seed", "request");
      request.expected_catalog_identity = read_catalog_identity(
          field(raw, "expected_catalog_identity", "request"));
      if (const auto *creation = raw.find("character_creation_request"))
        request.character_creation_request = *creation;
      if (const auto *founder = raw.find("founder_selection"))
        request.founder_selection =
            string(*founder, "request/founder_selection");
      if (const auto *appearance = raw.find("appearance_request"))
        request.appearance_request = *appearance;
      if (const auto *unlocked = raw.find("unlocked_actor_ids")) {
        if (array(*unlocked, "request/unlocked_actor_ids").size() > 4096)
          fail("InvalidUnlockedActorIds", "request/unlocked_actor_ids");
        request.unlocked_actor_ids =
            strings(*unlocked, "request/unlocked_actor_ids");
      }
      auto initial = make_native_initial_world(catalog, request, field_resources_.get());
      if (!initial.ok)
        return {false, {}, initial.error};
      auto candidate = std::make_unique<ResidentWorld>();
      auto installed = candidate->install(initial.value.catalog,
                                          std::move(initial.value.seed), true);
      if (!installed.ok)
        return installed;
      catalog_ = std::move(initial.value.catalog);
      world_ = std::move(candidate);
      return common_success();
    } catch (const boundary::DecodeFailure &error) {
      return {false, {}, error.error};
    }
  }
  Status initialize_checkpoint(const BoundaryValue &catalog,
                               const Bytes &bytes) override {
    return initialize_checkpoint_with_continuation(catalog, bytes, {});
  }
  // 配对载荷全验后才发布世界，损坏许可不能留下部分安装。
  Status
  initialize_checkpoint_with_continuation(const BoundaryValue &catalog,
                                          const Bytes &bytes,
                                          const Bytes &continuation) override {
    try {
      if (!active_storage_restore_.expired())
        return common_failure("StorageRestoreBusy");
      if (world_)
        return common_failure("AlreadyInstalled");
      checkpoint_phase_timing::Scope selection_time(checkpoint_phase_timing::Stage::restore_selection);
      auto selected = inspect_world_checkpoint_selection(bytes);
      if (!selected.ok)
        return {false, {}, selected.error};
      auto expected = read_catalog_identity(
          boundary::field(selected.value, "catalog", "selection"));
      if (expected.ruleset_id != dynamic_ruleset_id)
        return common_failure("UnsupportedCheckpointInstallation", {},
                              ErrorKind::unsupported_capability);
      const auto scenario =
          boundary::get_string(selected.value, "scenario_id", "selection");
      selection_time.stop();
      checkpoint_phase_timing::Scope catalog_time(checkpoint_phase_timing::Stage::restore_catalog);
      auto prepared =
          prepare_native_catalog_for_install(catalog, scenario, expected, field_resources_.get());
      catalog_time.stop();
      if (!prepared.ok)
        return {false, {}, std::move(prepared.error)};
      std::optional<ContinuationSnapshot> stopped;
      if (!continuation.empty()) {
        auto decoded = unbind_continuation_checkpoint(
            continuation, bytes, engineering_slot_capacity_profile());
        if (!decoded.ok)
          return {false, {}, decoded.error};
        stopped = std::move(decoded.value);
      }
      auto candidate = std::make_unique<ResidentWorld>();
      auto restored = candidate->restore(bytes, prepared.value, stopped);
      if (!restored.ok)
        return restored;
      catalog_ = std::move(prepared.value);
      world_ = std::move(candidate);
      return common_success();
    } catch (const boundary::DecodeFailure &error) {
      return {false, {}, error.error};
    }
  }
  Outcome<BoundaryValue> installed_selection() const override {
    if (!world_)
      return failure<BoundaryValue>("NoWorld");
    const auto &metadata = world_->impl_->world->common.state().metadata;
    return {true,
            boundary::obj(
                {{"scenario_id", BoundaryValue{metadata.scenario_id}},
                 {"player_actor_id", BoundaryValue{metadata.player_actor_id}}}),
            {}};
  }
  Outcome<ReplayCommandResult> execute(const BoundaryValue &raw) override {
    auto capture = world_ ? world_->active_storage_capture() : nullptr;
    try {
      WorldCheckpointWriteScope write(capture);
      auto result = execute_native(raw);
      result.value.checkpoint_capture = std::move(capture);
      return result;
    } catch (checkpoint_job_detail::FailureException &error) {
      error.error.try_materialize_rule_error();
      ReplayCommandResult result;
      result.checkpoint_failure = std::move(error.error);
      result.checkpoint_capture = std::move(capture);
      return {false, std::move(result), {}};
    } catch (const std::bad_alloc &) {
      ReplayCommandResult result;
      result.checkpoint_failure =
          capture ? capture->failure_lease() : CheckpointJobFailureLease{};
      if (!result.checkpoint_failure)
        result.checkpoint_failure = CheckpointJobFailure::fixed(
            CheckpointJobFailureCode::allocation_failed);
      result.checkpoint_capture = std::move(capture);
      return {false, std::move(result), {}};
    }
  }
  Outcome<ReplayCommandResult> execute_native(const BoundaryValue &raw) {
    if (!active_storage_restore_.expired())
      return failure<ReplayCommandResult>("StorageRestoreBusy");
    if (!world_)
      return failure<ReplayCommandResult>("NoWorld");
    auto &bundle = *world_->impl_->world;
    const Integer before_revision = bundle.common.revision();
    ReplayCommandResult result;
    const auto start = Clock::now();
    const auto *kind_value = raw.find("kind");
    const auto *params = raw.find("params");
    const auto *seq = raw.find("request_seq");
    const auto *revision = raw.find("expected_revision");
    const auto *actor_value = raw.find("actor_id");
    const auto *kind = kind_value ? kind_value->as<StableId>() : nullptr;
    const auto *number = seq ? seq->as<Integer>() : nullptr;
    const auto *expected = revision ? revision->as<Integer>() : nullptr;
    const bool shape = raw.as<BoundaryValue::Dict>() && kind && params &&
                       params->as<BoundaryValue::Dict>() && number &&
                       expected && *number >= 1 && *number <= 2147483647;
    StableId actor = bundle.common.state().metadata.player_actor_id;
    bool actor_shape = true;
    if (actor_value) {
      if (auto *text = actor_value->as<StableId>())
        actor = *text;
      else
        actor_shape = false;
    }
    // 旧规则目录可没有Director；新登记制仍只代表本人，报价与回执不得绕过权限。
    const bool player_authorized =
        !bundle.catalog->director ||
        !bundle.catalog->director->dynamic_subjects ||
        actor == bundle.common.state().metadata.player_actor_id;
    Outcome<CommandPayload> decoded;
    Outcome<std::shared_ptr<const PreviewResult>> preview;
    CommandEnvelope preview_command;
    auto guard = actor_shape && kind
                     ? command_common_guard(bundle, actor, *kind)
                     : common_failure("UnknownActor");
    if (!kind || !params || !params->as<BoundaryValue::Dict>())
      preview = failure<std::shared_ptr<const PreviewResult>>("InvalidCommand");
    else if (actor_shape && bundle.common.actor(actor) && !player_authorized)
      preview = failure<std::shared_ptr<const PreviewResult>>(
          "PlayerCommandAuthorityRequired");
    else if (!guard.ok)
      preview = {false, {}, guard.error};
    else {
      decoded = decode_resident_command(*kind, *params);
      if (!decoded.ok)
        preview = {false, {}, decoded.error};
    }
    result.timing.decode_usec = elapsed(start);
    const auto preview_start = Clock::now();
    if (decoded.ok) {
      // 报价借同一真实世界身份；request_seq 仍只在原提交分支填写和消费。
      const auto &source = bundle.common.state().identity;
      preview_command.world_id = source.world_id;
      preview_command.execution_epoch = source.execution_epoch;
      preview_command.expected_revision = before_revision;
      preview_command.actor_id = actor;
      preview_command.payload =
          std::make_shared<const CommandPayload>(std::move(decoded.value));
      preview = world_->preview(preview_command);
    }
    result.timing.preview_usec = elapsed(preview_start);
    const auto submit_start = Clock::now();
    Outcome<std::shared_ptr<const CommitResult>> receipt;
    // 原submit首错：形状→人物→幂等→revision→preview；预览失败不能阻止缓存重放。
    if (!shape)
      receipt = failure<std::shared_ptr<const CommitResult>>("InvalidCommand");
    else if (!actor_shape || !bundle.common.actor(actor))
      receipt = failure<std::shared_ptr<const CommitResult>>("UnknownActor");
    else if (!player_authorized)
      receipt = failure<std::shared_ptr<const CommitResult>>(
          "PlayerCommandAuthorityRequired");
    else {
      const auto *sequence =
          bundle.common.state().request_sequences.find(actor);
      if (!sequence)
        receipt = failure<std::shared_ptr<const CommitResult>>(
            "InvalidRequestSequence");
      else if (*number <= *sequence) {
        auto *cached = bundle.common.state().results.find(
            actor + ":" + std::to_string(*number));
        receipt = cached ? Outcome<std::shared_ptr<const CommitResult>>{true,
                                                                        *cached,
                                                                        {}}
                         : failure<std::shared_ptr<const CommitResult>>(
                               "AlreadyApplied");
      } else if (*expected != before_revision)
        receipt = failure<std::shared_ptr<const CommitResult>>("StalePreview");
      else if (!preview.ok)
        receipt = {false, {}, preview.error};
      else {
        // 仍先走原缓存/revision/preview 顺序；同一 typed payload
        // 不重解码或重分配。
        preview_command.request_seq = *number;
        preview_command.expected_revision = *expected;
        receipt = world_->submit(preview_command);
      }
    }
    result.timing.submit_usec = elapsed(submit_start);
    result.timing.stages = world_->impl_->last_submission_timing;
    const auto dto_start = Clock::now();
    auto p = encode_resident_preview(preview, before_revision);
    if (!p.ok)
      return {false, {}, p.error};
    auto r = encode_resident_receipt(receipt);
    if (!r.ok)
      return {false, {}, r.error};
    result.preview = std::move(p.value);
    result.receipt = std::move(r.value);
    result.timing.dto_usec = elapsed(dto_start);
    return {true, std::move(result), {}};
  }
  Status arm_late_failure_probe() override {
#ifdef HANMO_RESIDENT_TEST_FAULTS
    if (!world_)
      return common_failure("NoWorld");
    world_->impl_->fail_after_full_advance_once = true;
    return common_success();
#else
    return ResidentReplaySession::arm_late_failure_probe();
#endif
  }
  // P2始终从调用者身份进入同一core；预览只读，不借CLI execute悄悄提交。
  Outcome<LiveOperationResult>
  execute_live(const LiveOperation &operation) override {
    auto capture = world_ ? world_->active_storage_capture() : nullptr;
    try {
      WorldCheckpointWriteScope write(capture);
      auto result = execute_live_native(operation);
      result.value.checkpoint_capture = std::move(capture);
      return result;
    } catch (checkpoint_job_detail::FailureException &error) {
      error.error.try_materialize_rule_error();
      LiveOperationResult result;
      result.source_identity = operation.source;
      if (world_)
        result.current_identity = world_->identity().value;
      result.checkpoint_failure = std::move(error.error);
      result.checkpoint_capture = std::move(capture);
      return {false, std::move(result), {}};
    } catch (const std::bad_alloc &) {
      LiveOperationResult result;
      result.source_identity = operation.source;
      if (world_)
        result.current_identity = world_->identity().value;
      result.checkpoint_failure =
          capture ? capture->failure_lease() : CheckpointJobFailureLease{};
      if (!result.checkpoint_failure)
        result.checkpoint_failure = CheckpointJobFailure::fixed(
            CheckpointJobFailureCode::allocation_failed);
      result.checkpoint_capture = std::move(capture);
      return {false, std::move(result), {}};
    }
  }
  Outcome<LiveOperationResult>
  execute_live_native(const LiveOperation &operation) {
    if (!active_storage_restore_.expired() &&
        (operation.kind == LiveOperationKind::command ||
         operation.kind == LiveOperationKind::begin_run ||
         operation.kind == LiveOperationKind::grant ||
         operation.kind == LiveOperationKind::restore))
      return failure<LiveOperationResult>("StorageRestoreBusy");
    if (!world_)
      return failure<LiveOperationResult>("NoWorld");
    const auto initial = world_->identity().value;
    if (initial.world_id != operation.source.world_id ||
        initial.execution_epoch != operation.source.execution_epoch)
      return failure<LiveOperationResult>("StaleWorld");
    auto complete = [&](V result) -> Outcome<LiveOperationResult> {
      return {true,
              {operation.source, world_->identity().value, std::move(result)},
              {}};
    };
    try {
      using namespace boundary;
      const auto &payload = operation.payload;
      if (operation.checkpoint_bytes &&
          operation.kind != LiveOperationKind::restore)
        return failure<LiveOperationResult>("InvalidLiveCheckpointFields");
      if (operation.previous_checkpoint_bytes &&
          operation.kind != LiveOperationKind::checkpoint)
        return failure<LiveOperationResult>("InvalidLiveCheckpointFields");
      if (operation.continuation_bytes &&
          (operation.kind != LiveOperationKind::restore ||
           !operation.checkpoint_bytes))
        return failure<LiveOperationResult>("InvalidLiveCheckpointFields");
      if (operation.previous_continuation_bytes &&
          (operation.kind != LiveOperationKind::checkpoint ||
           !operation.previous_checkpoint_bytes))
        return failure<LiveOperationResult>("InvalidLiveCheckpointFields");
      if ((operation.continuation_bytes &&
           operation.continuation_bytes->size() >
               selected_continuation_limit()) ||
          (operation.previous_continuation_bytes &&
           operation.previous_continuation_bytes->size() >
               selected_continuation_limit()))
        return failure<LiveOperationResult>("SaveSizeLimit");
      if ((operation.checkpoint_bytes &&
           operation.checkpoint_bytes->size() > live_checkpoint_byte_limit) ||
          (operation.previous_checkpoint_bytes &&
           operation.previous_checkpoint_bytes->size() >
               live_checkpoint_byte_limit))
        return failure<LiveOperationResult>("SaveSizeLimit");
      // 外部任职答复只代表当前玩家；NPC的本人选择只从内部Director准备/提交路径进入。
      auto membership_actor_allowed = [&](const StableId &kind,
                                          const StableId &actor) {
        const auto &current = *world_->impl_->world;
        return !(named_membership_command_kind(kind) ||
                 (current.named_membership_enabled() &&
                  kind == "organization_member_response")) ||
               actor == current.common.state().metadata.player_actor_id;
      };
      // 无Director的旧目录保持原行为；新注册制外部仍只代表本人，内部走原事务。
      auto player_actor_allowed = [&](const StableId &actor) {
        const auto &current = *world_->impl_->world;
        return !current.catalog->director ||
               !current.catalog->director->dynamic_subjects ||
               actor == current.common.state().metadata.player_actor_id;
      };
      if (operation.kind == LiveOperationKind::command) {
        const auto *kind_value = payload.find("kind");
        const auto *actor_value = payload.find("actor_id");
        if (kind_value && kind_value->as<StableId>()) {
          const auto actor = actor_value && actor_value->as<StableId>()
                                 ? *actor_value->as<StableId>()
                                 : world_->impl_->world->common.state()
                                       .metadata.player_actor_id;
          if (!player_actor_allowed(actor))
            return complete(live_error(
                common_failure("PlayerCommandAuthorityRequired").error));
          if (!membership_actor_allowed(*kind_value->as<StableId>(), actor))
            return complete(live_error(
                common_failure("MembershipPlayerAuthorityRequired").error));
        }
        // execute保原actor→缓存→revision顺序；此时world/epoch已由来源核定。
        auto result = execute(payload);
        if (!result.ok) {
          LiveOperationResult failed;
          failed.source_identity = operation.source;
          failed.current_identity = world_->identity().value;
          failed.checkpoint_failure =
              std::move(result.value.checkpoint_failure);
          failed.checkpoint_capture =
              std::move(result.value.checkpoint_capture);
          return {false, std::move(failed), std::move(result.error)};
        }
        return complete(std::move(result.value.receipt));
      }
      if (operation.kind == LiveOperationKind::preview ||
          operation.kind == LiveOperationKind::query) {
        if (initial.revision != operation.source.revision ||
            initial.tick != operation.source.tick)
          return failure<LiveOperationResult>("StalePreview");
      }
      auto &bundle = *world_->impl_->world;
      if (operation.kind == LiveOperationKind::preview) {
        fields(payload, {"actor_id", "kind", "params"}, {}, "preview");
        const auto actor = get_string(payload, "actor_id", "preview"),
                   kind = get_string(payload, "kind", "preview");
        const auto &params = field(payload, "params", "preview");
        object(params, "preview/params");
        Outcome<std::shared_ptr<const PreviewResult>> preview;
        auto guard = command_common_guard(bundle, actor, kind);
        if (!player_actor_allowed(actor))
          preview = {false,
                     {},
                     common_failure("PlayerCommandAuthorityRequired").error};
        else if (!membership_actor_allowed(kind, actor))
          preview = {false,
                     {},
                     common_failure("MembershipPlayerAuthorityRequired").error};
        else if (!guard.ok)
          preview = {false, {}, guard.error};
        else {
          auto command = decode_resident_command(kind, params);
          if (!command.ok) {
            preview = {false, {}, command.error};
          } else {
            // 上方已核原权限、守卫和解码；公共报价仍只读这一个活动世界。
            CommandEnvelope envelope;
            envelope.world_id = initial.world_id;
            envelope.execution_epoch = initial.execution_epoch;
            envelope.expected_revision = initial.revision;
            envelope.actor_id = actor;
            envelope.payload = std::make_shared<const CommandPayload>(
                std::move(command.value));
            preview = world_->preview(envelope);
          }
        }
        auto encoded = encode_resident_preview(preview, initial.revision);
        if (!encoded.ok)
          return {false, {}, encoded.error};
        return complete(std::move(encoded.value));
      }
      if (operation.kind == LiveOperationKind::query) {
        fields(payload, {"query_id", "args"}, {}, "query");
        const auto query = get_string(payload, "query_id", "query");
        const auto &args = field(payload, "args", "query");
        const auto &player = bundle.common.state().metadata.player_actor_id;
        if (query == "player_frame_seed") {
          fields(args, {}, {}, "args");
          // 组合只投影叶 DTO；失败的原 checkpoint 租约仍由外层结果拥有。
          std::optional<LiveOperationResult> leaf_failure;
          PlayerFrameLeaf leaf = [&](LiveOperationKind kind,
                                     const V &child_payload) -> Outcome<V> {
            if (kind != LiveOperationKind::query &&
                kind != LiveOperationKind::preview)
              return {false,
                      {},
                      common_failure("InvalidPlayerFrameLeaf", {},
                                     ErrorKind::internal)
                          .error};
            LiveOperation child;
            child.kind = kind;
            child.source = operation.source;
            child.payload = child_payload;
            auto result = execute_live(child);
            if (!result.ok) {
              leaf_failure = std::move(result.value);
              return {false, {}, std::move(result.error)};
            }
            return {true, std::move(result.value.result), {}};
          };
          auto result = query_player_frame_seed(
              leaf, bundle.catalog->interaction->npc_society);
          if (!result.ok) {
            if (leaf_failure)
              return {false, std::move(*leaf_failure), std::move(result.error)};
            return {false, {}, std::move(result.error)};
          }
          return complete(std::move(result.value));
        }
        if (query == "command_context") {
          fields(args, {}, {"actor_id"}, "args");
          auto actor = player;
          if (auto *value = args.find("actor_id")) {
            actor = string(*value, "args/actor_id");
            if (actor.empty())
              actor = player;
          }
          if (!player_actor_allowed(actor))
            return complete(live_error(
                common_failure("PlayerCommandAuthorityRequired").error));
          if (!bundle.common.actor(actor))
            return complete(live_error(common_failure("UnknownActor").error));
          auto *sequence = bundle.common.state().request_sequences.find(actor);
          if (!sequence)
            return complete(
                live_error(common_failure("InvalidCommandContext").error));
          return complete(obj({{"ok", V{true}},
                               {"actor_id", V{actor}},
                               {"revision", V{initial.revision}},
                               {"next_request_seq", V{*sequence + 1}}}));
        }
        if (query == "action_context") {
          fields(args, {}, {}, "args");
          auto *actor = bundle.common.actor(player);
          if (!actor)
            return complete(
                live_error(common_failure("InvalidActionContext").error));
          StableId kind;
          bool paused = false;
          if (!actor->action_id.empty()) {
            auto *action = bundle.common.action(actor->action_id);
            if (!action)
              return complete(
                  live_error(common_failure("InvalidActionContext").error));
            kind = action->kind;
            if (kind == "travel") {
              auto travel = travel_encounter(bundle.travel, action->id);
              if (!travel.ok)
                return complete(
                    live_error(common_failure("InvalidActionContext").error));
              paused = travel.value.second;
            }
          }
          return complete(obj({{"ok", V{true}},
                               {"actor_id", V{player}},
                               {"revision", V{initial.revision}},
                               {"action_id", V{actor->action_id}},
                               {"action_kind", V{kind}},
                               {"travel_paused", V{paused}}}));
        }
        if (query == "player_plan") {
          fields(args, {}, {}, "args");
          auto value = query_player_plan_page(bundle, player);
          if (!value.ok)
            return {false, {}, value.error};
          return complete(std::move(value.value));
        }
        if (query == "continuation_state") {
          fields(args, {}, {}, "args");
          auto result = live_continuation(world_->continuation_snapshot());
          if (const auto *ok = result.find("ok");
              ok && ok->as<bool>() && *ok->as<bool>()) {
            result.as<V::Dict>()->emplace_back("source_identity",
                                               live_identity(initial));
            result.as<V::Dict>()->emplace_back("pending_decisions",
                                               live_pending_decisions(bundle));
          }
          return complete(std::move(result));
        }
        if (query == "run_drive_gate_v2") {
          fields(args, {"run_id"}, {}, "args");
          return complete(live_run_drive_gate(world_->read_run_drive_gate(
              {initial.world_id, initial.execution_epoch,
               get_string(args, "run_id", "args")})));
        }
        if (query == "run_progress") {
          fields(args, {"run_id"}, {}, "args");
          return complete(live_progress(
              world_->poll_run({initial.world_id, initial.execution_epoch,
                                get_string(args, "run_id", "args")})));
        }
        // 下面只读取权威player；caller不能传observer或army来扩大可见范围。
        CommonPresentation presentation(bundle.common.state(),
                                        *bundle.catalog->common);
        if (query == "journey_input_need") {
          fields(args, {}, {}, "args");
          auto value = query_journey_input_need(bundle.common, bundle.personal,
                                                bundle.military,
                                                bundle.interaction, player);
          if (!value.ok)
            return {false, {}, value.error};
          if (bundle.naval_combat_enabled() &&
              bundle.services.in_open_battle(player))
            value.value = true;
          return complete(
              obj({{"ok", V{true}}, {"needs_input", V{value.value}}}));
        }
        if (query == "journey_encounter") {
          fields(args, {}, {}, "args");
          auto value = query_journey_encounter(
              bundle.common, bundle.interaction, *bundle.catalog->interaction,
              presentation, player);
          if (!value.ok)
            return {false, {}, value.error};
          auto out =
              obj({{"travel", obj({{"active", V{value.value.travel_active}}})},
                   {"encounter", obj({})}});
          if (value.value.encounter) {
            const auto &peer = *value.value.encounter;
            *out.find("encounter") =
                obj({{"encounter_id", V{peer.encounter_id}},
                     {"provider_actor_id", V{peer.provider_actor_id}},
                     {"display_name", V{peer.display_name}}});
          }
          if (value.value.dialogue_speaker) {
            const auto &speaker = *value.value.dialogue_speaker;
            out.as<V::Dict>()->emplace_back(
                "dialogue_speaker",
                obj({{"actor_id", V{speaker.actor_id}},
                     {"display_name", V{speaker.display_name}}}));
          }
          return complete(std::move(out));
        }
        if (query == "journey_logistics") {
          fields(args, {}, {}, "args");
          auto value =
              query_own_army_logistics(bundle.common, bundle.military,
                                       *bundle.catalog->military, player);
          if (!value.ok)
            return {false, {}, value.error};
          V quote = obj({});
          if (value.value) {
            const auto &q = *value.value;
            quote = obj({{"ok", V{true}},
                         {"available", V{q.available}},
                         {"required", V{q.required}},
                         {"mouth_count", V{q.mouth_count}},
                         {"full_ticks", V{q.full_ticks}},
                         {"shortage", V{q.shortage}}});
          }
          return complete(
              obj({{"ok", V{true}}, {"logistics_quote", std::move(quote)}}));
        }
        if (query == "interaction_view") {
          fields(args, {}, {"cursor"}, "args");
          const auto cursor =
              args.find("cursor") ? get_int(args, "cursor", "args") : 0;
          auto access = bundle.services.interaction_access();
          auto value = query_interaction_view(
              bundle.common, bundle.interaction, *bundle.catalog->interaction,
              access, presentation, player, cursor);
          if (!value.ok)
            return {false, {}, value.error};
          const auto &q = value.value;
          auto out = obj({{"phase", V{q.phase}}});
          auto &fields_out = *out.as<V::Dict>();
          if (q.context)
            fields_out.emplace_back(
                "context", interaction_codec::encode_context(*q.context));
          if (q.node_id)
            fields_out.emplace_back("node_id", V{*q.node_id});
          if (q.last_outcome)
            fields_out.emplace_back(
                "last_outcome",
                interaction_codec::encode_outcome(*q.last_outcome));
          if (q.speaker)
            fields_out.emplace_back("speaker", V{*q.speaker});
          if (q.text)
            fields_out.emplace_back("text", V{*q.text});
          if (q.has_options) {
            V::Array options;
            for (const auto &option : q.options)
              options.push_back(
                  obj({{"id", V{option.id}}, {"text", V{option.text}}}));
            fields_out.emplace_back("options", V{std::move(options)});
            auto page =
                obj({{"cursor", V{q.cursor}}, {"total_rows", V{q.total_rows}}});
            if (q.next_cursor)
              page.as<V::Dict>()->emplace_back("next_cursor",
                                               V{*q.next_cursor});
            fields_out.emplace_back("page", std::move(page));
          }
          if (q.command)
            fields_out.emplace_back(
                "command", interaction_codec::encode_command(*q.command));
          if (q.pending_action_id)
            fields_out.emplace_back("pending_action_id",
                                    V{*q.pending_action_id});
          return complete(std::move(out));
        }
        auto value = query_live_player_view(bundle, query, args);
        if (!value.ok)
          return {false, {}, value.error};
        return complete(std::move(value.value));
      }
      if (operation.kind == LiveOperationKind::begin_run) {
        fields(payload,
               {"run_id", "actor_id", "action_id", "max_half_days",
                "max_gross_outflow"},
               {"resume_saved_scope", "plan_id", "run_drive_version", "target"},
               "begin_run");
        BeginRun request{
            operation.source,
            get_string(payload, "run_id", "begin_run"),
            get_string(payload, "actor_id", "begin_run"),
            get_string(payload, "action_id", "begin_run"),
            get_int(payload, "max_half_days", "begin_run"),
            get_int(payload, "max_gross_outflow", "begin_run"),
            payload.find("resume_saved_scope")
                ? get_bool(payload, "resume_saved_scope", "begin_run")
                : false,
            payload.find("plan_id")
                ? get_string(payload, "plan_id", "begin_run")
                : StableId{}};
        if (payload.find("run_drive_version") || payload.find("target")) {
          if (!payload.find("run_drive_version") || !payload.find("target") ||
              get_int(payload, "run_drive_version", "begin_run") != 2)
            return failure<LiveOperationResult>("RunDriveVersionMismatch");
          request.target =
              read_run_drive_target(field(payload, "target", "begin_run"));
        }
        return complete(live_progress(world_->begin_run(request)));
      }
      if (operation.kind == LiveOperationKind::grant) {
        fields(payload, {"run_id", "permit_seq", "grant_half_days"}, {},
               "grant");
        const auto count = get_int(payload, "grant_half_days", "grant");
        if (count != 1)
          return failure<LiveOperationResult>("InvalidRunPermit");
        return complete(live_progress(world_->resume_run(
            {operation.source, get_string(payload, "run_id", "grant"),
             get_int(payload, "permit_seq", "grant"), count})));
      }
      if (operation.kind == LiveOperationKind::cancel_run) {
        fields(payload, {"run_id"}, {}, "cancel_run");
        return complete(live_progress(
            world_->cancel_run({initial.world_id, initial.execution_epoch,
                                get_string(payload, "run_id", "cancel_run")})));
      }
      if (operation.kind == LiveOperationKind::checkpoint ||
          operation.kind == LiveOperationKind::restore) {
        if (initial.revision != operation.source.revision ||
            initial.tick != operation.source.tick)
          return failure<LiveOperationResult>("StalePreview");
        if (operation.kind == LiveOperationKind::restore) {
          fields(payload, {}, {}, "restore");
          if (!operation.checkpoint_bytes)
            return failure<LiveOperationResult>("MissingCheckpointBytes");
          std::optional<ContinuationSnapshot> stopped;
          if (operation.continuation_bytes) {
            auto decoded = unbind_continuation_checkpoint(
                *operation.continuation_bytes, *operation.checkpoint_bytes,
                engineering_slot_capacity_profile());
            if (!decoded.ok)
              return {false, {}, decoded.error};
            stopped = std::move(decoded.value);
          }
          auto restored =
              world_->restore(*operation.checkpoint_bytes, catalog_, stopped);
          if (!restored.ok)
            return {false, {}, restored.error};
          return complete(
              obj({{"ok", V{true}},
                   {"identity", live_identity(world_->identity().value)}}));
        }
        fields(payload, {}, {"previous_text_sha256"}, "checkpoint");
        auto previous_text =
            payload.find("previous_text_sha256")
                ? get_string(payload, "previous_text_sha256", "checkpoint")
                : StableId{};
        if (operation.previous_checkpoint_bytes) {
          if (previous_text.size() != 64 ||
              previous_text.find_first_not_of("0123456789abcdef") !=
                  StableId::npos)
            return failure<LiveOperationResult>(
                "InvalidPreviousCheckpointHash");
          auto accepted =
              validate_cold_checkpoint(*operation.previous_checkpoint_bytes,
                                       operation.previous_continuation_bytes);
          if (!accepted.ok)
            return {false, {}, accepted.error};
        } else if (!previous_text.empty())
          return failure<LiveOperationResult>("InvalidPreviousCheckpointHash");
        auto bytes = world_->checkpoint();
        if (!bytes.ok)
          return {false, {}, bytes.error};
        if (bytes.value.size() > live_checkpoint_byte_limit)
          return failure<LiveOperationResult>("SaveSizeLimit");
        std::optional<Bytes> continuation;
        auto stopped = world_->continuation_snapshot();
        if (!stopped.ok)
          return {false, {}, stopped.error};
        if (stopped.value) {
          auto encoded = bind_continuation_checkpoint(
              *stopped.value, bytes.value, engineering_slot_capacity_profile());
          if (!encoded.ok)
            return {false, {}, encoded.error};
          continuation = std::move(encoded.value);
        }
        auto accepted = validate_cold_checkpoint(bytes.value, continuation);
        if (!accepted.ok)
          return {false, {}, accepted.error};
        const auto candidate_hash = picosha2::hash256_hex_string(
            bytes.value.begin(), bytes.value.end());
        StableId previous_hash;
        if (operation.previous_checkpoint_bytes)
          previous_hash = picosha2::hash256_hex_string(
              operation.previous_checkpoint_bytes->begin(),
              operation.previous_checkpoint_bytes->end());
        auto output = complete(
            obj({{"ok", V{true}},
                 {"validation",
                  obj({{"ok", V{true}},
                       {"candidate_sha256", V{candidate_hash}},
                       {"previous_sha256", V{previous_hash}},
                       {"previous_text_sha256", V{previous_text}}})}}));
        auto *validation =
            output.value.result.find("validation")->as<V::Dict>();
        if (continuation)
          validation->emplace_back(
              "candidate_continuation_sha256",
              V{picosha2::hash256_hex_string(continuation->begin(),
                                             continuation->end())});
        if (operation.previous_continuation_bytes)
          validation->emplace_back(
              "previous_continuation_sha256",
              V{picosha2::hash256_hex_string(
                  operation.previous_continuation_bytes->begin(),
                  operation.previous_continuation_bytes->end())});
        output.value.continuation_bytes = std::move(continuation);
        output.value.checkpoint_bytes = std::move(bytes.value);
        return output;
      }
      return {false,
              {},
              common_failure("UnsupportedLiveOperation", {},
                             ErrorKind::unsupported_capability)
                  .error};
    } catch (const boundary::DecodeFailure &error) {
      return {false, {}, error.error};
    }
  }
  // 存储读回沿同一冷核验，不重建常驻世界、不安装候选、不分配epoch或推进ID。
  Outcome<StorageCheckpointValidation> validate_storage_checkpoint(
      const WorldIdentity &source, const Bytes &candidate,
      const std::optional<Bytes> &continuation,
      const std::optional<Bytes> &previous,
      const std::optional<Bytes> &previous_continuation) const override {
    if (!world_)
      return failure<StorageCheckpointValidation>("NoWorld");
    const auto current = world_->identity().value;
    if (current.world_id != source.world_id ||
        current.execution_epoch != source.execution_epoch)
      return failure<StorageCheckpointValidation>("StaleWorld");
    // 同epoch只能核已捕获的过去或当前快照，不能接受尚未发生的来源。
    if (source.revision > current.revision || source.tick > current.tick)
      return failure<StorageCheckpointValidation>("StalePreview");
    if (previous_continuation && !previous)
      return failure<StorageCheckpointValidation>(
          "InvalidLiveCheckpointFields");
    if (candidate.size() > live_checkpoint_byte_limit ||
        (previous && previous->size() > live_checkpoint_byte_limit) ||
        (continuation &&
         continuation->size() > selected_continuation_limit()) ||
        (previous_continuation &&
         previous_continuation->size() > selected_continuation_limit()))
      return failure<StorageCheckpointValidation>("SaveSizeLimit");
    if (previous) {
      auto valid = validate_cold_checkpoint(*previous, previous_continuation);
      if (!valid.ok)
        return {false, {}, valid.error};
    }
    auto valid = validate_cold_checkpoint(candidate, continuation, &source);
    if (!valid.ok)
      return {false, {}, valid.error};
    StorageCheckpointValidation result;
    result.source_identity = source;
    result.current_identity = current;
    result.candidate_sha256 =
        picosha2::hash256_hex_string(candidate.begin(), candidate.end());
    if (continuation)
      result.candidate_continuation_sha256 = picosha2::hash256_hex_string(
          continuation->begin(), continuation->end());
    if (previous)
      result.previous_sha256 =
          picosha2::hash256_hex_string(previous->begin(), previous->end());
    if (previous_continuation)
      result.previous_continuation_sha256 = picosha2::hash256_hex_string(
          previous_continuation->begin(), previous_continuation->end());
    return {true, std::move(result), {}};
  }
  Outcome<BoundaryValue> export_state() const override {
    if (!world_)
      return failure<BoundaryValue>("NoWorld");
    return export_world_state(*world_->impl_->world);
  }
  Outcome<WorldIdentity> identity() const override {
    if (!world_)
      return failure<WorldIdentity>("NoWorld");
    return world_->identity();
  }
  Outcome<Bytes> checkpoint() override {
    if (!world_)
      return failure<Bytes>("NoWorld");
    return world_->checkpoint();
  }
  Status restore(const Bytes &data) override {
    if (!world_)
      return common_failure("NoWorld");
    return world_->restore(data, catalog_);
  }

private:
  // 受信工程上限只由已验完整profile选择；旧profile仍严格保留原2048首错。
  std::uint64_t selected_continuation_limit() const {
    return catalog_ && catalog_->exact_time
               ? engineering_slot_capacity_profile().max_continuation_bytes
               : continuation_snapshot_byte_limit;
  }
  // 沿restore原语义准入与导出边界，但临时候选绝不安装、不分配epoch或清除run。
  Status validate_cold_checkpoint(const Bytes &bytes,
                                  const std::optional<Bytes> &continuation = {},
                                  const WorldIdentity *source = nullptr) const {
    auto seed = decode_world_checkpoint(bytes, *catalog_);
    if (!seed.ok)
      return {false, {}, seed.error};
    // epoch从不入checkpoint；原捕获的world/revision/tick仍必须与实际解码值一致。
    if (source) {
      const auto &identity = seed.value.common.identity;
      if (identity.world_id != source->world_id ||
          identity.revision != source->revision ||
          identity.tick != source->tick)
        return common_failure("CheckpointIdentityMismatch");
    }
    WorldBundle candidate(catalog_, std::move(seed.value));
    auto valid = candidate.validate();
    if (!valid.ok)
      return valid;
    if (continuation) {
      auto decoded = unbind_continuation_checkpoint(
          *continuation, bytes, engineering_slot_capacity_profile());
      if (!decoded.ok)
        return {false, {}, decoded.error};
      auto bound = validate_continuation_snapshot(candidate, decoded.value);
      if (!bound.ok)
        return bound;
    }
    auto exportable = export_world_state(candidate);
    if (!exportable.ok)
      return {false, {}, exportable.error};
    return common_success();
  }
  std::shared_ptr<const FieldBattleResourceRead> field_resources_;
  std::weak_ptr<const WorldRestoreRetention> active_storage_restore_;
  std::unique_ptr<ResidentWorld> world_;
  std::shared_ptr<const PreparedCatalog> catalog_;
};
std::unique_ptr<ResidentReplaySession> create_resident_replay_session() {
  return std::make_unique<NativeBoundarySession>();
}
std::unique_ptr<ResidentReplaySession> create_resident_replay_session(
    std::shared_ptr<const FieldBattleResourceRead> resources) {
  return std::make_unique<NativeBoundarySession>(std::move(resources));
}
} // namespace hanmo::resident
