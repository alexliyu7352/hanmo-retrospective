// Naval段2专用严格冷边界；只有显式combat_policy启用才可调用，不给段1添加存在位。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident::naval_battle_codec {
Status emit_fields(ProjectionWriter&,const NavalBattleState&);
BoundaryValue encode(const NavalBattleState&);
Outcome<NavalBattleState> decode(const BoundaryValue&,bool combat_enabled);
Status visit(CodecVisitor&,NavalBattleState&,bool combat_enabled);
// 回合账审计只导出明确typed事实，热规则不保存BoundaryValue。
BoundaryValue encode_capture(const NavalCaptureFact&);
BoundaryValue encode_round(const NavalRoundFact&);
BoundaryValue encode_frame(const NavalTacticalFrame&);
// 命令边界复用同一战术字段定义，未知键和重复部队仍严格拒绝。
Outcome<NavalTacticalOrders> decode_orders(const BoundaryValue&);
BoundaryValue encode_orders(const NavalTacticalOrders&);
Outcome<std::vector<NavalVesselAssignment>> decode_assignments(const BoundaryValue&);
}
