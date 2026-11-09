// 原 LE 需求的纯材料读口；只借真实库存帧，不写库存或授予新的加工/供养权限。
#pragma once
#include "economic_market_cause.hpp"
#include "resident/domains/craft/item_service.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <cstdint>
#include <span>
namespace hanmo::resident {
enum class EconomicMaterialKind { production_input, ration };
struct EconomicMaterialNeedKey {
  EconomicMaterialKind kind = EconomicMaterialKind::production_input;
  const StableId &original_business_key;
  std::uint32_t need_slot_ordinal = 0;
};
// RuleRef 的字符串只借原冻结目录；不复制另一份可编辑的需求定义。
struct EconomicMaterialNeedRead {
  const StableId *item_id = nullptr;
  const StableId *rule_id = nullptr;
  const StableId *catalog_digest = nullptr;
  Integer rule_version = 1;
  std::uint32_t need_slot_ordinal = 0;
};
struct EconomicMaterialSources {
  const CommonStore &common;
  const CommonCatalog &shared;
  const CatalogIdentity &identity;
  const LocalEconomyCatalog &economy;
  const CurrentMomentRead &clock;
  // 只允许原 Item/Properties 模块均未选时为空，不以空表推断未选。
  const ItemInstancesState *instances = nullptr;
  const LotPropertiesState *properties = nullptr;
  const ItemServiceCatalog *item_catalog = nullptr;
  const ItemOutputSourceRead *outputs = nullptr;
};
struct EconomicMaterialFifoRow {
  const LotView *lot = nullptr;
  Integer quantity = 0;
};
struct EconomicMaterialFifoRead {
  CheckpointVector<EconomicMaterialFifoRow> selections;
  Integer quantity = 0;
};
class NativeEconomicMaterialRead final {
  EconomicMaterialSources x_;

public:
  explicit NativeEconomicMaterialRead(EconomicMaterialSources x) : x_(x) {}
  const EconomicMaterialSources &sources() const noexcept { return x_; }
  Status validate_bindings() const;
  bool bound_to(const CommonStore &, const CommonCatalog &,
                const LocalEconomyCatalog &) const noexcept;
  bool bound_to(const CommonStore &c,
                const LocalEconomyCatalog &e) const noexcept {
    return &c == &x_.common && &e == &x_.economy;
  }
  Outcome<EconomicMaterialNeedRead>
  read_need(const EconomicMaterialNeedKey &) const;
  // 完整 Owner 由原业务独立反读；此处仅核它仍是原冻结条款所准的 Owner。
  // cold before_result 必为本笔真实 LE 收据头，热调用为空且直接借当前真实帧。
  Outcome<Integer> count(const EconomicMaterialNeedKey &, const InventoryView &,
                         const Owner &, Integer actual_tick,
                         const SourceRef *before_result = nullptr) const;
  Outcome<EconomicMaterialFifoRead>
  select(const EconomicMaterialNeedKey &, const InventoryView &, const Owner &,
         Integer quantity, Integer actual_tick,
         const SourceRef *before_result = nullptr) const;
  Status visit_candidate_pins(const EconomicMaterialNeedKey &,
                              const InventoryView &, const Owner &,
                              Integer actual_tick, LotPropertiesSourcePinSink &,
                              const SourceRef *before_result = nullptr) const;
};
struct EconomicMaterialConsumption {
  EconomicMaterialNeedKey need;
  const InventoryView &inventory;
  const Owner &owner;
  Integer quantity = 0;
};
// 仅短寿命原 recipe；费用随 selections 一直保留到原 apply 完成。
class EconomicMaterialConsumptionRecipe {
  CheckpointCopyLease copied_fields_;
  ResourceRecipe recipe_;
  explicit EconomicMaterialConsumptionRecipe(CheckpointCopyLease memory)
      : copied_fields_(std::move(memory)) {}
  friend Outcome<EconomicMaterialConsumptionRecipe>
  prepare_economic_material_consumption(
      const NativeEconomicMaterialRead &,
      std::span<const EconomicMaterialConsumption>, Integer);

public:
  EconomicMaterialConsumptionRecipe() = default;
  EconomicMaterialConsumptionRecipe(
      EconomicMaterialConsumptionRecipe &&) noexcept = default;
  EconomicMaterialConsumptionRecipe &
  operator=(EconomicMaterialConsumptionRecipe &&v) noexcept {
    if (this != &v) {
      std::swap(copied_fields_, v.copied_fields_);
      recipe_.operations.swap(v.recipe_.operations);
      recipe_.expected_versions.swap(v.recipe_.expected_versions);
    }
    return *this;
  }
  EconomicMaterialConsumptionRecipe(const EconomicMaterialConsumptionRecipe &) =
      delete;
  EconomicMaterialConsumptionRecipe &
  operator=(const EconomicMaterialConsumptionRecipe &) = delete;
  const ResourceRecipe &recipe() const noexcept { return recipe_; }
};
Outcome<EconomicMaterialConsumptionRecipe>
prepare_economic_material_consumption(
    const NativeEconomicMaterialRead &,
    std::span<const EconomicMaterialConsumption>, Integer actual_tick);
} // namespace hanmo::resident
