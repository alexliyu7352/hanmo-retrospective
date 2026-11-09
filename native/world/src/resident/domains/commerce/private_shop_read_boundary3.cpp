// Shop3 只借实际 S0 范围证明；旧1/2宏步保持原输入，不由 tick 拼时间或空 phase。
#include "private_shop.hpp"
#include "private_shop_operation_sources.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
namespace hanmo::resident {
namespace {
Status bound(const PrivateShopContext &x) {
  if (x.catalog.version != 3 || x.state.version != 3 || !x.exact_operations ||
      &x.exact_operations->shops != &x || !x.read_boundary3 ||
      !x.read_boundary3->bound_to(x.common, x.exact_operations->clock))
    return common_failure("PrivateShopOriginalReadBoundaryRequired");
  return common_success();
}
} // namespace
Outcome<WorldMoment>
read_private_shop_observed_moment3(const PrivateShopContext &x) {
  auto valid = bound(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  return x.read_boundary3->read_observed_moment();
}
Status validate_private_shop_committed_read3(const PrivateShopContext &x) {
  if (x.catalog.version != 3 && x.state.version != 3)
    return common_success();
  auto valid = bound(x);
  if (!valid.ok)
    return valid;
  if (x.phase)
    return common_failure("PrivateShopCommandRequiresCommittedClock");
  auto moment = x.read_boundary3->read_committed_moment();
  if (!moment.ok)
    return {false, {}, moment.error};
  return common_success();
}
} // namespace hanmo::resident
