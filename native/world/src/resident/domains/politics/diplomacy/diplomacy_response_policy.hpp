// NPC回应只把作者明确组织指令转成原回复命令；不生成谈判分数、同意事实或通行权。
#pragma once
#include "resident/domains/politics/diplomacy/diplomacy_views.hpp"
#include "diplomacy_response_model.hpp"
namespace hanmo::resident {
struct DiplomaticResponseCandidate {
    StableId offer_id,source_fact_id,waiting_code;
    std::optional<DiplomaticReplyCommand> command;
    Integer inspected=0;
};
// 只解作者指令形状；具体Office与条款引用仍须实际目录校验。
Outcome<DiplomaticResponsePolicy> decode_diplomatic_response_policy(const BoundaryValue&);
BoundaryValue encode_diplomatic_response_policy(const DiplomaticResponsePolicy&);
Status validate_diplomatic_response_policy(const OrganizationCatalog&,const DiplomacyCatalog&,const DiplomaticResponsePolicy&);
Outcome<DiplomaticResponseCandidate> propose_diplomatic_response(const DiplomaticPresentRead&,const DiplomacyViewIndex&,const StableId& actor,const DiplomaticResponsePolicy&,Integer candidate_limit);
}
