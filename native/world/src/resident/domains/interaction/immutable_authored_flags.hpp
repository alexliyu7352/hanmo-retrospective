// 原作者flags的不可变凭据；只供中央已选无业务flag writer分支，不增世界账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <vector>
namespace hanmo::resident {
struct FlagRecord;
struct InteractionState;
struct BoundaryValue;
class CheckpointRetentionCensus;
class AuthoredInteractionFlags final {
  struct CaptureTag {
  private:
    CaptureTag() {}
    friend class AuthoredInteractionFlags;
  };
  // 首字段先收费、末析构；目录锁及原字符串均不能早于租约消亡。
  CheckpointMemoryCharge memory_;
  CatalogIdentity identity_;
  std::vector<FlagRecord> flags_;

public:
  // 仅capture可产生私有tag；参数只来自该次真实data.flags，不接受运行态反填。
  AuthoredInteractionFlags(CaptureTag, CheckpointMemoryCharge,
                           const CatalogIdentity &, const BoundaryValue &);
  ~AuthoredInteractionFlags();
  AuthoredInteractionFlags(const AuthoredInteractionFlags &) = delete;
  AuthoredInteractionFlags &
  operator=(const AuthoredInteractionFlags &) = delete;
  static Outcome<std::shared_ptr<const AuthoredInteractionFlags>>
  capture(const BoundaryValue &original_data,
          const CatalogIdentity &locked_identity);
  const CatalogIdentity &identity() const noexcept;
  const std::vector<FlagRecord> &rows() const noexcept;
  void retain(CheckpointRetentionCensus &) const;
};
// 不要求原排列相同，但两个根都须唯一且全覆盖、值逐项相等；空作者表只能配空状态。
// 不比较旧profile，也不凭本函数自行宣布当前选型已无writer；准入由中央锁定。
Status
validate_immutable_interaction_flags(const AuthoredInteractionFlags &,
                                     const CatalogIdentity &expected_lock,
                                     const InteractionState &);
// 必先整根核验；返回原immutable行，不把缺失id当false，也不制造Common来源。
Outcome<const FlagRecord *>
read_immutable_interaction_flag(const AuthoredInteractionFlags &,
                                const CatalogIdentity &expected_lock,
                                const InteractionState &, const StableId &id);
} // namespace hanmo::resident
