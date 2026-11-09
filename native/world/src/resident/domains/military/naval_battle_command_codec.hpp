// 战斗命令和回执只接受已选择的 Naval 段2；资产段1与 feature256 均不能单独授权。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "naval_battle_codec.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct NavalBattleCommandReceipt;
Status emit_receipt_fields(ProjectionWriter&,const NavalBattleCommandReceipt&,Integer version);

using NavalBattleCommand=std::variant<NavalPlanCommand,NavalCaptureCommand,NavalClaimDeclineCommand,NavalContinueCommand>;
struct NavalBattleCommandReceipt {
 StableId kind,request_key,receipt_id,battle_id,round_source_id,actor_id;Integer tick=0;FieldOrder field_order;
};
struct NavalBattleCommandQuote { NavalBattleCommandReceipt receipt;bool replayed=false; };
namespace naval_battle_command_codec {
bool command_kind(const StableId&);
StableId command_kind(const NavalBattleCommand&);
Outcome<NavalBattleCommand> decode_command(const BoundaryValue&,Integer selected_naval_version);
BoundaryValue encode_receipt(const NavalBattleCommandReceipt&,Integer selected_naval_version);
Outcome<NavalBattleCommandReceipt> decode_receipt(const BoundaryValue&,Integer selected_naval_version);
Status visit_receipt(CodecVisitor&,NavalBattleCommandReceipt&,Integer selected_naval_version);
BoundaryValue encode_quote(const NavalBattleCommandQuote&,Integer selected_naval_version);
// 缓存必须精确引用同请求人的唯一永久事实，丢掉 result 缓存也不能放过旧 profile 的 history。
Status validate_history_version(const CommonState&,Integer selected_naval_version);
Status validate_cached_receipt(const NavalBattleState&,const NavalBattleCommandReceipt&,const StableId& actor_id,Integer selected_naval_version);
NavalBattleCommandReceipt receipt(const NavalPlanFact&);
NavalBattleCommandReceipt receipt(const NavalCaptureFact&);
NavalBattleCommandReceipt receipt(const NavalClaimDeclineFact&);
NavalBattleCommandReceipt receipt(const NavalContinueFact&);
}
}
