#include "safe_exit_progress.hpp"
namespace hanmo::resident {
// 原路径进度核验缺决定中目的、权限依据及退出窗口；当前入口显式拒绝，不补造持久字段或改写候选。
Status validate_safe_exit_original_path_progress(
    const SafeExitEntryContext &, const SafeExitDecisionSourceContext &,
    const SafeExitBinding &, const OrderedTable<SafeExitLine> &) {
  return common_failure(
      "NOT_IMPLEMENTED",
      "politics/treaty/"
      "safe_exit_progress.cpp::validate_safe_exit_original_path_progress",
      ErrorKind::unsupported_capability);
}
// 政治跨界采用缺决定中原退出目的与权限依据；当前入口显式拒绝，不补造持久字段或改写候选。
Outcome<std::optional<SourceRef>>
adopt_safe_exit_political_crossing_in_candidate(
    CommonStore &, SafeExitState &, const SafeExitEntryContext &,
    const SafeExitDecisionSourceContext &, const SourceRef &) {
  return {false,
          {},
          common_failure("NOT_IMPLEMENTED",
                         "politics/treaty/"
                         "safe_exit_progress.cpp::adopt_safe_exit_political_"
                         "crossing_in_candidate",
                         ErrorKind::unsupported_capability)
              .error};
}
// 边界完成采用缺决定中原退出目的与权限依据；当前入口显式拒绝，不补造持久字段或改写候选。
Status adopt_safe_exit_boundary_completion_in_candidate(
    CommonStore &, SafeExitState &, const SafeExitEntryContext &,
    const SafeExitDecisionSourceContext &, const SourceRef &) {
  return common_failure("NOT_IMPLEMENTED",
                        "politics/treaty/"
                        "safe_exit_progress.cpp::adopt_safe_exit_boundary_"
                        "completion_in_candidate",
                        ErrorKind::unsupported_capability);
}
} // namespace hanmo::resident
