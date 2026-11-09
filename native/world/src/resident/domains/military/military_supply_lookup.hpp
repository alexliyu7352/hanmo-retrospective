// 仅为原同栈调用建立供养 ID 候选索引；不保存任何历史验证结果或持久字段。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <cstddef>
#include <memory>
namespace hanmo::resident {
struct CommonState;
class CommonStore;
struct MilitaryState;
struct MilitarySupplyFact;
struct SourceRef;
// 每个入口独立拥有借用期；嵌套入口、另一世界和另一 job 不共用候选索引。
class MilitarySupplyLookupPass final {
public:
  MilitarySupplyLookupPass(const CommonState&, const MilitaryState&) noexcept;
  ~MilitarySupplyLookupPass() noexcept;
  MilitarySupplyLookupPass(const MilitarySupplyLookupPass&) = delete;
  MilitarySupplyLookupPass& operator=(const MilitarySupplyLookupPass&) = delete;
  // 在原 apply 结束处释放，不能把候选借用带到发布或另一原阶段。
  void stop() noexcept;
private:
  struct Index;
  const CommonState* common_;
  const MilitaryState* military_;
  MilitarySupplyLookupPass* previous_;
  std::shared_ptr<Index> index_;
  bool active_ = true;
  friend void invalidate_military_supply_lookup(const MilitaryState&) noexcept;
  friend const MilitarySupplyFact* find_field_supply_candidate(
      const CommonStore&, const MilitaryState&, const SourceRef&, Integer, std::size_t);
};
// 必须在真实供养表写入之前撤销全部同根活借用，包括追加失败的路径。
void invalidate_military_supply_lookup(const MilitaryState&) noexcept;
// 原来源检查之后只查 ID；重复仅在被请求的 ID 上按原错误拒绝。
const MilitarySupplyFact* find_field_supply_candidate(
    const CommonStore&, const MilitaryState&, const SourceRef&, Integer, std::size_t);
} // namespace hanmo::resident
