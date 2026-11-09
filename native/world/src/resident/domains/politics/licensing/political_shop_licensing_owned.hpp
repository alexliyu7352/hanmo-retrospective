// 实际命令产物的同job拥有返回；不得剥离IDs后丢掉原租约。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
struct OwnedPoliticalShopLicensingEffects {
  CheckpointMemoryCharge memory;
  CheckpointVector<StableId> fact_ids;
  OwnedPoliticalShopLicensingEffects() = default;
  OwnedPoliticalShopLicensingEffects(OwnedPoliticalShopLicensingEffects &&) =
      default;
  OwnedPoliticalShopLicensingEffects &
  operator=(OwnedPoliticalShopLicensingEffects &&) = delete;
  OwnedPoliticalShopLicensingEffects(
      const OwnedPoliticalShopLicensingEffects &) = delete;
  OwnedPoliticalShopLicensingEffects &
  operator=(const OwnedPoliticalShopLicensingEffects &) = delete;
};
// 报价的普通字符串/容器租约先于真实payload，跨正式prepare/apply保持。
class OwnedPoliticalShopLicensingQuote {
  CheckpointMemoryCharge memory_;
  PoliticalShopLicensingQuote value_;
  friend Outcome<OwnedPoliticalShopLicensingQuote>
  preview_political_shop_licensing_owned(const PoliticalShopLicensingContext &,
                                         const StableId &,
                                         const PoliticalShopLicensingCommand &);

public:
  OwnedPoliticalShopLicensingQuote() = default;
  OwnedPoliticalShopLicensingQuote(OwnedPoliticalShopLicensingQuote &&other)
      : value_(std::move(other.value_)) {
    memory_ = std::move(other.memory_);
  }
  OwnedPoliticalShopLicensingQuote &
  operator=(OwnedPoliticalShopLicensingQuote &&) = delete;
  OwnedPoliticalShopLicensingQuote(const OwnedPoliticalShopLicensingQuote &) =
      delete;
  OwnedPoliticalShopLicensingQuote &
  operator=(const OwnedPoliticalShopLicensingQuote &) = delete;
  const PoliticalShopLicensingQuote &quote() const noexcept { return value_; }
};
Outcome<OwnedPoliticalShopLicensingQuote>
preview_political_shop_licensing_owned(const PoliticalShopLicensingContext &,
                                       const StableId &actor,
                                       const PoliticalShopLicensingCommand &);
Outcome<OwnedPoliticalShopLicensingEffects>
apply_political_shop_licensing_owned_in_candidate(
    CommonStore &, PoliticalShopLicensingState &,
    const PoliticalShopLicensingContext &, const StableId &actor,
    const PoliticalShopLicensingCommand &, const PoliticalShopLicensingQuote &);
} // namespace hanmo::resident
