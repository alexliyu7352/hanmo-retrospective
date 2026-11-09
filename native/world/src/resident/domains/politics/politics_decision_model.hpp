// 政治事业的不可变策略值供Director目录引用；不携带世界、人物或组织可变状态。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
enum class PoliticalOfficeResponse { defer, accept_qualified, decline };
// 岗位与候选名单是作者给的有限机会，现任者与权限每次从真实Office复验。
struct PoliticalStaffingPost { StableId issuing_office_id,jurisdiction_id,appointment_policy_id; std::vector<StableId> candidate_actor_ids; };
// 本人明确政策和组织的成员响应不同；没有政策默认暂缓，不能自动视为同意。
struct PoliticalCareerPolicy {
    StableId id;
    PoliticalOfficeResponse response=PoliticalOfficeResponse::defer;
    // 每次新旅行报价的本人支出上限；不是公款拨付或整个委任的累计预算。
    Integer maximum_travel_quote_cost=0;
    std::vector<StableId> acceptable_organization_ids,acceptable_jurisdiction_ids;
    std::vector<PoliticalStaffingPost> staffing_posts;
};
}
