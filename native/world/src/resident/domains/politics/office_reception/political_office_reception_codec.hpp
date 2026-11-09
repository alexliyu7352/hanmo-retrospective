// 独立接待段使用显式module版本，旧政治、Common Role和Office的访客布局不变。
#pragma once
#include "political_office_reception_rules.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_office_reception_state(const PoliticalOfficeReceptionState&);
Outcome<PoliticalOfficeReceptionState> decode_political_office_reception_state(const BoundaryValue&);
Status visit_political_office_reception_checkpoint(CodecVisitor&,PoliticalOfficeReceptionState&);
StableId political_office_reception_command_kind(const PoliticalOfficeReceptionCommand&);
bool is_political_office_reception_command(const StableId&);
BoundaryValue encode_political_office_reception_command(const PoliticalAssumeOfficeReception&);
Outcome<PoliticalAssumeOfficeReception> decode_political_office_reception_command(const BoundaryValue&);
Outcome<PoliticalOfficeReceptionCommand> decode_political_office_reception_command(const StableId& kind,const BoundaryValue&);
}
