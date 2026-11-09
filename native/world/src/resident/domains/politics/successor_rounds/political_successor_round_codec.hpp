// 新继位段自持生前意愿与正式就任引用；旧Organization的任何序列化字段不变。
#pragma once
#include "political_successor_round_contract.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_successor_round_state(const PoliticalSuccessorRoundState&);
Outcome<PoliticalSuccessorRoundState> decode_political_successor_round_state(const BoundaryValue&);
Status visit_political_successor_round_checkpoint(CodecVisitor&,PoliticalSuccessorRoundState&);
StableId political_successor_round_command_kind(const PoliticalSuccessorRoundCommand&);
bool is_political_successor_round_command(const StableId&);
BoundaryValue encode_political_successor_round_command(const PoliticalSuccessorRoundCommand&);
Outcome<PoliticalSuccessorRoundCommand> decode_political_successor_round_command(const StableId&,const BoundaryValue&);
}
