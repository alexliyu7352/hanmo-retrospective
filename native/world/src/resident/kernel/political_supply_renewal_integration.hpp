// 自主续签只构造同候选短命读，再提交既有经济发行或完整政治裁定命令。
#pragma once
#include "resident/domains/politics/supply/politics_supply_renewal_validation.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status validate_political_supply_renewal_world(const WorldBundle&);
Outcome<std::optional<DecisionProposal>> propose_political_supply_renewal_world(const WorldBundle&,
    const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
}
