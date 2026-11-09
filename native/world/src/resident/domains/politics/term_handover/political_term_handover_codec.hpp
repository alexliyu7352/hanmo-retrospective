// 生前交接独立版本尾；旧死亡继位与Organization访客字节不随本模块追加字段。
#pragma once
#include "political_term_handover_rules.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_term_handover_state(const PoliticalTermHandoverState&);
Outcome<PoliticalTermHandoverState> decode_political_term_handover_state(const BoundaryValue&);
Status visit_political_term_handover_checkpoint(CodecVisitor&,PoliticalTermHandoverState&);
StableId political_term_handover_command_kind(const PoliticalTermHandoverCommand&);
bool is_political_term_handover_command(const StableId&);
BoundaryValue encode_political_term_handover_command(const PoliticalTermHandoverCommand&);
Outcome<PoliticalTermHandoverCommand> decode_political_term_handover_command(const StableId&,const BoundaryValue&);
}
