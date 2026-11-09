// 两个权源holder只在同候选提案调用栈存活；不保存临时reader指针。
#pragma once
#include "resident/domains/politics/licensing/political_shop_registry_intent_validation.hpp"
namespace hanmo::resident {
struct WorldBundle;
Status validate_political_shop_registry_director_world(const WorldBundle&);
Outcome<std::optional<DecisionProposal>> propose_political_shop_registry_director_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
}
