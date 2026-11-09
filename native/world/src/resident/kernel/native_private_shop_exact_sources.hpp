// Shop3/Law2 只借同一实际 Complete 前序；不反取工厂，不复制世界或年龄账。
#pragma once
#include "native_organization_runtime.hpp"
#include "original_world_read_boundary.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/commerce/private_shop_age3.hpp"
#include "resident/domains/commerce/private_shop_opening_source3.hpp"

namespace hanmo::resident {
struct NativeCompleteWorldCoreSources;
// 原 Core 和 World 先建立并晚于本 owner 析构；后续 Law/Shop 再借本 owner。
class NativePrivateShopExactSources final : public CheckpointAllocatedObject {
  const NativeCompleteWorldCoreSources &core_;
  CharacterConditionSources body_;
  NativeOrganizationAuthorityClock authority_clock_;
  OriginalWorldReadBoundary read_boundary_;
  PrivateShopOpeningSources3 opening_;
  PrivateShopAgeSources3 age_;
  NativePrivateShopExactSources(const NativeCompleteWorldCoreSources &,
                                CharacterConditionSources);

public:
  NativePrivateShopExactSources(const NativePrivateShopExactSources &) = delete;
  NativePrivateShopExactSources &
  operator=(const NativePrivateShopExactSources &) = delete;
  NativePrivateShopExactSources(NativePrivateShopExactSources &&) = delete;
  NativePrivateShopExactSources &
  operator=(NativePrivateShopExactSources &&) = delete;
  static Outcome<std::unique_ptr<NativePrivateShopExactSources>>
  create(const NativeCompleteWorldCoreSources &);
  bool bound_to(const WorldBundle &) const noexcept;
  Status validate_bindings() const;
  const CharacterConditionSources &body() const noexcept { return body_; }
  const OrganizationAuthorityClock &authority_clock() const noexcept {
    return authority_clock_;
  }
  const CurrentMomentRead &clock() const noexcept { return opening_.clock; }
  const OriginalWorldReadBoundary &read_boundary() const noexcept {
    return read_boundary_;
  }
  const PrivateShopOpeningSources3 &opening() const noexcept {
    return opening_;
  }
  const PrivateShopAgeSources3 &age() const noexcept { return age_; }
};
} // namespace hanmo::resident
