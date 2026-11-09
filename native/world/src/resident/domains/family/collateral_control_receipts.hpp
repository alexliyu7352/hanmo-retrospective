// 认可不代本人许可；此尾只把原唯一Legacy三表钉到本次真实请求，不再保存第二份控制事实。
#pragma once
#include "collateral_control_sources.hpp"
#include "family_legacy.hpp"
namespace hanmo::resident {
Status validate_collateral_control_receipt(const FamilyLegacyState&,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
// 原Legacy3完整历史、认可及Known来源先验；同时检查认可与控制跨表实际结果不复用。
Status validate_collateral_control_result_bindings(const CommonStore&,const FamilyLegacyState&,const CollateralControlCatalog&,const CollateralControlState&);
// 必须先绑定同次认可，再调用本口；只读真正before/after，不追认旧未绑定来源。
Status bind_collateral_control_result_in_candidate(CommonStore&,const FamilyLegacyState& before,const FamilyLegacyState& after,const CollateralControlCatalog&,const CollateralControlState& before_bindings,CollateralControlState&,const StableId& actor,Integer request_seq,const CommitResult& actual_result);
Outcome<StableId> read_collateral_control_source_for_result(const CommonStore&,const FamilyLegacyState&,const CollateralControlCatalog&,const CollateralControlState&,const CommitResult& actual_result);
}
