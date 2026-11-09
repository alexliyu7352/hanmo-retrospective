// Naval命令/公开回执的严格冷边界，只供中央显式feature256分支调用。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "naval_commands.hpp"
#include "naval_asset_codec.hpp"
namespace hanmo::resident::naval_command_codec {
Status emit_receipt_fields(ProjectionWriter&,const NavalCommandReceipt&);

inline constexpr std::uint32_t checkpoint_feature=256u;
Outcome<NavalCommand> decode_command(const BoundaryValue&);
BoundaryValue encode_quote(const NavalCommandQuote&);
BoundaryValue encode_receipt(const NavalCommandReceipt&);
Outcome<NavalCommandReceipt> decode_receipt(const BoundaryValue&);
Status visit_receipt(CodecVisitor&,NavalCommandReceipt&);
// 野外接任profile的冷口另外借不可变Military形成源，不改原receipt字段或旧重载。
Status validate_cached_receipt(const MilitaryState&,const NavalAssetState&,const NavalCommandReceipt&,const StableId& actor_id);
Status validate_cached_receipt(const NavalAssetState&,const NavalCommandReceipt&,const StableId& actor_id);
}
