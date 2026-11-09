// 具名招聘只允许作者明确指定的本地职事、条款和候选；不是全城自动搜人。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct NamedRecruitmentPost {
    StableId office_id,terms_id;
    std::vector<StableId> candidate_actor_ids;
};
}
