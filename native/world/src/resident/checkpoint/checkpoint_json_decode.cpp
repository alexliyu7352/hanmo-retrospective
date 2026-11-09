#include "checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "checkpoint_copy_shape.hpp"
#include "ordered_value.hpp"
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
namespace {
thread_local ScopedCheckpointJsonDecode *active_json_decode = nullptr;
thread_local ScopedCheckpointBoundaryDecode *active_boundary_decode = nullptr;
} // namespace
ScopedCheckpointJsonDecode::ScopedCheckpointJsonDecode(
    CodecDecodeBudget &budget, CheckpointJobBudget &job,
    std::string_view domain, std::shared_ptr<CheckpointMemoryResource> resource)
    : budget_(budget), job_(job), domain_(domain),
      resource_(std::move(resource)), previous_(active_json_decode) {
  activate();
}
void ScopedCheckpointJsonDecode::activate() {
  if (!budget_.belongs_to(job_))
    throw checkpoint_job_detail::FailureException{job_.capture_fixed_failure(
        CheckpointJobFailureCode::reservation_mismatch)};
  auto checked = job_.check_profile(budget_.profile);
  if (!checked.ok)
    throw checkpoint_job_detail::FailureException{std::move(checked.error)};
  if (domain_.empty() || domain_.size() > 256)
    fail("InvalidCheckpointDomain");
  bool registered = false;
  for (const auto &item : budget_.profile.max_section_bytes_by_registered_name)
    if (item.first == domain_) {
      registered = true;
      break;
    }
  if (!catalog_mode_ && !registered)
    fail("UnregisteredCheckpointDomain");
  allocator(resource_);
  const auto current = current_checkpoint_memory();
  if (!current || !current->shares_job(*resource_))
    fail("CheckpointCandidateMemoryJobMismatch");
  if (previous_ &&
      (&previous_->budget_ != &budget_ || &previous_->job_ != &job_ ||
       previous_->catalog_mode_ != catalog_mode_))
    fail("CheckpointJsonNestedJobMismatch");
  if (active_boundary_decode && !active_boundary_decode->uses(budget_, job_))
    fail("CheckpointJsonNestedJobMismatch");
  if (previous_)
    depth_ = previous_->depth_;
  active_json_decode = this;
}
namespace {
ScopedCheckpointBoundaryDecode &boundary_parent() {
  if (auto *parent = ScopedCheckpointBoundaryDecode::current())
    return *parent;
  throw checkpoint_job_detail::FailureException{CheckpointJobFailure::fixed(
      CheckpointJobFailureCode::json_context_required)};
}
std::string_view world_section_domain(CheckpointWorldJsonSection section,
                                      ScopedCheckpointBoundaryDecode &parent) {
  switch (section) {
  case CheckpointWorldJsonSection::face_to_face_contacts:
    return "face_to_face_contacts";
  case CheckpointWorldJsonSection::private_shop_household_custody:
    return "private_shop_household_custody_sources";
  case CheckpointWorldJsonSection::growth_market_history:
    return "growth_market_history";
  case CheckpointWorldJsonSection::public_supply_inspection:
    return "public_supply_inspection";
  case CheckpointWorldJsonSection::field_battle:
    return "field_battle";
  case CheckpointWorldJsonSection::item_instances:
    return "item_instances";
  case CheckpointWorldJsonSection::lot_properties:
    return "lot_properties";
  case CheckpointWorldJsonSection::craft_jobs:
    return "craft_jobs";
  case CheckpointWorldJsonSection::dynamic_military:
    return "dynamic_military";
  case CheckpointWorldJsonSection::military_action_time:
    return "military_action_time";
  case CheckpointWorldJsonSection::control_continuation:
    return "control_continuation";
  case CheckpointWorldJsonSection::private_shop_operation_sources:
    return "private_shop_operation_sources";
  case CheckpointWorldJsonSection::private_shop_operation_capabilities:
    return "private_shop_operation_capabilities";
  case CheckpointWorldJsonSection::interaction:
    return "interaction";
  case CheckpointWorldJsonSection::social:
    return "social";
  case CheckpointWorldJsonSection::dynamic_social:
    return "dynamic_social";
  case CheckpointWorldJsonSection::social_period_sources:
    return "social_period_sources";
  case CheckpointWorldJsonSection::military_authority_time:
    return "military_authority_time";
  case CheckpointWorldJsonSection::organization_runtime:
    return "organization_runtime";
  case CheckpointWorldJsonSection::organization:
    return "organization";
  case CheckpointWorldJsonSection::economic_obligation_sources:
    return "economic_obligation_sources";
  case CheckpointWorldJsonSection::governance_programs:
    return "governance_programs";
  case CheckpointWorldJsonSection::family_birth:
    return "family_birth";
  case CheckpointWorldJsonSection::personal_battle_lifecycle:
    return "personal_battle_lifecycle";
  case CheckpointWorldJsonSection::military_battle_lifecycle:
    return "military_battle_lifecycle";
  case CheckpointWorldJsonSection::military_action_resources:
    return "military_action_resources";
  case CheckpointWorldJsonSection::local_economy:
    return "local_economy";
  case CheckpointWorldJsonSection::private_transport_funding:
    return "private_transport_funding";
  case CheckpointWorldJsonSection::private_shops:
    return "private_shops";
  case CheckpointWorldJsonSection::estate_employment:
    return "estate_employment";
  case CheckpointWorldJsonSection::estate:
    return "estate";
  case CheckpointWorldJsonSection::economic_facility_capacity_sources:
    return "economic_facility_capacity_sources";
  case CheckpointWorldJsonSection::production_facility_conditions:
    return "production_facility_conditions";
  case CheckpointWorldJsonSection::governance_work_expense:
    return "governance_work_expense";
  case CheckpointWorldJsonSection::military_action_conditions:
    return "military_action_conditions";
  case CheckpointWorldJsonSection::military_training_completion_time:
    return "military_training_completion_time";
  case CheckpointWorldJsonSection::military_formation_reservations:
    return "military_formation_reservations";
  case CheckpointWorldJsonSection::organization_service_effects:
    return "organization_service_effects";
  case CheckpointWorldJsonSection::military_return_detachments:
    return "military_return_detachments";
  case CheckpointWorldJsonSection::task_execution_time:
    return "task_execution_time";
  case CheckpointWorldJsonSection::production_cycles:
    return "production_cycles";
  case CheckpointWorldJsonSection::civilian_transport_population:
    return "civilian_transport_population";
  case CheckpointWorldJsonSection::household_purchases:
    return "household_purchases";
  case CheckpointWorldJsonSection::household_retail_settlements:
    return "household_retail_settlements";
  case CheckpointWorldJsonSection::economic_transport_expenses:
    return "economic_transport_expenses";
  }
  parent.failure("UnregisteredCheckpointWorldSection", {}, ErrorKind::internal);
}
// 职责：把既有目录种类映射到其唯一预算域，未知种类保原拒绝。
// 物理定义独立使用六个已登记集合，不借 timings 的目录额度。
std::string_view catalog_domain(CheckpointCatalogJsonKind kind,
                                ScopedCheckpointBoundaryDecode &parent) {
  switch (kind) {
  case CheckpointCatalogJsonKind::public_supply_inspection:
    return "@catalog/public_supply_inspection";
  case CheckpointCatalogJsonKind::growth_permanent_definitions:
    return "@catalog/growth_permanent";
  case CheckpointCatalogJsonKind::dynamic_military_action_timings:
    return "@catalog/dynamic_military_action";
  case CheckpointCatalogJsonKind::military_spatial_definitions:
    return "@catalog/military_spatial_definition";
  case CheckpointCatalogJsonKind::military_session_definitions:
    return "@catalog/military_session_definition";
  case CheckpointCatalogJsonKind::item_services:
    return "@catalog/item_services";
  case CheckpointCatalogJsonKind::repair_policies:
    return "@catalog/repair_policies";
  case CheckpointCatalogJsonKind::repair_service_rules:
    return "@catalog/repair_service_rules";
  case CheckpointCatalogJsonKind::craft_recipes:
    return "@catalog/craft_recipes";
  case CheckpointCatalogJsonKind::organization_lifecycle:
    return "@catalog/organization_lifecycle";
  case CheckpointCatalogJsonKind::organization_authority:
    return "@catalog/organization_authority";
  case CheckpointCatalogJsonKind::local_economy:
    return "@catalog/local_economy";
  case CheckpointCatalogJsonKind::private_shops:
    return "@catalog/private_shops";
  case CheckpointCatalogJsonKind::estate_employment:
    return "@catalog/estate_employment";
  case CheckpointCatalogJsonKind::estate:
    return "@catalog/estate";
  case CheckpointCatalogJsonKind::economic_facilities:
    return "@catalog/economic_facilities";
  case CheckpointCatalogJsonKind::production_facility_conditions:
    return "@catalog/production_facility_conditions";
  case CheckpointCatalogJsonKind::governance_work_fees:
    return "@catalog/governance_work_fees";
  case CheckpointCatalogJsonKind::governance_program_catalog:
    return "@catalog/governance_program_catalog";
  case CheckpointCatalogJsonKind::military_duty_catalog:
    return "@catalog/military_duty_catalog";
  case CheckpointCatalogJsonKind::dependent_provision_catalog:
    return "@catalog/dependent_provision";
  case CheckpointCatalogJsonKind::facility_claims:
    return "@catalog/facility_claims";
  case CheckpointCatalogJsonKind::political_treaty_terms:
    return "@catalog/political_treaty_terms";
  case CheckpointCatalogJsonKind::treaty_authority_mapping:
    return "@catalog/treaty_authority_mapping";
  case CheckpointCatalogJsonKind::private_shop_household_custody:
    return "@catalog/private_shop_household_custody";
  case CheckpointCatalogJsonKind::household_purchases:
    return "@catalog/household_purchases";
  case CheckpointCatalogJsonKind::household_retail_settlements:
    return "@catalog/household_retail_settlements";
  case CheckpointCatalogJsonKind::production_cycles:
    return "@catalog/production_cycles";
  case CheckpointCatalogJsonKind::politics:
    return "@catalog/politics";
  case CheckpointCatalogJsonKind::fiscal_settlement:
    return "@catalog/fiscal_settlement";
  case CheckpointCatalogJsonKind::political_relations:
    return "@catalog/political_relations";
  }
  parent.failure("UnregisteredCheckpointCatalog", {}, ErrorKind::internal);
}
} // namespace
ScopedCheckpointJsonDecode::ScopedCheckpointJsonDecode(
    CheckpointCatalogJsonKind kind)
    : ScopedCheckpointJsonDecode(boundary_parent(), kind) {}
ScopedCheckpointJsonDecode::ScopedCheckpointJsonDecode(
    ScopedCheckpointBoundaryDecode &parent, CheckpointCatalogJsonKind kind)
    : catalog_mode_(true), budget_(parent.budget_), job_(parent.job_),
      domain_(catalog_domain(kind, parent)), resource_(parent.resource_),
      previous_(active_json_decode) {
  activate();
}
ScopedCheckpointJsonDecode::ScopedCheckpointJsonDecode(
    CheckpointWorldJsonSection section)
    : ScopedCheckpointJsonDecode(boundary_parent(), section) {}
ScopedCheckpointJsonDecode::ScopedCheckpointJsonDecode(
    ScopedCheckpointBoundaryDecode &parent, CheckpointWorldJsonSection section)
    : budget_(parent.budget_), job_(parent.job_),
      domain_(world_section_domain(section, parent)),
      resource_(parent.resource_), previous_(active_json_decode) {
  activate();
}
ScopedCheckpointJsonDecode::~ScopedCheckpointJsonDecode() noexcept {
  active_json_decode = previous_;
}
ScopedCheckpointJsonDecode *ScopedCheckpointJsonDecode::current() noexcept {
  return active_json_decode;
}
void ScopedCheckpointJsonDecode::ordinary(std::uint64_t bytes) {
  auto charged = budget_.charge_candidate(bytes);
  if (!charged.ok)
    throw checkpoint_job_detail::FailureException{
        job_.capture_failure(std::move(charged.error))};
}
void ScopedCheckpointJsonDecode::allocator(
    const std::shared_ptr<CheckpointMemoryResource> &resource) {
  auto checked = budget_.account_allocator_candidate(0, resource);
  if (!checked.ok)
    throw checkpoint_job_detail::FailureException{
        job_.capture_failure(std::move(checked.error))};
}
[[noreturn]] void ScopedCheckpointJsonDecode::fail(const char *code,
                                                   std::string_view field) {
  failure(code, {field}, ErrorKind::internal);
}
void ScopedCheckpointJsonDecode::collection(std::string_view name,
                                            std::uint64_t total) {
  if (name.empty() || name.size() > 256)
    fail("UnregisteredCheckpointRecordKind", name);
  // JSON 此处借用的是已解析的真实数组；原 parser 的字节／深度门不变。
  // 名称只来自 paired typed owner，不能靠另一份逐型容量表授予未知字段。
  if (total > checkpoint_coarse_file_limit_bytes)
    throw checkpoint_job_detail::FailureException{job_.capture_capacity_failure(
        {"collection_items"}, total, budget_.profile.profile_id,
        budget_.profile.revision)};
}
void ScopedCheckpointJsonDecode::text(std::string &destination,
                                      const std::string &input) {


  ordinary(std::size_t{0});
  // 使用已锁定STL的fresh range构造，不对现有capacity做可能几何扩容的assign。
  auto fresh = checkpoint_fresh_string_copy(input);
  destination.swap(fresh);
}
void ScopedCheckpointJsonDecode::enter() {
  if (depth_ >= budget_.profile.max_nesting_depth)
    throw checkpoint_job_detail::FailureException{job_.capture_capacity_failure(
        {"nesting_depth"}, depth_ + 1, budget_.profile.profile_id,
        budget_.profile.revision)};
  ++depth_;
}
void ScopedCheckpointJsonDecode::leave() noexcept {
  if (depth_)
    --depth_;
}
[[noreturn]] void ScopedCheckpointJsonDecode::failure(
    std::string_view code, std::initializer_list<std::string_view> path,
    ErrorKind kind) {
  throw checkpoint_job_detail::FailureException{
      job_.capture_boundary_failure(code, path, kind)};
}
ScopedCheckpointBoundaryDecode::ScopedCheckpointBoundaryDecode(
    CodecDecodeBudget &budget, CheckpointJobBudget &job,
    std::shared_ptr<CheckpointMemoryResource> resource)
    : budget_(budget), job_(job), resource_(resource),
      previous_(active_boundary_decode) {
  if (!budget.belongs_to(job))
    throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(
        CheckpointJobFailureCode::reservation_mismatch)};
  auto profile = job.check_profile(budget.profile);
  if (!profile.ok)
    throw checkpoint_job_detail::FailureException{std::move(profile.error)};
  auto checked = budget.account_allocator_candidate(0, resource);
  if (!checked.ok)
    throw checkpoint_job_detail::FailureException{
        job.capture_failure(std::move(checked.error))};
  const auto current = current_checkpoint_memory();
  if (!current || !resource || !current->shares_job(*resource) ||
      (previous_ &&
       (&previous_->budget_ != &budget || &previous_->job_ != &job)))
    throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(
        CheckpointJobFailureCode::reservation_mismatch)};
  // World JSON has its own domain/collection scope; a catalog may not shadow
  // it.
  if (ScopedCheckpointJsonDecode::current())
    throw checkpoint_job_detail::FailureException{job.capture_fixed_failure(
        CheckpointJobFailureCode::reservation_mismatch)};
  active_boundary_decode = this;
}
ScopedCheckpointBoundaryDecode::~ScopedCheckpointBoundaryDecode() noexcept {
  active_boundary_decode = previous_;
}
ScopedCheckpointBoundaryDecode *
ScopedCheckpointBoundaryDecode::current() noexcept {
  return active_boundary_decode;
}
void ScopedCheckpointBoundaryDecode::ordinary(std::uint64_t bytes) {
  auto result = budget_.charge_candidate(bytes);
  if (!result.ok)
    throw checkpoint_job_detail::FailureException{
        job_.capture_failure(std::move(result.error))};
}
[[noreturn]] void ScopedCheckpointBoundaryDecode::failure(
    std::string_view code, std::initializer_list<std::string_view> path,
    ErrorKind kind) {
  throw checkpoint_job_detail::FailureException{
      job_.capture_boundary_failure(code, path, kind)};
}
bool checkpoint_boundary_decode_active() noexcept {
  return active_boundary_decode || active_json_decode;
}
namespace {
void ordinary_boundary(std::uint64_t bytes) {
  if (active_boundary_decode)
    active_boundary_decode->ordinary(bytes);
  else if (active_json_decode)
    active_json_decode->ordinary(bytes);
}
} // namespace
BoundaryValue checkpoint_decode_boundary_copy(const BoundaryValue &source) {
  if (checkpoint_boundary_decode_active()) {


    ordinary_boundary(std::size_t{0});
  }
  return source;
}
std::string checkpoint_decode_boundary_text(std::string_view source) {
  if (checkpoint_boundary_decode_active()) {


    ordinary_boundary(std::size_t{0});
  }
  if (source.empty())
    return {};
  // The active plan pins the same libstdc++ fresh range capacity as string().
  return std::string(source.data(), source.size());
}
std::string checkpoint_decode_boundary_join(
    std::initializer_list<std::string_view> pieces) {
  std::size_t count = 0;
  for (auto piece : pieces) {
    if (piece.size() > SIZE_MAX - count) {
      if (checkpoint_boundary_decode_active())
        checkpoint_decode_boundary_failure("CheckpointCollectionTooLarge", {},
                                           ErrorKind::internal);
      throw std::length_error("boundary path capacity");
    }
    count += piece.size();
  }
  if (checkpoint_boundary_decode_active()) {
    if (count == SIZE_MAX)
      checkpoint_decode_boundary_failure("CheckpointCollectionTooLarge", {},
                                         ErrorKind::internal);


    ordinary_boundary(std::size_t{0});
  }
  std::string result(count, '\0');
  std::size_t offset = 0;
  for (auto piece : pieces)
    for (char ch : piece)
      result[offset++] = ch;
  return result;
}
std::string checkpoint_decode_boundary_string(const std::string &source) {
  return checkpoint_decode_boundary_text(source);
}
void checkpoint_decode_boundary_array(std::size_t count,
                                      std::size_t element_size) {
  if (!checkpoint_boundary_decode_active())
    return;
  if (element_size && count > SIZE_MAX / element_size)
    checkpoint_decode_boundary_failure("CheckpointCollectionTooLarge", {},
                                       ErrorKind::internal);
  ordinary_boundary(count * element_size);
}
[[noreturn]] void
checkpoint_decode_boundary_failure(std::string_view code,
                                   std::initializer_list<std::string_view> path,
                                   ErrorKind kind) {
  if (active_boundary_decode)
    active_boundary_decode->failure(code, path, kind);
  if (active_json_decode)
    active_json_decode->failure(code, path, kind);
  throw checkpoint_job_detail::FailureException{CheckpointJobFailure::fixed(
      CheckpointJobFailureCode::json_context_required)};
}
CheckpointJsonVisitDepth::CheckpointJsonVisitDepth(bool reading)
    : owner_(reading ? ScopedCheckpointJsonDecode::current() : nullptr) {
  if (reading && !owner_ && current_checkpoint_memory())
    throw checkpoint_job_detail::FailureException{CheckpointJobFailure::fixed(
        CheckpointJobFailureCode::json_context_required)};
  if (owner_)
    owner_->enter();
}
CheckpointJsonVisitDepth::~CheckpointJsonVisitDepth() noexcept {
  if (owner_)
    owner_->leave();
}
} // namespace hanmo::resident
