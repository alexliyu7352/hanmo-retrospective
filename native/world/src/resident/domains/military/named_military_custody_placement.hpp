// S1 只借同 Military 根的真实释放落点；完整期限/最优落点校验由整根领域入口负责。
#pragma once
#include "named_military_custody.hpp"
namespace hanmo::resident {
class NamedMilitaryCustodyPlacementRead {
public:
  virtual ~NamedMilitaryCustodyPlacementRead() = default;
  virtual bool bound_to(const CommonRead &, const MovementExactState &) const noexcept = 0;
  // 原首次捕获区间必须对历史位置可见，不能沿释放叶跳回被俘前旧点。
  virtual Outcome<std::vector<const NamedMilitaryCustodyTerm *>>
      read_named_custody_capture_history(const StableId &actor_id) const = 0;
  virtual Outcome<const NamedMilitaryCustodyRelease *> read_named_custody_placement(
      const SourceRef &, const StableId &actor_id) const = 0;
  virtual Outcome<std::vector<const NamedMilitaryCustodyRelease *>>
      read_named_custody_placement_history(const StableId &actor_id) const = 0;
};
// 持有 context 不等于保活其借用根；中央必须以原 World scope 的 alias 保持同生期。
class OriginalNamedMilitaryCustodyPlacementRead final : public NamedMilitaryCustodyPlacementRead {
  NamedMilitaryCustodyContext context_;
  const MovementExactState &movement_;
public:
  OriginalNamedMilitaryCustodyPlacementRead(NamedMilitaryCustodyContext context,
      const MovementExactState &movement) : context_(context), movement_(movement) {}
  bool bound_to(const CommonRead &, const MovementExactState &) const noexcept override;
  Outcome<std::vector<const NamedMilitaryCustodyTerm *>>
      read_named_custody_capture_history(const StableId &actor_id) const override;
  Outcome<const NamedMilitaryCustodyRelease *> read_named_custody_placement(
      const SourceRef &, const StableId &actor_id) const override;
  Outcome<std::vector<const NamedMilitaryCustodyRelease *>>
      read_named_custody_placement_history(const StableId &actor_id) const override;
};
bool is_named_military_custody_placement_source(const SourceRef &) noexcept;
} // namespace hanmo::resident
