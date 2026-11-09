// 生前交接独立版本尾；旧死亡继位与Organization访客字节不随本模块追加字段。
#pragma once
#include "political_head_handover_rules.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_head_handover_state(const PoliticalHeadHandoverState&);
Outcome<PoliticalHeadHandoverState> decode_political_head_handover_state(const BoundaryValue&);
Status visit_political_head_handover_checkpoint(CodecVisitor&,PoliticalHeadHandoverState&);
StableId political_head_handover_command_kind(const PoliticalHeadHandoverCommand&);
bool is_political_head_handover_command(const StableId&);
BoundaryValue encode_political_head_handover_command(const PoliticalHeadHandoverCommand&);
Outcome<PoliticalHeadHandoverCommand> decode_political_head_handover_command(const StableId&,const BoundaryValue&);
}
