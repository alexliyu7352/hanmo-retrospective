#include "resident/checkpoint/projection_writer.hpp"
#include "resident/indexes/contact_model.hpp"
#include "world_bundle.hpp"

namespace hanmo::resident {

// 所属线程先拒绝未结束的事务/阶段，再准备 Common 和 services 的原有读缓存。
Status WorldBundle::prepare_checkpoint_read_cache() {
  if (has_active_read_phase() || active_exact_write_context())
    return common_failure("CheckpointRequiresStableBoundary",
                          "world/checkpoint/read_cache", ErrorKind::internal);
  // 适配器随同一 WorldBundle 真正拥有，不再因缺内存计量图拒绝。
  // 市场服务仍须先通过原绑定、live scope 与完整来源门；restore 路径也不能跳过。
  if (const auto* market = services.market_history()) {
    auto stable = market->validate_for_publish();
    if (!stable.ok)
      return stable;
  }
  auto prepared = common.prepare_checkpoint_read_cache();
  if (!prepared.ok)
    return prepared;
  services.prepare_checkpoint_read_cache();
  // RouteCache 由紧接的 owner 入口在原互斥锁内封存；失败仍由原调用者拒绝。
  return common_success();
}

} // namespace hanmo::resident
