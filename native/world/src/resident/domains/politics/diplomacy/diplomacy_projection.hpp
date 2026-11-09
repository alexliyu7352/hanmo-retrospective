// 外交事实投影只公开原见证者与实际历史，不因存在目标组织而向全体成员泄露提议。
#pragma once
#include "diplomacy_offers.hpp"
namespace hanmo::resident {
std::optional<DynamicFactView> diplomacy_dynamic_fact(const DiplomacyState&,const StableId& fact_id);
}
