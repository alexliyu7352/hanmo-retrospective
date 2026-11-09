// 具名窗口引用完整原接管读上下文，默认冷读不容许下一半日事实。
#pragma once
#include "politics_occupation_contract.hpp"
#include "politics_takeover_rules.hpp"
namespace hanmo::resident {
class PoliticalOccupationAccess final:public PoliticalOccupationRead {
public:
    explicit PoliticalOccupationAccess(PoliticalTakeoverContext context):context_(context){}
    Outcome<std::optional<PoliticalRecruitmentOccupation>> recruitment_occupation(const StableId&)const override;
    Outcome<std::optional<PoliticalRecruitmentOccupation>> recruitment_occupation_before(const StableId&,Integer,const StableId&)const override;
private:
    PoliticalTakeoverContext context_;
};
}
