// 原柜台查询的八身份字段拥有返回；不授予接册/司法权，不扩展任何来源选型。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "political_shop_registry_contract.hpp"
namespace hanmo::resident {
class OwnedPoliticalShopRegistryEndpoint {
  CheckpointMemoryCharge memory;
  PoliticalShopRegistryEndpoint value;

public:
  OwnedPoliticalShopRegistryEndpoint() = default;
  // 仅拥有这些明确文本，不自行证明源或权限。
  OwnedPoliticalShopRegistryEndpoint(const StableId &court,
                                     const StableId &jurisdiction,
                                     const StableId &organization,
                                     const StableId &office,
                                     const StableId &site, const StableId &role,
                                     const StableId &dialogue,
                                     const StableId &node);
  OwnedPoliticalShopRegistryEndpoint(OwnedPoliticalShopRegistryEndpoint &&other)
      : value(std::move(other.value)) {
    memory = std::move(other.memory);
  }
  OwnedPoliticalShopRegistryEndpoint &
  operator=(OwnedPoliticalShopRegistryEndpoint &&) = delete;
  OwnedPoliticalShopRegistryEndpoint(
      const OwnedPoliticalShopRegistryEndpoint &) = delete;
  OwnedPoliticalShopRegistryEndpoint &
  operator=(const OwnedPoliticalShopRegistryEndpoint &) = delete;
  const PoliticalShopRegistryEndpoint &endpoint() const noexcept {
    return value;
  }
};
} // namespace hanmo::resident
