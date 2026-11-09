// 具名任职的中央接缝只转交真实领域命令、借用责任和核对回执来源。
#pragma once
#include "world_bundle.hpp"
namespace hanmo::resident {
bool named_membership_command_kind(const StableId&);
Outcome<CommandPayload> decode_named_membership_command(const StableId&,const BoundaryValue&);
Outcome<MembershipOfferQuote> preview_named_membership(const WorldBundle&,const StableId&,const CommandPayload&);
Outcome<MembershipOfferEffects> apply_named_membership(WorldBundle&,const StableId&,const CommandPayload&);
Status validate_named_membership_world(const WorldBundle&);
}
