#include "private_shop_opening_provenance.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
namespace {
class Clients final : public TaskFulfilledClientProvenanceSink {
  PrivateShopOpeningProvenanceSink &output_;

public:
  explicit Clients(PrivateShopOpeningProvenanceSink &out) : output_(out) {}
  Status
  checked_source(const TaskFulfilledClientContext &x,
                 const TaskFulfilledClientCheckedSource &source) override {
    return output_.task_source(x, source);
  }
  Status actual_roots(const TaskFulfilledClientContext &x,
                      const StableId &actor,
                      const TaskFulfilledClientBoundary &boundary) override {
    return output_.task_clients(x, actor, boundary);
  }
};
} // namespace
Status visit_original_private_shop_opening_provenance(
    const PrivateShopContext &x, const SourceRef &source,
    PrivateShopOpeningProvenanceSink &sink) {
  // 原 full read 完整核身份、历史年龄、商术、客户与原司法许可；不复制算法。
  auto verified = read_original_private_shop_opening(x, source);
  if (!verified.ok)
    return {false, {}, verified.error};
  const auto &shop = *verified.value;
  if (!x.study_history || !x.permissions)
    return common_failure("PrivateShopOpeningProvenanceRootsRequired", shop.id);
  auto result = sink.opening(x, shop);
  if (!result.ok)
    return result;
  result = sink.growth_study(*x.study_history, x.skill_origins);
  if (!result.ok)
    return result;
  Clients clients(sink);
  result = x.clients.visit_before_shop_provenance(
      shop.terms.proprietor_actor_id, shop.id, shop.opened_tick, clients);
  if (!result.ok)
    return result;
  // 该请求是原不可变租约身份，不是任意授权；真实 provider 再走同一 full 口。


  auto memory = CheckpointMemoryCharge(0);
  PrivateShopPermissionRequest request{shop.terms.proprietor_actor_id,
                                       shop.terms.premises_id,
                                       shop.terms.place_id};
  return x.permissions->visit_opening_permission_provenance(
      request, shop.id, shop.opened_tick, sink);
}
} // namespace hanmo::resident
