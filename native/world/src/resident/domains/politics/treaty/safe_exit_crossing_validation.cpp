#include "safe_exit_crossing_validation.hpp"
namespace hanmo::resident {
// 跨界后果核验缺决定中实际采用的权限依据与退出目的；当前入口显式拒绝，不补造持久字段或改写候选。
Status
validate_safe_exit_crossing_consequences(const SafeExitEntryContext &,
                                         const SafeExitDecisionSourceContext &,
                                         const BoundaryCrossingReceipt &) {
  return common_failure("NOT_IMPLEMENTED",
                        "politics/treaty/"
                        "safe_exit_crossing_validation.cpp::validate_safe_exit_"
                        "crossing_consequences",
                        ErrorKind::unsupported_capability);
}
} // namespace hanmo::resident
