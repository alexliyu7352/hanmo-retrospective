// 显式JSON冷读同job上下文；不从档案字段授额，不为每个域重建账本。
#pragma once
#include "checkpoint_capacity.hpp"
#include "checkpoint_job_memory.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include <string_view>
#include <type_traits>
namespace hanmo::resident {
class ScopedCheckpointBoundaryDecode;
// Finite original catalog producer; never a caller-provided world domain.
enum class CheckpointCatalogJsonKind : int;
// Explicit original World JSON routes borrow the existing Boundary candidate.
// This enum is not a free-form domain or a new admission/budget authority.
enum class CheckpointWorldJsonSection {
  face_to_face_contacts,
  growth_market_history,
  public_supply_inspection,
  field_battle,
  item_instances,
  lot_properties,
  craft_jobs,
  dynamic_military,
  military_action_time,
  control_continuation,
  private_shop_operation_sources,
  private_shop_operation_capabilities,
  interaction,
  social,
  dynamic_social,
  social_period_sources,
  military_authority_time,
  organization_runtime,
  organization,
  economic_obligation_sources,
  military_action_resources,
  local_economy,
  military_action_conditions,
  task_execution_time,
  production_cycles,
  civilian_transport_population,
  household_purchases,
  household_retail_settlements,
  economic_transport_expenses,
  private_transport_funding,
  private_shops,
  estate_employment,
  estate,
  economic_facility_capacity_sources,
  production_facility_conditions,
  governance_work_expense,
  military_training_completion_time,
  military_formation_reservations,
  organization_service_effects,
  military_return_detachments,
  governance_programs,
  family_birth,
  personal_battle_lifecycle,
  military_battle_lifecycle,
  private_shop_household_custody
};
class ScopedCheckpointJsonDecode {
public:
  ScopedCheckpointJsonDecode(CodecDecodeBudget &, CheckpointJobBudget &,
                             std::string_view domain,
                             std::shared_ptr<CheckpointMemoryResource>);
  explicit ScopedCheckpointJsonDecode(CheckpointCatalogJsonKind);
  explicit ScopedCheckpointJsonDecode(CheckpointWorldJsonSection);
  ~ScopedCheckpointJsonDecode() noexcept;
  ScopedCheckpointJsonDecode(const ScopedCheckpointJsonDecode &) = delete;
  static ScopedCheckpointJsonDecode *current() noexcept;
  void collection(std::string_view name, std::uint64_t total);
  void ordinary(std::uint64_t);
  // 必须在T{}、emplace、resize前调用，同candidate token保留默认普通字段。
  template <class T> void default_values(std::uint64_t count = 1) {
    auto checked = charge_checkpoint_defaults<T>(budget_, count);
    if (!checked.ok)
      throw checkpoint_job_detail::FailureException{
          job_.capture_failure(std::move(checked.error))};
  }

  void allocator(const std::shared_ptr<CheckpointMemoryResource> &);
  void text(std::string &destination, const std::string &input);
  void enter();
  void leave() noexcept;
  [[noreturn]] void failure(std::string_view,
                            std::initializer_list<std::string_view>, ErrorKind);
  [[noreturn]] void fail(const char *code, std::string_view field = {});
  template <class T, class A>
  void reserve_block(const std::vector<T, A> &value, std::size_t count) {
    if (count > value.max_size() || count > SIZE_MAX / sizeof(T))
      fail("CheckpointCollectionTooLarge");
    if constexpr (std::is_same_v<A, CheckpointAllocator<T>>)
      allocator(value.get_allocator().resource());
    else
      ordinary(count * sizeof(T));
  }

private:
  ScopedCheckpointJsonDecode(ScopedCheckpointBoundaryDecode &,
                             CheckpointCatalogJsonKind);
  ScopedCheckpointJsonDecode(ScopedCheckpointBoundaryDecode &,
                             CheckpointWorldJsonSection);
  void activate();
  bool catalog_mode_ = false;
  CodecDecodeBudget &budget_;
  CheckpointJobBudget &job_;
  std::string_view domain_;
  std::shared_ptr<CheckpointMemoryResource> resource_;
  ScopedCheckpointJsonDecode *previous_;
  std::uint64_t depth_ = 0;
};
// Original Boundary readers and finite World section adapters borrow this same
// candidate budget. It meters actual explicitly connected ordinary copies;
// it does not make catalogs World sections or certify complete decode.
class ScopedCheckpointBoundaryDecode {
public:
  ScopedCheckpointBoundaryDecode(CodecDecodeBudget &, CheckpointJobBudget &,
                                 std::shared_ptr<CheckpointMemoryResource>);
  ~ScopedCheckpointBoundaryDecode() noexcept;
  ScopedCheckpointBoundaryDecode(const ScopedCheckpointBoundaryDecode &) =
      delete;
  static ScopedCheckpointBoundaryDecode *current() noexcept;
  // Same original typed default plan for hand-written catalog candidate rows.
  template <class T> void default_values(std::uint64_t count = 1) {
    auto checked = charge_checkpoint_defaults<T>(budget_, count);
    if (!checked.ok)
      throw checkpoint_job_detail::FailureException{
          job_.capture_failure(std::move(checked.error))};
  }

  bool uses(const CodecDecodeBudget &budget,
            const CheckpointJobBudget &job) const noexcept {
    return &budget_ == &budget && &job_ == &job;
  }
  void ordinary(std::uint64_t);
  [[noreturn]] void failure(std::string_view,
                            std::initializer_list<std::string_view>, ErrorKind);

private:
  friend class ScopedCheckpointJsonDecode;
  CodecDecodeBudget &budget_;
  CheckpointJobBudget &job_;
  std::shared_ptr<CheckpointMemoryResource> resource_;
  ScopedCheckpointBoundaryDecode *previous_;
};
bool checkpoint_boundary_decode_active() noexcept;
struct BoundaryValue;
// Charge the actual original fresh copy before calling its unchanged
// constructor. The active original decoded owner retains the reservation
// through adoption.
BoundaryValue checkpoint_decode_boundary_copy(const BoundaryValue &);
std::string checkpoint_decode_boundary_string(const std::string &);
// Borrow the actual text before its first ordinary allocation.
std::string checkpoint_decode_boundary_text(std::string_view);
std::string
    checkpoint_decode_boundary_join(std::initializer_list<std::string_view>);
void checkpoint_decode_boundary_array(std::size_t count,
                                      std::size_t element_size);
[[noreturn]] void checkpoint_decode_boundary_failure(
    std::string_view, std::initializer_list<std::string_view>, ErrorKind);
// 每个原reader子访客只借同一个context；不复制profile/map或新的reservation。
class CheckpointJsonVisitDepth {
public:
  explicit CheckpointJsonVisitDepth(bool reading = true);
  ~CheckpointJsonVisitDepth() noexcept;
  CheckpointJsonVisitDepth(const CheckpointJsonVisitDepth &) = delete;

private:
  ScopedCheckpointJsonDecode *owner_ = nullptr;
};
} // namespace hanmo::resident
