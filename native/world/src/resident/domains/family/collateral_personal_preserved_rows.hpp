// 原A命令只许新增一条表达；借原拥有字段逐值核旧表，不复制/序列化整个根。
#pragma once
#include "collateral_control_state.hpp"
#include "collateral_personal_commands2.hpp"
namespace hanmo::resident {
Status validate_collateral_personal_preserved_rows2(
    const CollateralControlState &before, const CollateralControlState &after,
    const StableId &actual_new_id, std::string_view actual_new_kind);
// 唯一owned_fields递归比较供真实执行源复用；只是payload映射，不授业务许可。
Status validate_collateral_personal_applied_request2(
    const ProposeCollateralRecognition2 &, const CollateralPersonalProposal2 &);
Status validate_collateral_personal_applied_request2(
    const RecognizeCollateral2 &, const CollateralPersonalAccepted2 &);
Status validate_collateral_personal_applied_request2(
    const RespondCollateralRecognition2 &, const CollateralPersonalReply2 &);
} // namespace hanmo::resident
