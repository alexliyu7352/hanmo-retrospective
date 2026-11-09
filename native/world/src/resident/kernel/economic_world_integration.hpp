// S5仅组合既有WorldBundle和领域业务；公共variant、存档身份与最终index赋值由中央登记。
#pragma once
#include "resident/domains/local_economy/production_management.hpp"
#include "resident/domains/local_economy/views/local_economy_view.hpp"
#include "resident/domains/local_economy/views/production_history.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement_view.hpp"
namespace hanmo::resident {
struct WorldBundle;
bool production_management_enabled(const WorldBundle&);
Outcome<ProductionConfigurationQuote> prepare_economic_configuration(const WorldBundle&,const StableId& actor_id,const ConfigureLocalProductionCommand&);
Outcome<LocalEconomyEffects> apply_economic_configuration(WorldBundle&,const StableId& actor_id,const ConfigureLocalProductionCommand&);
// 返回派生值，由中央在最终world/epoch/revision就位后赋给自己的字段；不进checkpoint。
LocalEconomyIndex rebuild_economic_world_index(const WorldBundle&);
Outcome<LocalEconomyPage> read_economic_world(const WorldBundle&,const LocalEconomyIndex&,const LocalEconomyQuery&);
// SDK local_economy只接当前本人现场页；严格边界与权限留在本域薄叶。
Outcome<BoundaryValue> query_economic_world(const WorldBundle&,const LocalEconomyIndex&,const BoundaryValue& args);
// 财政从同一政治真源读取，独立责任状态不放进旧地方经济codec。
FiscalSettlementIndex rebuild_economic_fiscal_index(const WorldBundle&);
Status validate_economic_fiscal_world(const WorldBundle&);
Outcome<LocalEconomyEffects> step_economic_fiscal_world(WorldBundle&,const DynamicStepInput&);
Outcome<BoundaryValue> query_fiscal_taxpayer_continuity_world(const WorldBundle&,const BoundaryValue&);
Outcome<BoundaryValue> query_economic_fiscal_world(const WorldBundle&,const FiscalSettlementIndex&,const BoundaryValue&);
BoundaryValue encode_economic_configuration_effects(const LocalEconomyEffects&);
Outcome<LocalEconomyEffects> decode_economic_configuration_effects(const BoundaryValue&);
Status visit_economic_configuration_effects(CodecVisitor&,LocalEconomyEffects&);
Status validate_economic_configuration_receipts(const WorldBundle&);

Outcome<BoundaryValue> query_economic_production_history(const WorldBundle&,const LocalEconomyIndex&,const BoundaryValue& args);

}
