// 每次读借用同一完整候选；草案签权不要求已经批准原供给责任。
#pragma once
#include "politics_supply_contract.hpp"
#include "resident/domains/politics/takeover/politics_takeover_rules.hpp"
namespace hanmo::resident {
// 作者只给本新pair与同接任Office声明捕获公库；当前与历史签权仍核实际产权链。
bool political_supply_grant_declares_resource(const CommonCatalog&,const OrganizationCatalog&,const PoliticsCatalog&,
    const MilitaryCatalog&,const PoliticalTakeoverCatalog&,bool enabled,const StableId& grant_id,const ResourceKey&);
class PoliticalSupplyPolicyAccess final:public PoliticalSupplyPolicyAuthorityRead {
public:
    PoliticalSupplyPolicyAccess(PoliticalTakeoverContext context,bool enabled):context_(context),enabled_(enabled){}
    Outcome<PoliticalSupplyPolicyAuthoritySource> political_supply_policy_authority(const ActingAuthority&,const PoliticalSupplyPolicyMandate&)const override;
    Outcome<PoliticalSupplyPolicyAuthoritySource> political_supply_policy_authority_before_issue(const ActingAuthority&,const PoliticalSupplyPolicyMandate&,const StableId& grant_id,const StableId& policy_issue_fact_id,Integer issued_tick)const override;
private:
    PoliticalTakeoverContext context_;
    bool enabled_=false;
};
}
