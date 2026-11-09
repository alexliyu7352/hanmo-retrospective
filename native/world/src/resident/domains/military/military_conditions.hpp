#include "resident/domains/military/military_refs.hpp"
// 真实旅行执行驱动部队条件；收据复用行旅事实、供给和接敌锚点，不扣第二份钱粮。
#pragma once
#include "resident/domains/military/training_model.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryConditionSourceProjection {MilitaryConditionProfile condition;StableId last_source_id;};
// 同原条件折叠，额外返回末个实际条件来源；不会生成或替换任何历史。
Outcome<MilitaryConditionSourceProjection> fold_military_condition_source(const MilitaryState&,const MilitaryCatalog&,const StableId& unit,Integer through_tick);
// 仅真实M10入战头前可查询；同tick实际课时/行军/休整必须先于入口，严格单步才能观察to。
Outcome<MilitaryConditionSourceProjection> query_military_condition_before_rout_entry(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& unit,const StableId& actual_entry_fact_id,const DynamicStepInput* phase=nullptr);
struct MilitaryMarchApplied { StableId receipt_id;bool already_completed=false; };
Status validate_military_marches(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,Integer through_tick);
Outcome<MilitaryMarchApplied> apply_military_march(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const TerrainExecution&,Integer finish_tick,const StableId& contact_source_id={},const std::optional<TerrainExecution>& uncut={});
// 完整历史先验下只折叠某实际半日之前的条件；后发课时/行军不能回填旧休整起点。
Outcome<MilitaryConditionProfile> fold_military_condition_before_period(const MilitaryState&,const MilitaryCatalog&,const StableId& unit,Integer period_tick);
Outcome<MilitaryConditionProfile> fold_military_condition(const MilitaryState&,const MilitaryCatalog&,const StableId& unit,Integer through_tick);
}
