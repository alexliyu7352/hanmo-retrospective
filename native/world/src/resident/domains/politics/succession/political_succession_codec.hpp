// 新继位段自持生前意愿与正式就任引用；旧Organization的任何序列化字段不变。
#pragma once
#include "political_succession_nominations.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* political_succession_answer_command="political_answer_succession";
inline constexpr const char* political_succession_accede_command="political_accede_succession";
struct PoliticalSuccessionState {Integer version=1;PoliticalSuccessionNominationState nominations;PoliticalSuccessionAccessionState accessions;};
using PoliticalSuccessionCommand=std::variant<PoliticalNominateSuccessor,PoliticalAnswerSuccession,PoliticalWithdrawSuccession,PoliticalAccedeSuccession>;
BoundaryValue encode_political_succession_state(const PoliticalSuccessionState&);
Outcome<PoliticalSuccessionState> decode_political_succession_state(const BoundaryValue&);
Status visit_political_succession_checkpoint(CodecVisitor&,PoliticalSuccessionState&);
StableId political_succession_command_kind(const PoliticalSuccessionCommand&);
bool is_political_succession_command(const StableId&);
BoundaryValue encode_political_succession_command(const PoliticalSuccessionCommand&);
Outcome<PoliticalSuccessionCommand> decode_political_succession_command(const StableId&,const BoundaryValue&);
}
