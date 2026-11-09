// Common JSON与独立typed认可账必须双向逐值相等；结果绑定只放独立尾，不伪装为业务事实。
#pragma once
#include "collateral_control_state.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status emit_collateral_recognition_fact_payloads_fields(ProjectionWriter&,const CommonStore&,const CollateralControlState*);
Outcome<BoundaryValue> collateral_recognition_fact_payloads(const CommonStore&,const CollateralControlState*);
Status validate_collateral_recognition_json_payloads(const CommonStore&,const CollateralControlState*,const BoundaryValue& full_state);
}
