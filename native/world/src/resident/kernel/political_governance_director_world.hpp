// World短栈借用原三域读Context；返回提案只带值与ID，不能保存临时业务借口。
#pragma once
#include "resident/domains/politics/governance_intents/political_governance_intent_validation.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<std::optional<DecisionProposal>> propose_political_governance_director_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_political_governance_director_world(const WorldBundle&);
}
