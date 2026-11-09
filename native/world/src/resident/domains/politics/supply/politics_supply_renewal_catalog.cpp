#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/politics/politics_projection_detail.hpp"
// 独立目录只保存作者意图，不增加Director检查点字段，也不把配置当作实际签权。
#include "politics_supply_renewal_catalog.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include <set>
namespace hanmo::resident::dynamic_economy_codec {
template<class A>void record(A& a,PoliticalSupplyRenewalIntent& v){a("id",v.id);a("takeover_rule_id",v.takeover_rule_id);a("author_policy_id",v.author_policy_id);}
template<class A>void record(A& a,PoliticalSupplyRenewalCatalog& v){a("version",v.version);a("intents",v.intents);}
}
namespace hanmo::resident {
namespace {
// 目录规模和ID在未连接其他模块前即可拒绝，直接BoundaryValue入口亦不接受重名键。
Status shape(const PoliticalSupplyRenewalCatalog& c){
    if(c.version!=1)return common_failure("UnsupportedPoliticalSupplyRenewalVersion");
    if(c.intents.empty()||c.intents.size()>64)return common_failure("PoliticalSupplyRenewalIntentLimit");
    for(const auto& row:c.intents.rows()){
        const auto& v=row.second;if(row.first!=v.id||v.id.empty()||v.id.size()>4096
            ||v.takeover_rule_id.empty()||v.takeover_rule_id.size()>4096||v.author_policy_id.empty()||v.author_policy_id.size()>4096)
            return common_failure("InvalidPoliticalSupplyRenewalIntent",row.first);
    }
    return common_success();
}
}
Outcome<PoliticalSupplyRenewalCatalog> decode_political_supply_renewals(const BoundaryValue& value){
    auto unique=validate_political_takeover_payload(value);if(!unique.ok)return {false,{},unique.error};
    try{PoliticalSupplyRenewalCatalog out;dynamic_economy_codec::decode(value,out,"/political_supply_renewals");auto valid=shape(out);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}
    catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
// 原作者目录同字段直接发射，值树与无树导出不再各维护一套序列。
Status emit_political_supply_renewals_fields(ProjectionWriter& writer,const PoliticalSupplyRenewalCatalog& value){
 try{return dynamic_economy_codec::emit_fields(writer,value);}
 catch(const boundary::DecodeFailure& e){return writer.reject(e.error.original.code,e.error.original.path);}
}
BoundaryValue encode_political_supply_renewals(const PoliticalSupplyRenewalCatalog& value){return politics_projection_detail::encode_fields(value,emit_political_supply_renewals_fields);}
Status validate_political_supply_renewals(const PoliticalSupplyRenewalCatalog& c,
    const PoliticalTakeoverCatalog& takeover,const OrganizationCatalog& organizations,const EconomicSupplyPolicyCatalog& economic){
    auto valid=shape(c);if(!valid.ok)return valid;
    if(!takeover.enabled||takeover.version!=1||economic.version!=1)return common_failure("PoliticalSupplyRenewalPairRequired");
    std::set<std::pair<StableId,StableId>> sources;
    for(const auto& row:c.intents.rows()){
        const auto& intent=row.second;const auto* rule=takeover.rules.find(intent.takeover_rule_id);
        const auto* policy=economic.policies.find(intent.author_policy_id);const auto* office=rule?organizations.offices.find(rule->successor_office_id):nullptr;
        if(!rule||!policy||!office||office->organization_id!=policy->organization_id)
            return common_failure("PoliticalSupplyRenewalIntentMismatch",row.first);
        if(!sources.emplace(rule->id,policy->original_source_key).second)
            return common_failure("DuplicatePoliticalSupplyRenewalSource",row.first);
    }
    return common_success();
}
}
