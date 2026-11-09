// 作者策略的纯值形状；不携带人物、权限、资产或决策钟。
#pragma once
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
enum class DiplomaticResponseChoice { defer, accept, refuse };
struct DiplomaticResponseDirective {
    StableId mission_policy_id,terms_id;
    DiplomaticResponseChoice choice=DiplomaticResponseChoice::defer;
};
struct DiplomaticResponsePolicy {
    StableId id,office_id;
    std::vector<DiplomaticResponseDirective> directives;
};
}
