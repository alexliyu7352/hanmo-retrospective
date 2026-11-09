// History2原三业务执行证据；只保存已发生值，不拥有可再次执行的recipe或库存。
#pragma once
#include "growth_market_resource_contract.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <cstdint>
#include <variant>
namespace hanmo::resident {
// 原业务阶段数与顺序保持；空读阶段不冒充受保护写步骤。
enum class MarketCaptureStage { trade=0,production_inputs=1,production_outputs=2,ration=3 };
struct MarketLegacyOriginalStep {};
struct MarketSelectedTradeRows {
  StableId life_activity_id;
  Integer source_schema=2;
  std::vector<std::uint32_t> row_ordinals;
};
struct MarketOriginalBusinessFifo {
  StableId business_fact_id;
  std::uint32_t execution_stage_ordinal=0;
};
using MarketCaptureSourceBinding=std::variant<MarketLegacyOriginalStep,
    MarketSelectedTradeRows,MarketOriginalBusinessFifo>;
struct MarketOriginalTick {};
struct MarketScalarActualStart { WorldMoment started_at;StableId action_ref; };
using MarketOriginalTimeBasis=std::variant<MarketOriginalTick,MarketScalarActualStart>;
struct MarketProtectedHistoryBefore {};
struct MarketFullCapturedBefore { InventoryView inventory; };
struct MarketPriorPinnedStageAfter {
  StableId movement_id;
  std::uint32_t execution_stage_ordinal=0;
};
using MarketInventoryBeforeBasis=std::variant<MarketProtectedHistoryBefore,
    MarketFullCapturedBefore,MarketPriorPinnedStageAfter>;
struct OriginalBusinessInventoryFrame {
  StableId inventory_id;
  MarketInventoryBeforeBasis before_basis;
  std::optional<MarketInventoryChange> after_delta;
  FieldOrder field_order;
};
struct MarketScalarTradeSlot {};
struct MarketProductionInputSlot { std::uint32_t receipt_input_ordinal=0; };
struct MarketProductionOutputSlot { std::uint32_t receipt_output_ordinal=0; };
struct MarketRationConsumptionSlot {};
using MarketOriginalSourceSlot=std::variant<MarketScalarTradeSlot,
    MarketProductionInputSlot,MarketProductionOutputSlot,MarketRationConsumptionSlot>;
struct MarketNotConsumption {};
struct MarketOriginalMaterialNeed {
  ActorFoundationRuleRef rule_ref;
  std::uint32_t need_slot_ordinal=0;
};
using MarketOriginalMaterialBasis=std::variant<MarketNotConsumption,MarketOriginalMaterialNeed>;
struct MarketNotOutput {};
struct MarketOriginalPlainAdd {};
struct MarketRegisteredProductionOutput {
  std::uint32_t output_ordinal=0;
  LotPropertiesRef properties_ref;
};
using MarketOriginalOutputBasis=std::variant<MarketNotOutput,
    MarketOriginalPlainAdd,MarketRegisteredProductionOutput>;
// 导入Common实际flow的七字段和tag；不持久保存其外围draft或recipe。
struct MarketOriginalLotFlow {
  ResourceLotFlowDraft::Kind kind=ResourceLotFlowDraft::Kind::add;
  StableId source_inventory_id,source_lot_id,destination_inventory_id,destination_lot_id,item_id;
  Integer quantity=0;
};
struct OriginalBusinessItemOperation {
  std::uint32_t operation_index=0;
  MarketOriginalSourceSlot source_slot;
  std::vector<CargoSelection> selections;
  MarketOriginalMaterialBasis material_need;
  MarketOriginalOutputBasis output_basis;
  std::vector<MarketOriginalLotFlow> lot_flows;
  FieldOrder field_order;
};
// 沿原TradeWalletPosting全部字段，预约保持原typed身份和顺序，不存新余额账。
struct TradeWalletPosting {
  StableId wallet_id;
  Owner owner;
  Integer delta_qian=0,balance_before=0,balance_after=0;
  std::vector<Reservation> reservations_before,reservations_after;
  Integer spendable_before=0;
};
struct OriginalBusinessStage {
  MarketCaptureStage stage=MarketCaptureStage::trade;
  Integer next_instance_before=1,next_instance_after=1;
  std::vector<OriginalBusinessInventoryFrame> inventory_frames;
  std::vector<OriginalBusinessItemOperation> item_operations;
  std::vector<TradeWalletPosting> wallet_postings;
  FieldOrder field_order;
};
struct OriginalBusinessFifoExecution {
  Integer schema_version=1;
  ActorFoundationRuleRef adapter_rule_ref;
  MarketOriginalTimeBasis time_basis;
  std::vector<OriginalBusinessStage> stages;
  FieldOrder field_order;
};
} // namespace hanmo::resident
