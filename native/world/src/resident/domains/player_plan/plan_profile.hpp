// 同一有限生活计划的显式物理版本选择；未知或缺Time2不能靠存档字段自升版。
#pragma once
#include "plan_model.hpp"
namespace hanmo::resident {
Outcome<Integer> decode_player_plan_profile_version(const BoundaryValue& scenario);
Outcome<Integer> read_player_plan_selected_version(const CatalogIdentity&);
Status validate_player_plan_selected_version(const PlayerPlanState&,const CatalogIdentity&,bool enabled,bool complete_time2_selected);
}
