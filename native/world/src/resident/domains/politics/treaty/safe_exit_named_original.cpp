#include "safe_exit_named_original.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::rule) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}

// 两窄时钟借同候选；不以保存下来的entry时刻替当前处理M。
void contexts(const SafeExitNamedOriginalContext &x) {
  if (&x.entries.common != &x.decisions.common ||
      &x.entries.common != &x.treaties.activations.signatures.revisions.common)
    fail("SafeExitNamedOriginalCandidateMismatch");
  if (take(
          compare_world_moments(take(x.entries.clock.read_current_moment()),
                                take(x.clock.read_organization_authority_now()),
                                x.entries.arithmetic)))
    fail("SafeExitNamedOriginalClockMismatch");
}

} // namespace
SafeExitEntryContext NativeNamedSafeExitOriginalRead::entry_context() const {
  const auto &e = x_.entries;
  return {e.common,     e.carriers,       e.movement,
          e.executions, e.travel_catalog, e.manifest_history,
          x_.treaties,  x_.state,         *this,
          x_.clock,     e.arithmetic};
}
// 具名入境条款分类缺决定中原权限依据；当前入口显式拒绝，不补造持久字段或改写候选。
Outcome<std::optional<SafeExitEntryTerm>>
NativeNamedSafeExitOriginalRead::read_actual_safe_exit_entry_term(
    const BoundaryCrossingReceipt &) const {
  return {false,
          {},
          common_failure(
              "NOT_IMPLEMENTED",
              "politics/treaty/"
              "safe_exit_named_original.cpp::NativeNamedSafeExitOriginalRead::"
              "read_actual_safe_exit_entry_term",
              ErrorKind::unsupported_capability)
              .error};
}

Outcome<SafeExitEntryPartition>
NativeNamedSafeExitOriginalRead::read_original_safe_exit_entry_partition(
    const BoundaryCrossingReceipt &crossing,
    const TransportEntryManifestSource &manifest,
    const TreatyPassageTerm &) const {
  try {
    contexts(x_);
    return read_original_named_safe_exit_partition(x_.entries, crossing,
                                                   manifest);
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
Status
NativeNamedSafeExitOriginalRead::validate_original_safe_exit_entry_partition(
    const BoundaryCrossingReceipt &crossing,
    const TransportEntryManifestSource &manifest, const TreatyPassageTerm &,
    const SafeExitEntryPartition &partition) const {
  try {
    contexts(x_);
    return validate_original_named_safe_exit_partition(x_.entries, crossing,
                                                       manifest, partition);
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
// 保留资格进度核验缺决定中原退出目的；当前入口显式拒绝，不补造持久字段或改写候选。
Status NativeNamedSafeExitOriginalRead::validate_safe_exit_retained_progress(
    const SafeExitBinding &, const OrderedTable<SafeExitLine> &) const {
  return common_failure(
      "NOT_IMPLEMENTED",
      "politics/treaty/"
      "safe_exit_named_original.cpp::NativeNamedSafeExitOriginalRead::validate_"
      "safe_exit_retained_progress",
      ErrorKind::unsupported_capability);
}

// 实际退出采用核验缺决定中原权限依据与退出窗口；当前入口显式拒绝，不补造持久字段或改写候选。
Status NativeNamedSafeExitOriginalRead::validate_actual_safe_exit_exit_adoption(
    const SafeExitBinding &, const SafeExitPassageContext &,
    const BoundaryCrossingReceipt &,
    const TransportEntryManifestSource &) const {
  return common_failure(
      "NOT_IMPLEMENTED",
      "politics/treaty/"
      "safe_exit_named_original.cpp::NativeNamedSafeExitOriginalRead::validate_"
      "actual_safe_exit_exit_adoption",
      ErrorKind::unsupported_capability);
}

} // namespace hanmo::resident
