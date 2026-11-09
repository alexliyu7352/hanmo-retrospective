// 同一税政报价和真实来源回执闭合；S5结算不通过此叶转钱粮。
#include "politics_fiscal_integration.hpp"
#include "command_types.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 完整选择逐项比较，不能在报价后把常赋改成加征或切换公共容器。
bool command_equal(const PoliticalSetTaxCommand& a,const PoliticalSetTaxCommand& b){return a.authority.organization_id==b.authority.organization_id&&a.authority.office_id==b.authority.office_id&&a.authority.authority_epoch==b.authority.authority_epoch&&a.authority.actor_id==b.authority.actor_id&&a.jurisdiction_id==b.jurisdiction_id&&a.public_inventory_id==b.public_inventory_id&&a.expected_jurisdiction_revision==b.expected_jurisdiction_revision&&a.expected_policy_revision==b.expected_policy_revision&&a.mode==b.mode;}
// 世界实例、执行代次、修订和tick须同时相同。
bool identity_equal(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
}
PoliticalFiscalReadContext PoliticalFiscalWriteContext::read()const{return {common,common_catalog,politics,fiscal,fiscal_catalog,organization,organization_catalog,activity};}
Outcome<PoliticalFiscalQuote> preview_political_fiscal_business(const PoliticalFiscalReadContext& w,const StableId& actor,const PoliticalSetTaxCommand& command){
    auto valid=preview_political_set_tax(w.common,w.politics,w.fiscal,w.fiscal_catalog,w.organization,w.organization_catalog,w.activity,actor,command);if(!valid.ok)return {false,{},valid.error};
    PoliticalFiscalQuote out;out.identity=w.common.state().identity;out.actor_id=actor;out.command=command;const auto* j=w.politics.jurisdictions.find(command.jurisdiction_id);
    for(const auto& resource:{ResourceKey{ResourceKey::Kind::actor,actor},ResourceKey{ResourceKey::Kind::wallet,j->public_wallet_id},ResourceKey{ResourceKey::Kind::inventory,command.public_inventory_id}})out.resource_versions.push_back(w.common.read_version(resource));
    return {true,std::move(out),{}};
}
Outcome<PoliticalFiscalEffects> apply_political_fiscal_business(PoliticalFiscalWriteContext& w,const StableId& actor,const PoliticalSetTaxCommand& command,const PoliticalFiscalQuote& quote){
    const auto stale=[](){return Outcome<PoliticalFiscalEffects>{false,{},common_failure("StalePoliticalFiscalQuote").error};};
    if(quote.actor_id!=actor||quote.duration!=0||!identity_equal(quote.identity,w.common.state().identity)||!command_equal(command,quote.command))return stale();
    auto current=preview_political_fiscal_business(w.read(),actor,command);if(!current.ok)return {false,{},current.error};
    if(current.value.resource_versions.size()!=quote.resource_versions.size())return stale();
    for(std::size_t n=0;n<quote.resource_versions.size();++n){const auto& a=quote.resource_versions[n];const auto& b=current.value.resource_versions[n];if(a.resource.kind!=b.resource.kind||a.resource.id!=b.resource.id||a.version!=b.version)return stale();}
    return apply_political_set_tax_in_transaction(w.common,w.politics,w.fiscal,w.fiscal_catalog,w.organization,w.organization_catalog,w.activity,actor,command);
}
// 回执必须绑定同一个实际签发者、时刻、来源和政策revision，不能借另一次合法决定。
Status validate_political_fiscal_receipt(const PoliticalFiscalReadContext& w,const StableId& actor,Integer tick,const PoliticalFiscalEffects& effects){
    const auto* record=w.fiscal.policies.find(effects.policy_fact_id);const auto* fact=w.common.state().facts.find(effects.policy_fact_id);
    if(!record||!fact||fact->domain!="politics"||fact->kind!="political_fiscal_policy_changed"||record->issuer_actor_id!=actor||record->snapshot.issued_tick!=tick||record->snapshot.policy_revision!=effects.policy_revision||record->snapshot.effective_from_tick!=effects.effective_from_tick)return common_failure("PoliticalFiscalReceiptMismatch",effects.policy_fact_id);
    return common_success();
}
// 中央getter仅提取实际ReceiptData；本叶检查来源、历史分配区间和缓存去重。
Status validate_political_fiscal_business(const PoliticalFiscalReadContext& w,bool enabled,const PoliticalFiscalReceiptReader& read){
    if(enabled!=w.fiscal_catalog.enabled)return common_failure("PoliticalFiscalProfileMismatch");
    auto state=validate_political_fiscal(w.common,w.common_catalog,w.politics,w.fiscal,w.fiscal_catalog,w.organization,w.organization_catalog);if(!state.ok)return state;
    return validate_political_fiscal_business_receipts(w,enabled,read);
}
Status validate_political_fiscal_business_receipts(const PoliticalFiscalReadContext& w,bool enabled,const PoliticalFiscalReceiptReader& read){
    if(enabled!=w.fiscal_catalog.enabled)return common_failure("PoliticalFiscalProfileMismatch");
    std::set<StableId> seen;
    for(const auto& cached:w.common.state().results.rows()){
        if(!cached.second)continue;const auto& result=*cached.second;const auto* effects=read?read(result):nullptr;
        if(!effects){if(result.kind=="political_set_tax")return common_failure("PoliticalFiscalReceiptMismatch",cached.first);continue;}
        if(!enabled||result.kind!="political_set_tax")return common_failure("PoliticalFiscalReceiptMismatch",cached.first);
        const auto split=cached.first.rfind(':');if(split==StableId::npos)return common_failure("PoliticalFiscalReceiptMismatch",cached.first);
        const auto actor=cached.first.substr(0,split);const auto sequence=organization_instance_sequence(cached.first,actor);const auto* last=w.common.state().request_sequences.find(actor);
        if(!sequence||!last||*sequence>*last)return common_failure("PoliticalFiscalReceiptMismatch",cached.first);
        const HistoryRecord* history=nullptr;const HistoryRecord* before=nullptr;const HistoryRecord* previous=nullptr;
        for(const auto& item:w.common.state().history){if(item.result_id==result.result_id){if(history)return common_failure("PoliticalFiscalReceiptMismatch",cached.first);history=&item;before=previous;}previous=&item;}
        if(!history||history->actor_id!=actor||history->kind!=result.kind||history->tick!=result.tick)return common_failure("PoliticalFiscalReceiptMismatch",cached.first);
        auto source=validate_political_fiscal_receipt(w,actor,result.tick,*effects);if(!source.ok)return source;
        const auto upper=organization_instance_sequence(result.result_id,"result");const auto lower=before?organization_instance_sequence(before->result_id,"result"):std::optional<Integer>{0};const auto policy=organization_instance_sequence(effects->policy_fact_id,"politics_fiscal");
        if(!upper||!lower||!policy||*policy<=*lower||*policy>=*upper||!seen.insert(effects->policy_fact_id).second)return common_failure("PoliticalFiscalReceiptAllocationMismatch",cached.first);
    }
    return common_success();
}
}
