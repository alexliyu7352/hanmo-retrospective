// 认可真实请求永久绑定；裁掉缓存后也不能把同tick旧关系冒充本次双方声明。
#pragma once
#include "collateral_control_state.hpp"
namespace hanmo::resident {
Status validate_collateral_recognition_effect(const CollateralControlState&,const StableId& fact,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
Status validate_collateral_recognition_result_bindings(const CommonStore&,const CollateralControlState&);
Status bind_collateral_recognition_result_in_candidate(CommonStore&,const CollateralControlState& before,CollateralControlState& after,const StableId& actor,Integer request_seq,const CommitResult& actual_result);
Outcome<StableId> read_collateral_recognition_for_result(const CommonStore&,const CollateralControlState&,const CommitResult& actual_result);
}
