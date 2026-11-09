#include "resident/kernel/catalog_section_observation.hpp"
// 安民规则到分配计划在签发前闭合；不分配ID、预留资源或生成任何政策/任务事实。
#include "public_relief_catalog_links.hpp"
#include "public_relief_rules.hpp"
#include "resident/domains/politics/politics_governance.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 不依赖先前验证顺序，整数份额不能溢出或把负数抵扣成合法固定成本。
Status fixed_cost(const PublicReliefPolicy& plan,const AppeasementRule& rule){
    Integer cash=0,grain=0;
    if(plan.recipients.empty()||plan.recipients.size()>256)return common_failure("InvalidReliefRecipients",plan.id);
    for(const auto& recipient:plan.recipients){
        if(recipient.cash<0||recipient.grain<0||(!recipient.cash&&!recipient.grain)||cash>std::numeric_limits<Integer>::max()-recipient.cash||grain>std::numeric_limits<Integer>::max()-recipient.grain)return common_failure("ReliefQuantityOverflow",plan.id);
        cash+=recipient.cash;grain+=recipient.grain;
    }
    if(plan.item_id!=rule.grain_item_id||cash!=rule.cash_amount||grain!=rule.grain_amount)return common_failure("ReliefFixedCostMismatch",plan.id);
    return common_success();
}
// 从明确策略与正式规则查唯一作者责任，未知引用不补空计划或默认安民成本。
Outcome<const PublicReliefPolicy*> commission_plan(const PublicReliefCatalog& relief,const PoliticsGovernanceCatalog& governance,const StableId& policy_id){
    if(relief.version!=1||!governance.enabled)return {false,{},common_failure("PublicReliefGovernanceDependencyRequired").error};
    const auto* commission=governance.commission_policies.find(policy_id);const auto* rule=commission?governance.appeasement_rules.find(commission->rule_id):nullptr;
    if(!commission||commission->id!=policy_id||!rule||rule->id!=commission->rule_id)return {false,{},common_failure("InvalidGovernanceCommissionPolicy",policy_id).error};
    const auto* plan=relief.policies.find(commission->relief_policy_id);if(!plan||plan->id!=commission->relief_policy_id)return {false,{},common_failure("ReliefPolicySourceMismatch",commission->relief_policy_id).error};
    auto valid=fixed_cost(*plan,*rule);if(!valid.ok)return {false,{},valid.error};return {true,plan,{}};
}
}
Status validate_public_relief_catalog_links(const CommonStore& common,const LocalEconomyState& economy,const PublicReliefCatalog& relief,const PoliticsCatalog& politics,const PoliticsGovernanceCatalog& governance){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 75, "PublicReliefCatalog", "validate_public_relief_catalog_links", &relief);

    if(relief.version!=1||!politics.enabled||!governance.enabled||relief.policies.empty())return common_failure("PublicReliefGovernanceDependencyRequired");
    for(const auto& entry:relief.policies.rows()){
        const auto& plan=entry.second;bool found=false;
        if(plan.id!=entry.first||plan.site_id.empty()||plan.organization_id.empty())return common_failure("InvalidPublicReliefPolicy",entry.first);
        auto recipients=validate_public_relief_policy_recipients(common,economy,plan);if(!recipients.ok)return recipients;
        for(const auto& row:politics.jurisdictions.rows())if(row.second.seat_place_id==plan.site_id&&row.second.political_organization_id==plan.organization_id){found=true;break;}
        if(!found)return common_failure("ReliefJurisdictionSourceMismatch",plan.id);
    }
    for(const auto& row:governance.commission_policies.rows()){auto checked=commission_plan(relief,governance,row.first);if(!checked.ok)return {false,{},checked.error};}
    return common_success();
}
Status validate_public_relief_commission_plan(const CommonStore& common,const LocalEconomyState& economy,const PublicReliefCatalog& relief,const PoliticsState& politics,const PoliticsGovernanceCatalog& governance,const PoliticalGovernanceCommissionCommand& command){
    auto planned=commission_plan(relief,governance,command.policy_id);if(!planned.ok)return {false,{},planned.error};const auto& plan=*planned.value;
    const auto* jurisdiction=politics.jurisdictions.find(command.jurisdiction_id);
    if(!jurisdiction||jurisdiction->id!=command.jurisdiction_id||jurisdiction->seat_place_id!=plan.site_id||jurisdiction->political_organization_id!=plan.organization_id||jurisdiction->controlling_organization_id!=plan.organization_id||command.authority.organization_id!=plan.organization_id)return common_failure("ReliefJurisdictionSourceMismatch",plan.id);
    return validate_public_relief_policy_recipients(common,economy,plan);
}
}
