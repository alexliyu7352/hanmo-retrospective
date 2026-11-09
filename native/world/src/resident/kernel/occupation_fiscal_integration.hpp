// 本调用栈持有新税政的所有只读权源；每次候选替换后重新构造。
#pragma once
#include "political_takeover_integration.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity.hpp"
#include "political_takeover_execution_world.hpp"
#include "resident/domains/local_economy/fiscal/occupation_fiscal_settlement.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_civil.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_command.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_profile.hpp"
namespace hanmo::resident {
// 旧Need只接旧账；新税期必须有同ID、同kind的实际账实体，未知事实仍交旧严格观察器。
inline Outcome<std::vector<StableId>> legacy_fiscal_need_sources(const OrderedTable<FactRecord>& facts,const OccupationFiscalSettlementState& state,const std::vector<StableId>& ids){
    std::vector<StableId> legacy;
    for(const auto& id:ids){
        const auto* fact=facts.find(id);
        const bool period=fact&&fact->kind==occupation_fiscal_period_kind;
        const bool assessment=fact&&fact->kind==occupation_fiscal_assessment_kind;
        const bool payment=fact&&fact->kind==occupation_fiscal_payment_kind;
        const bool suspended=fact&&fact->kind==occupation_fiscal_suspension_kind;
        if(!period&&!assessment&&!payment&&!suspended){legacy.push_back(id);continue;}
        const auto* p=state.periods.find(id);const auto* a=state.assessments.find(id);
        const auto* paid=state.payments.find(id);const auto* pause=state.suspended_periods.find(id);
        const int matches=int(p!=nullptr)+int(a!=nullptr)+int(paid!=nullptr)+int(pause!=nullptr);
        const bool valid=(period&&p&&p->id==id)||(assessment&&a&&a->assessment.id==id)||(payment&&paid&&paid->id==id)||(suspended&&pause&&pause->period.id==id);
        if(fact->id!=id||fact->domain!="local_economy"||matches!=1||!valid)return {false,{},common_failure("OccupationFiscalHistoryUnavailable",id).error};
    }
    return {true,std::move(legacy),{}};
}
class OccupationFiscalWorldAccess final {
public:
    static Outcome<std::unique_ptr<OccupationFiscalWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    const OccupationFiscalContext& context()const{return context_;}
    const OccupationFiscalAccess& policy()const{return policy_;}
    OccupationFiscalWorldAccess(const OccupationFiscalWorldAccess&)=delete;
    OccupationFiscalWorldAccess& operator=(const OccupationFiscalWorldAccess&)=delete;
private:
    OccupationFiscalWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverWorldAccess>);
    std::unique_ptr<PoliticalTakeoverWorldAccess> takeover_;
    OccupationFiscalContext context_;
    OccupationFiscalAccess policy_;
};
// 金额结算与民心读取复用同一World身份；所有借口由这个栈对象持有。
class OccupationFiscalSettlementWorldAccess final:public OccupationFiscalCivilRead {
public:
    static Outcome<std::unique_ptr<OccupationFiscalSettlementWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
    const OccupationFiscalSettlementContext& context()const{return context_;}
    Outcome<std::optional<OccupationFiscalCivilAssessment>> occupation_fiscal_civil_period(const StableId&)const override;
private:
    OccupationFiscalSettlementWorldAccess(const WorldBundle&,std::unique_ptr<PoliticalTakeoverExecutionWorldAccess>,std::unique_ptr<OccupationFiscalWorldAccess>,const DynamicStepInput*);
    const WorldBundle& world_;
    std::unique_ptr<PoliticalTakeoverExecutionWorldAccess> takeover_;
    std::unique_ptr<OccupationFiscalWorldAccess> policy_;
    OrganizationAccess organizations_;
    std::unique_ptr<FiscalPolicyAccess> legacy_;
    std::optional<FiscalTaxpayerContinuityContext> continuity_;
    OccupationFiscalSettlementContext context_;
};
Status initialize_occupation_fiscal_world(WorldBundle&);
Status validate_occupation_fiscal_world(const WorldBundle&);
Status require_legacy_fiscal_world(const WorldBundle&,const StableId& jurisdiction);
Outcome<OccupationFiscalQuote> prepare_occupation_fiscal_world(const WorldBundle&,const StableId& actor,const PoliticalSetOccupationTaxCommand&);
Outcome<PoliticalFiscalEffects> commit_occupation_fiscal_world(WorldBundle&,const StableId& actor,const PoliticalSetOccupationTaxCommand&,const OccupationFiscalQuote&);
Outcome<BoundaryValue> query_occupation_fiscal_settlement_world(const WorldBundle&,const BoundaryValue& args);
Outcome<BoundaryValue> query_occupation_fiscal_world(const WorldBundle&,const StableId& actor,const StableId& site);
}
