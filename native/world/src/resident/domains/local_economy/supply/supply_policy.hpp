// 供给草案与实际库存观察独立持久化；政治采用与采购履约分别引用本账，不复制钱货或合同。
#pragma once
#include "supply_policy_contract.hpp"
namespace hanmo::resident {
// 作者只声明新经营条件；来源地点/库存/物品从原供给登记读取，运行发行ID不由作者指定。
struct EconomicSupplyPolicyDefinition {
    StableId id,original_source_key,organization_id,template_id,terms_version="1",unit="unit";
    StableId funding_wallet_id,reward_wallet_id,accept_role,report_role;
    NeedMeasure measure=NeedMeasure::stock_gap;
    Integer target_amount=0,max_claim_quantity=0,purchase_budget_per_unit=0,reward_per_unit=0,deadline_ticks=0,valid_ticks=0;
    std::optional<Integer> reward_total;
    bool public_report=false;
};
struct EconomicSupplyPolicyCatalog {Integer version=1;OrderedTable<EconomicSupplyPolicyDefinition> policies;};
// grant与签发本人由S6历史权源复核；冻结条款不可随着作者当前政权或新草案改写。
struct EconomicSupplyPolicyRecord {
    StableId id,author_policy_id,registration_fact_id,original_source_key,original_source_fact_id,grant_id;
    ActingAuthority authority;
    NeedPolicy policy;
    Integer issued_tick=0,earliest_effective_tick=0,through_tick=0,target_amount=0;
    bool public_report=false;
};
struct EconomicSupplyPolicyObservation {StableId policy_fact_id;LocalSupplyObservation observation;};
struct EconomicSupplyPolicyState {
    Integer version=1,initialized_tick=0,last_observation_tick=0;
    OrderedTable<EconomicSupplyPolicyRecord> policies;
    OrderedTable<EconomicSupplyPolicyObservation> observations;
};
struct IssueEconomicSupplyPolicyCommand {ActingAuthority authority;StableId registration_fact_id,author_policy_id;};
// 纯本域前置读仅核原目录、原供给登记、本账与Common事实序，不回调S6整体audit。
struct EconomicSupplyPendingContext {
    const CommonStore& common;const CommonCatalog& common_catalog;
    const LocalEconomyState& economy;const LocalEconomyCatalog& economy_catalog;
    const EconomicSupplyPolicyCatalog& catalog;const EconomicSupplyPolicyState& state;
    const DynamicStepInput* phase=nullptr;
};
class EconomicSupplyPendingAccess final:public EconomicSupplyPendingPolicyRead {
public:
    explicit EconomicSupplyPendingAccess(EconomicSupplyPendingContext context):context_(context){}
    Outcome<std::optional<EconomicSupplyPolicyTermsSource>> economic_supply_policy_terms(const StableId&,const std::optional<StableId>& ={})const override;
    Outcome<std::optional<EconomicSupplyPolicyTermsSource>> economic_supply_pending_policy(const StableId&,const StableId&,const std::optional<StableId>& ={})const override;
private:EconomicSupplyPendingContext context_;
};
// 仅内部纯账/头完整性，不表示草案已获政治批准或可以发采购合同。
Status validate_economic_supply_policy_ledger(const EconomicSupplyPendingContext&);
Outcome<NeedPolicy> freeze_economic_supply_need_policy(const EconomicSupplyPolicyDefinition&,const LocalSupplySource&,const StableId& issued_fact_id);
Outcome<EconomicSupplyPolicyCatalog> decode_economic_supply_policy_catalog(const BoundaryValue&);
Outcome<EconomicSupplyPolicyState> decode_economic_supply_policy_state(const BoundaryValue&);
class ProjectionWriter;
Status emit_economic_supply_policy_state_fields(ProjectionWriter&,const EconomicSupplyPolicyState&);
BoundaryValue encode_economic_supply_policy_state(const EconomicSupplyPolicyState&);
Status visit_economic_supply_policy_checkpoint(CodecVisitor&,EconomicSupplyPolicyState&);
}
