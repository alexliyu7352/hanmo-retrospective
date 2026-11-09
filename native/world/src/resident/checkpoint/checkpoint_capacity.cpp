// 编译期文件／结构／磁盘上限；任何文件内容均不能改变这些受信边界。
#include "checkpoint_capacity.hpp"
#include "checkpoint_job_memory.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
namespace {
constexpr std::uint64_t signed_limit =
    std::uint64_t(std::numeric_limits<std::int64_t>::max());
// 非法受信配置属于部署技术错误，不降级为无限预算。
Status invalid_profile() {
  return {false,
          {},
          {{"InvalidCapacityProfile", "checkpoint/capacity"},
           ErrorKind::internal,
           {},
           {},
           {},
           {}}};
}
bool finite(std::uint64_t value) { return value && value <= signed_limit; }
} // namespace
namespace {
std::size_t
diagnostic_text_size(std::string_view prefix,
                     std::initializer_list<std::string_view> parts) {
  auto count = prefix.size();
  for (const auto part : parts) {
    if (part.size() > SIZE_MAX - count)
      throw std::length_error("checkpoint diagnostic extent");
    count += part.size();
  }
  return count;
}
std::string diagnostic_text(std::string_view prefix,
                            std::initializer_list<std::string_view> parts) {
  std::string result(diagnostic_text_size(prefix, parts), '\0');
  auto out = result.begin();
  out = std::copy(prefix.begin(), prefix.end(), out);
  for (const auto part : parts)
    out = std::copy(part.begin(), part.end(), out);
  return result;
}
// 仅限制首错诊断的实际字串窗口，不参与候选／复制对象的内存计量。
struct CapacityDiagnosticPlan {
  std::size_t bytes = 48; // phase/domain/command 三个未写入字段的原有限空串窗口。
  void add_text(std::size_t size) {
    const auto capacity = std::max<std::size_t>(15, size);
    if (capacity == SIZE_MAX || capacity + 1 > SIZE_MAX - bytes)
      throw std::length_error("checkpoint diagnostic extent");
    bytes += capacity + 1;
  }
  void text(std::string &, std::string_view prefix,
            std::initializer_list<std::string_view> parts) {
    add_text(diagnostic_text_size(prefix, parts));
  }
  void text(std::optional<StableId> &, std::string_view prefix,
            std::initializer_list<std::string_view> parts) {
    add_text(diagnostic_text_size(prefix, parts));
  }
};
struct CapacityDiagnosticWrite {
  void text(std::string &target, std::string_view prefix,
            std::initializer_list<std::string_view> parts) {
    auto value = diagnostic_text(prefix, parts);
    target.swap(value);
  }
  void text(std::optional<StableId> &target, std::string_view prefix,
            std::initializer_list<std::string_view> parts) {
    target.emplace(diagnostic_text(prefix, parts));
  }
};
template <class Writer>
void capacity_diagnostic_fields(
    RuleError &error, std::initializer_list<std::string_view> resource,
    std::uint64_t required, std::string_view profile, std::uint64_t revision,
    Writer &writer) {
  char revision_digits[20], required_digits[20];
  const auto rev = std::to_chars(
      revision_digits, revision_digits + sizeof(revision_digits), revision);
  const auto need = std::to_chars(
      required_digits, required_digits + sizeof(required_digits), required);
  writer.text(error.original.code, {}, {"CheckpointCapacityExceeded"});
  writer.text(error.original.path, "checkpoint/capacity/", resource);
  error.kind = ErrorKind::internal;
  writer.text(error.source_id, {}, {profile});
  writer.text(
      error.limit, {},
      {std::string_view(revision_digits,
                        static_cast<std::size_t>(rev.ptr - revision_digits))});
  writer.text(
      error.reason, {},
      {std::string_view(required_digits,
                        static_cast<std::size_t>(need.ptr - required_digits))});
}
} // namespace
RuleError
make_checkpoint_boundary_error(std::string_view code,
                               std::initializer_list<std::string_view> path,
                               ErrorKind kind) {
  return {{std::string(code), diagnostic_text({}, path)}, kind, {}, {}, {}, {}};
}
std::size_t checkpoint_capacity_diagnostic_bytes(
    std::initializer_list<std::string_view> resource, std::uint64_t required,
    std::string_view profile, std::uint64_t revision) {
  RuleError fields;
  CapacityDiagnosticPlan plan;
  capacity_diagnostic_fields(fields, resource, required, profile, revision,
                             plan);
  return plan.bytes;
}
RuleError
make_checkpoint_capacity_error(std::initializer_list<std::string_view> resource,
                               std::uint64_t required, std::string_view profile,
                               std::uint64_t revision) {
  RuleError fields;
  CapacityDiagnosticWrite writer;
  capacity_diagnostic_fields(fields, resource, required, profile, revision,
                             writer);
  return fields;
}
Status checkpoint_capacity_failure(const StableId &resource,
                                   std::uint64_t required,
                                   const StableId &profile,
                                   std::uint64_t revision) {
  return {
      false,
      {},
      make_checkpoint_capacity_error({resource}, required, profile, revision)};
}
bool valid_capacity_profile_shape(const CapacityProfile &p) noexcept {
  if (p.profile_id.empty() || !finite(p.revision) ||
      !finite(p.max_checkpoint_bytes) ||
      p.max_checkpoint_bytes > checkpoint_coarse_file_limit_bytes ||
      !finite(p.max_header_bytes) || !finite(p.max_nesting_depth) ||
      !finite(p.max_bigint_encoded_bytes) || !finite(p.stream_chunk_bytes) ||
      !finite(p.max_staging_disk_bytes))
    return false;
  if (p.max_header_bytes > p.max_checkpoint_bytes ||
      p.max_bigint_encoded_bytes > p.max_checkpoint_bytes ||
      p.stream_chunk_bytes > std::numeric_limits<std::size_t>::max() ||
      p.max_nesting_depth > std::numeric_limits<std::size_t>::max() ||
      p.max_section_bytes_by_registered_name.empty())
    return false;
  for (const auto &entry : p.max_section_bytes_by_registered_name)
    if (entry.first.empty() || !finite(entry.second) ||
        entry.second > p.max_checkpoint_bytes)
      return false;
  return true;
}
bool valid_slot_capacity_profile_shape(const SlotCapacityProfile &p) noexcept {
  if (p.profile_id.empty() || !finite(p.revision) ||
      !finite(p.max_slot_bytes) ||
      p.max_slot_bytes > checkpoint_coarse_file_limit_bytes ||
      !finite(p.max_slot_header_bytes) || !finite(p.max_continuation_bytes) ||
      !finite(p.max_admission_capsule_bytes) ||
      !finite(p.max_slot_staging_disk_bytes) ||
      p.max_slot_header_bytes > p.max_slot_bytes ||
      p.max_continuation_bytes > p.max_slot_bytes ||
      p.max_admission_capsule_bytes > p.max_slot_bytes)
    return false;
  return true;
}
Status validate_capacity_profile(const CapacityProfile &p) {
  return valid_capacity_profile_shape(p) ? Status{true, {}, {}}
                                         : invalid_profile();
}
Status validate_slot_capacity_profile(const SlotCapacityProfile &p) {
  return valid_slot_capacity_profile_shape(p) ? Status{true, {}, {}}
                                              : invalid_profile();
}
CodecDecodeBudget::CodecDecodeBudget(const CapacityProfile &p,
                                     CheckpointJobBudget &job,
                                     CheckpointJobUse use)
    : profile(p), job_bound_(true) {
  auto checked = job.check_profile(p);
  if (!checked.ok)
    throw checkpoint_job_detail::FailureException{std::move(checked.error)};
  if (!(std::uint8_t(use) & 4))
    throw checkpoint_job_detail::FailureException{
        job.capture_fixed_failure(CheckpointJobFailureCode::invalid_use)};
  auto held = job.reserve(use, 0);
  if (!held.ok)
    throw checkpoint_job_detail::FailureException{std::move(held.error)};
  job_reservation_ = std::move(held.value);
}
CheckpointJobReservation CodecDecodeBudget::take_job_reservation() noexcept {
  return std::move(job_reservation_);
}
Status CodecDecodeBudget::reject_technical(CheckpointJobFailureCode code,
                                           std::uint64_t required) {
  if (job_bound_)
    throw checkpoint_job_detail::FailureException{
        job_reservation_.capture_fixed_failure(code, required)};
  return {false,
          {},
          {{checkpoint_job_failure_code_name(code),
            "checkpoint/default-construction"},
           ErrorKind::internal,
           {},
           {},
           {},
           {}}};
}
Status
CodecDecodeBudget::reject_boundary(std::string_view code,
                                   std::initializer_list<std::string_view> path,
                                   ErrorKind kind) {
  if (job_bound_)
    throw checkpoint_job_detail::FailureException{
        job_reservation_.capture_boundary_failure(code, path, kind)};
  return {false, {}, make_checkpoint_boundary_error(code, path, kind)};
}
Status CodecDecodeBudget::reject_capacity(
    std::initializer_list<std::string_view> resource, std::uint64_t required) {
  if (job_bound_)
    throw checkpoint_job_detail::FailureException{
        job_reservation_.capture_capacity_failure(
            resource, required, profile.profile_id, profile.revision)};
  return {false,
          {},
          make_checkpoint_capacity_error(resource, required, profile.profile_id,
                                         profile.revision)};
}
Status CodecDecodeBudget::accept_failure(Status &&failure) {
  if (job_bound_ && !failure.ok)
    throw checkpoint_job_detail::FailureException{
        job_reservation_.capture_failure(std::move(failure.error))};
  return std::move(failure);
}
CheckpointJobReservation
CodecDecodeBudget::reserve_scratch(std::uint64_t bytes) {
  if (!job_bound_)
    return {};
  // Checks moved reservation, cancellation and original first error before
  // reserve.
  charge_candidate(0);
  auto held =
      job_reservation_.reserve_related(CheckpointJobUse::scratch, bytes);
  if (!held.ok)
    throw checkpoint_job_detail::FailureException{std::move(held.error)};
  return std::move(held.value);
}
Status CodecDecodeBudget::charge_candidate(std::uint64_t) {
  // 默认形状与候选内存累计已经删除；不放宽原 token 的移交/取消/首错检查。
  if (job_bound_) {
    if (!job_reservation_.valid() || job_reservation_.bytes() != 0 || candidate_bytes != 0)
      throw checkpoint_job_detail::FailureException{CheckpointJobFailure::fixed(
          CheckpointJobFailureCode::reservation_mismatch)};
    auto checked = job_reservation_.grow(0);
    if (!checked.ok)
      throw checkpoint_job_detail::FailureException{std::move(checked.error)};
  }
  return {true, {}, {}};
}
Status CodecDecodeBudget::account_allocator_candidate(
    std::uint64_t bytes,
    const std::shared_ptr<CheckpointMemoryResource> &resource) {
  if (!job_bound_)
    return charge_candidate(bytes);
  // grow(0) 先核移交/取消/首错；后续实际 allocate(n) 仍绑定同一 state。
  auto checked = charge_candidate(0);
  if (!checked.ok)
    return checked;
  if (!checkpoint_resource_accounts_candidate(resource, job_reservation_))
    throw checkpoint_job_detail::FailureException{CheckpointJobFailure::fixed(
        CheckpointJobFailureCode::reservation_mismatch)};
  return {true, {}, {}};
}
// 数值仅供原工程闭环；未知段名仍拒绝，不再登记 record-kind 内存额度。
const CapacityProfile &engineering_checkpoint_capacity_profile() {
  static const CapacityProfile value = []() {
    CapacityProfile p;
    p.profile_id = "hanmo-engineering-checkpoint-v2";
    p.revision = 50;
    p.max_checkpoint_bytes = checkpoint_coarse_file_limit_bytes;
    p.max_header_bytes = 1024u * 1024u;
    for (const auto *name : {"public_supply_inspection",
                             "field_battle",
                             "military_battle_lifecycle",
                             "personal_battle_lifecycle",
                             "military_return_detachments",
                             "organization_service_effects",
                             "military_formation_reservations",
                             "military_training_completion_time",
                             "economic_facility_capacity_sources",
                             "production_facility_conditions",
                             "governance_work_expense",
                             "military_action_conditions",
                             "task_execution_time",
                             "civilian_transport_population",
                             "economic_transport_expenses",
                             "military_action_resources",
                             "economic_obligation_sources",
                             "governance_programs",
                             "organization_runtime",
                             "military_authority_time",
                             "item_instances",
                             "lot_properties",
                             "craft_jobs",
                             "military_action_time",
                             "control_continuation",
                             "private_shop_operation_sources",
                             "private_shop_household_custody_sources",
                             "private_shop_operation_capabilities",
                             "dynamic_military",
                             "common",
                             "profile_deliveries",
                             "travel",
                             "group_transport",
                             "transport_manifest_bindings",
                             "military",
                             "interaction",
                             "social",
                             "estate",
                             "tasks",
                             "family",
                             "personal",
                             "growth",
                             "growth_terminal_publications",
                             "trade-grant",
                             "local_economy",
                             "organization",
                             "affairs",
                             "director",
                             "dynamic_tasks",
                             "dynamic_social",
                             "population_military",
                             "siege_integration",
                             "military_integration",
                             "player_plan",
                             "politics",
                             "political_fiscal",
                             "fiscal_settlement",
                             "politics_governance",
                             "governance_tasks",
                             "public_relief",
                             "task_offer_responses",
                             "task_personal_advances",
                             "task_message_handoffs",
                             "diplomacy",
                             "envoy_tasks",
                             "military_naval",
                             "household_provision_management",
                             "production_disruptions",
                             "political_takeover",
                             "economic_takeover_execution",
                             "task_takeover_execution",
                             "family_life",
                             "family_legacy",
                             "political_occupation_fiscal",
                             "occupation_fiscal_settlement",
                             "family_bequest",
                             "task_message_notices",
                             "treaty_passage",
                             "family_inheritance",
                             "fiscal_taxpayer_continuity",
                             "political_military_appointments",
                             "family_marriage",
                             "task_static_assets",
                             "estate_employment",
                             "political_field_appointments",
                             "economic_supply_policies",
                             "political_supply_adoptions",
                             "family_birth",
                             "visit_messages",
                             "visit_reception_claims",
                             "family_care_relief",
                             "growth_market_history",
                             "political_shop_licensing",
                             "private_shops",
                             "task_shop_orders",
                             "task_extensions",
                             "political_shop_registry_successions",
                             "personal_appointments",
                             "personal_injury_recovery",
                             "political_shop_licensing_receipts",
                             "private_shop_liquidations",
                             "task_shop_order_estate_cancellations",
                             "family_apprenticeship",
                             "political_governance_continuations",
                             "contact_inquiries",
                             "visit_message_replies",
                             "family_disciple_legacy",
                             "political_governance_invitation_replies",
                             "private_shop_daily_rent",
                             "born_personal_dialogues",
                             "family_biological_legacy",
                             "political_successions",
                             "visit_appointment_invitations",
                             "visit_appointment_meetings",
                             "political_head_handovers",
                             "military_tactic_learning",
                             "growth_tactic_lessons",
                             "political_successor_rounds",
                             "private_shop_command_results",
                             "political_term_handovers",
                             "visit_message_receipts",
                             "known_personal_dialogues",
                             "political_office_receptions",
                             "task_shop_order_contacts",
                             "family_collateral_legacy",
                             "political_governance_agencies",
                             "task_message_returns",
                             "family_voluntary_paths",
                             "task_personal_message_deliveries",
                             "political_office_resignations",
                             "task_personal_message_reports",
                             "task_personal_message_cancellations",
                             "political_subordinate_appointments",
                             "task_personal_message_acceptances",
                             "political_dismissal_sources",
                             "world_timeline",
                             "action_elapsed",
                             "actor_start_context",
                             "character_origin_records",
                             "growth_permanent_contributions",
                             "character_condition_extensions",
                             "facility_claims",
                             "household_purchases",
                             "household_retail_settlements",
                             "social_period_sources",
                             "face_to_face_contacts",
                             "actor_social_occupancy",
                             "life_age_checks",
                             "production_cycles",
                             "private_transport_funding",
                             "task_work_execution",
                             "private_shop_household_custody_sources",
                             "political_treaty_terms"})
      p.max_section_bytes_by_registered_name.emplace(name,
                                                     p.max_checkpoint_bytes);
    // 修订36只补原 Care 冻结字段的有限域额度，不选择或启用 Care 世界根。
    p.max_section_bytes_by_registered_name.emplace("care_requirements",
                                                   p.max_checkpoint_bytes);
    // 修订47登记三个真实 S1 独立段；容量不产生作者选择或业务许可。
    for (const auto *name :
         {"movement_boundary_contacts", "movement_group_member_releases",
          "movement_group_member_losses"})
      p.max_section_bytes_by_registered_name.emplace(name,
                                                     p.max_checkpoint_bytes);
    // 真有限段选择仍由原 selector / typed codec 授予；不再平行登记每个集合名。
    for (const auto *name : {"political_passage", "political_relations",
                            "character-injury-sources"})
      p.max_section_bytes_by_registered_name.emplace(name, p.max_checkpoint_bytes);
    // 保留 e72 已落地的真实 boundary continuation 独立段选择。
    p.max_section_bytes_by_registered_name.emplace("movement_boundary_continuations",
                                                   p.max_checkpoint_bytes);
    // local-move/1 仅登记原有限物理段；不恢复已删除逐对象/集合内存账。
    p.max_section_bytes_by_registered_name.emplace("local_move_sources",
                                                   p.max_checkpoint_bytes);
    p.max_nesting_depth = 64;
    p.max_bigint_encoded_bytes = 1024u * 1024u;
    p.stream_chunk_bytes = 64u * 1024u;
    p.max_staging_disk_bytes = checkpoint_coarse_file_limit_bytes;
    return p;
  }();
  return value;
}
const SlotCapacityProfile &engineering_slot_capacity_profile() {
  static const SlotCapacityProfile value{
      "hanmo-engineering-slot-v2",
      2,
      checkpoint_coarse_file_limit_bytes,
      1024u * 1024u,
      1024u * 1024u + 45u,
      8u * 1024u * 1024u,
      2 * checkpoint_coarse_file_limit_bytes};
  return value;
}
} // namespace hanmo::resident
