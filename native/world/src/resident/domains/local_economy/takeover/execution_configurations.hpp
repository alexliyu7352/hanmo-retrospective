// 接管只延后原计划的实际执行，原请求/首个生效时点及劳力预留不被重写。
#pragma once
#include "execution_phase.hpp"
namespace hanmo::resident {
Status activate_taken_production_configuration_in_candidate(CommonStore&,LocalEconomyState&,
    const LocalEconomyCatalog&,EconomicTakeoverPhaseContext&,const StableId& production_receipt_id,
    LocalEconomyEffects&);
Status validate_economic_taken_configuration_sources(const EconomicTakeoverReadContext&,
    const EconomicTakeoverExecutionState&);
// 只在上面的完整来源校验成功后用于原管理冷验的时点例外；不是执行授权。
bool economic_configuration_has_retained_delay(const EconomicTakeoverExecutionState&,const StableId& configuration_id);
// 当前单元、各设施及被替代原待配分别获准；不替代真实Office/Grant命令授权。
Outcome<std::optional<Owner>> economic_configuration_command_owner(const EconomicTakeoverReadContext&,const StableId& unit_id,const StableId& previous_configuration_id);
Status record_economic_configuration_command_sources(const EconomicTakeoverReadContext&,EconomicTakeoverExecutionState&,const ConfigureLocalProductionCommand&,const ProductionConfigurationQuote&,const LocalEconomyEffects&);
Status validate_economic_configuration_command_sources(const EconomicTakeoverReadContext&,const EconomicTakeoverExecutionState&);

}
