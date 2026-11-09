// 独立辞任段与三命令严格值边界；不改变原政治/Organization/首职继位visitor布局。
#pragma once
#include "political_office_resignation_rules.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
BoundaryValue encode_political_office_resignation_state(const PoliticalOfficeResignationState&);
Outcome<PoliticalOfficeResignationState> decode_political_office_resignation_state(const BoundaryValue&);
Status visit_political_office_resignation_checkpoint(CodecVisitor&,PoliticalOfficeResignationState&);
StableId political_office_resignation_command_kind(const PoliticalOfficeResignationCommand&);
bool is_political_office_resignation_command(const StableId&);
BoundaryValue encode_political_office_resignation_command(const PoliticalOfficeResignationCommand&);
Outcome<PoliticalOfficeResignationCommand> decode_political_office_resignation_command(const StableId&,const BoundaryValue&);
}
