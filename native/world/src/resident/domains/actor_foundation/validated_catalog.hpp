// 只封口原作者目录内容；世界来源、精确时刻及业务读取仍逐次核验。
#pragma once
#include <cstdint>
#include <memory>
namespace hanmo::resident {
struct ActorFoundationCatalog;
// 将已结束作者装配的值交给真实 const owner；首次原完整验证成功后方可复用。
std::shared_ptr<const ActorFoundationCatalog>
seal_actor_foundation_catalog(ActorFoundationCatalog &&);
namespace actor_foundation_catalog_reuse {
// 默认关闭的线程局部整数观察；不触业务资源、时钟或目录字段。
struct Counts {
  std::uint64_t requests = 0, reused = 0, full_validation_begins = 0,
                full_validation_successes = 0, sealed_creations = 0;
  bool overflow = false;
};
bool set_counting_enabled(bool) noexcept;
Counts counts() noexcept;
} // namespace actor_foundation_catalog_reuse
} // namespace hanmo::resident
