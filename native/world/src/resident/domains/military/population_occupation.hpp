// 征募只消费政治owner的原占领窗口；不扩大已有ArmyForm或退籍的语义。
#pragma once
#include "population_military_model.hpp"
namespace hanmo::resident {
Status validate_population_recruitment_occupation(const PopulationMilitaryReadContext&,
 const PopulationMilitarySource&,Integer effective_tick);
// 冷开始与成功完成各核一次原事实之前窗口，重占领不倒改旧合法登记。
Status validate_population_recruitment_occupation_before(const PopulationMilitaryReadContext&,
 const PopulationMilitarySource&,Integer event_tick,const StableId& action_or_enrollment_fact_id);
}
