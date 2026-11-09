// 显式意图只扩原有序目录，不增公共事实或另存Task状态；完成计划仍须真实原来源。
#pragma once
#include "political_governance_director.hpp"
namespace hanmo::resident {
Status validate_political_governance_intent_modules(bool enabled,const std::vector<std::pair<StableId,StableId>>&);
Status validate_political_governance_intent_director(const DirectorCatalog&,const DirectorState&,const GovernanceReadContext*);
}
