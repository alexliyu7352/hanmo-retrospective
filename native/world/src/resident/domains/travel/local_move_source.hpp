// 原 MoveLocal 的有限来源；不授权移动、不重放 on_enter、不借到达或 Done 头。
#pragma once
#include "spatial_location_capture.hpp"
#include "actor_spatial_source_membership.hpp"
namespace hanmo::resident {
// 只核持值及真实 Common 前缀；不递归完整空间历史。
Status validate_local_move_value(const LocalMoveSource&, ExactArithmeticBudget = {});
Outcome<const LocalMoveSource*> read_local_move_source(const CommonStore&,
    const MovementExactState&, const SourceRef&, Integer schema, ExactArithmeticBudget = {});
Status validate_local_move_headers(const CommonState&, const MovementExactState*, bool selected,
                                  ExactArithmeticBudget = {});
// 仅原 dispatcher 在 preview 成功后调用；before 必须来自同候选原空间读口，at 来自真实 owner。
// 本函数只调用原位置 writer 一次、登记真实头并绑定原 tap；调用方随后仍执行原 after_local_move。
Outcome<BoundSpatialLocationWrite> apply_local_move_with_source_in_candidate(
    CommonStore&, MovementExactState&, const TravelCatalog&, const StableId& actor,
    const StableId& place, const WorldMoment& at, const ActorSpatialLocationView& before,
    ExactArithmeticBudget = {});
// 完整冷门逐行沿原同 actor 历史证明前驱；不能只以当前位置或表存在充作来源。
Status validate_local_move_source_history(const ActorSpatialHeadSourceReadContext&,
    const MilitarySpatialHandoffState* = nullptr,
    const NamedMilitaryCustodyPlacementRead* = nullptr,
    const FieldBattleExitPlacementRead* = nullptr);
} // namespace hanmo::resident
