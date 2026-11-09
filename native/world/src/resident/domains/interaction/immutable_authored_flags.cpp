#include "immutable_authored_flags.hpp"
#include "interaction_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/world_retention.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
AuthoredInteractionFlags::AuthoredInteractionFlags(
    CaptureTag, CheckpointMemoryCharge memory, const CatalogIdentity &identity,
    const BoundaryValue &flags)
    : memory_(std::move(memory)), identity_(identity) {
  const auto &rows = boundary::object(flags, "data/flags");
  flags_.reserve(rows.size());
  for (const auto &row : rows)
    flags_.push_back({checkpoint_fresh_string_copy(row.first),
                      boundary::boolean(row.second, "data/flags/value")});
}
AuthoredInteractionFlags::~AuthoredInteractionFlags() = default;
Outcome<std::shared_ptr<const AuthoredInteractionFlags>>
AuthoredInteractionFlags::capture(const BoundaryValue &data,
                                  const CatalogIdentity &identity) {
  try {
    const auto &flags = boundary::field(data, "flags", "data");
    const auto &rows = boundary::object(flags, "data/flags");


    for (std::size_t i = 0; i < rows.size(); ++i) {
      if (rows[i].first.empty())
        return {false,
                {},
                common_failure("AuthoredInteractionFlagIdRequired").error};
      for (std::size_t j = 0; j < i; ++j)
        if (rows[i].first == rows[j].first)
          return {
              false,
              {},
              common_failure("DuplicateAuthoredInteractionFlag", rows[i].first)
                  .error};
      (void)boundary::boolean(rows[i].second, "data/flags/value");

    }
    // 同job实际node/control allocation收费；ordinary
    // payload费用随同唯一owner保活。
    auto owner = checkpoint_make_shared<AuthoredInteractionFlags>(
        CaptureTag{}, CheckpointMemoryCharge(0), identity, flags);
    return {true, std::move(owner), {}};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
const CatalogIdentity &AuthoredInteractionFlags::identity() const noexcept {
  return identity_;
}
const std::vector<FlagRecord> &AuthoredInteractionFlags::rows() const noexcept {
  return flags_;
}
// 实际目录值仍深拥有；这里只保原资源的生命周期，不计量字段。
void AuthoredInteractionFlags::retain(CheckpointRetentionCensus &census) const {
  census.resource(memory_.resource());
}
Status
validate_immutable_interaction_flags(const AuthoredInteractionFlags &seed,
                                     const CatalogIdentity &expected,
                                     const InteractionState &state) {
  if (!same_original_catalog_identity(seed.identity(), expected))
    return common_failure("AuthoredInteractionFlagsCatalogLockMismatch");
  const auto &original = seed.rows();
  if (original.size() != state.flags.size())
    return common_failure("ImmutableInteractionFlagsCoverageMismatch");
  for (const auto &row : original) {
    const FlagRecord *found = nullptr;
    for (const auto &actual : state.flags)
      if (actual.id == row.id) {
        if (found)
          return common_failure("DuplicateImmutableInteractionFlag", row.id);
        found = &actual;
      }
    if (!found || found->value != row.value)
      return common_failure("ImmutableInteractionFlagSourceMismatch", row.id);
  }
  return common_success();
}
Outcome<const FlagRecord *> read_immutable_interaction_flag(
    const AuthoredInteractionFlags &seed, const CatalogIdentity &expected,
    const InteractionState &state, const StableId &id) {
  auto valid = validate_immutable_interaction_flags(seed, expected, state);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  for (const auto &row : seed.rows())
    if (row.id == id)
      return {true, &row, {}};
  return {
      false, {}, common_failure("UnknownImmutableInteractionFlag", id).error};
}
} // namespace hanmo::resident
