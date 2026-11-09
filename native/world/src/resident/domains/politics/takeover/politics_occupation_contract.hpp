// 整合窗口只派生真实已应用占领；不授征募权、不收税、不复制军队或钱粮。
#pragma once
#include "politics_takeover_contract.hpp"
namespace hanmo::resident {
struct PoliticalRecruitmentOccupation {
    WorldIdentity source_identity;
    StableId registration_fact_id,occupation_fact_id,jurisdiction_id,site_id,garrison_id,controlling_organization_id;
    PoliticalOccupationRuleSource occupation_rule;
    Integer occupation_tick=0,observed_tick=0,integration_until_tick=0;
    bool blocked=false;
};
class PoliticalOccupationRead {
public:
    virtual ~PoliticalOccupationRead()=default;
    // 仅当前同候选或已核单步；调用者不能传任意“未来时间”逃过整合期限。
    virtual Outcome<std::optional<PoliticalRecruitmentOccupation>> recruitment_occupation(const StableId& garrison_id)const=0;
    // 仅具名真实人口征募开始/完成事实之前的窗口；调用方仍核原成功行动/兵员载荷。
    virtual Outcome<std::optional<PoliticalRecruitmentOccupation>> recruitment_occupation_before(const StableId& garrison_id,Integer event_tick,const StableId& population_action_or_enrollment_fact_id)const=0;
};
}
