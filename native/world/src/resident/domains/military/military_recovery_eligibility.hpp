// 安全驻营资格来自原驻地、战史和已经消费的口粮，不把当前库存充裕当作已受供给。
#pragma once
#include "military_recovery_source.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct MilitaryRecoveryEligibility {
 StableId unit_id,garrison_id,organization_id,ownership_id,supply_fact_id;
 Integer tick=0;
 bool qualified=false;
 StableId pause_reason;
};
// 严格原半日阶段：供给已落账，恢复在围城/会战步进前；未安全或缺粮返回具名暂停。
Outcome<MilitaryRecoveryEligibility> query_military_recovery_eligibility(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryPolicy&,
 const StableId& unit_id,const StableId& supply_fact_id,const DynamicStepInput&,const PopulationMilitaryState* population=nullptr);
// 冷读只能取实际恢复头前的原名册；后来入围城、出征或换主不能回填旧资格。
Outcome<MilitaryRecoveryEligibility> query_military_recovery_eligibility_before(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryPolicy&,const MilitaryRecoveryFact&,const PopulationMilitaryState* population=nullptr);
// 人口原消费只可借同一单步；rosters_before适配也必须明确携带同phase。
Outcome<MilitaryRecoveryEligibility> query_military_recovery_eligibility_before(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryPolicy&,const MilitaryRecoveryFact&,const DynamicStepInput&,const PopulationMilitaryState* population=nullptr);
// 恢复头被删时仍可从真实已提交供给半日核资格；只读该供给头前军籍，不伪造恢复事实。
Outcome<MilitaryRecoveryEligibility> query_military_recovery_supplied_period(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const MilitaryRecoveryPolicy&,
 const StableId& unit_id,const StableId& supply_fact_id,const DynamicStepInput* phase=nullptr,
 const PopulationMilitaryState* population=nullptr);
Status validate_military_recovery_policy(const MilitaryRecoveryPolicy&,const MilitaryCatalog&);
}
