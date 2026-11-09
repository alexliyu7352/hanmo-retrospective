// 政治责任裁定只在完整候选事务中写账，晚失败必须撤回共同状态和各领域状态。
#pragma once
#include "politics_takeover_state.hpp"
#include "politics_takeover_phase.hpp"
#include "resident/domains/politics/politics_model.hpp"
#include "resident/domains/military/occupation_rules.hpp"
namespace hanmo::resident {
class PoliticalTakeoverCapture;
class OrganizationAuthorityClock;
struct OrganizationRuntime;
struct PoliticalTakeoverProductionWriteContext;
struct PoliticalSupplyAdoptionState;
class EconomicSupplyPendingPolicyRead;
struct OccupationAppliedSource;
// 枚举和单项均读真实占领发生时的原责任；不得用今日Owner补历史。
class PoliticalTakeoverOriginalRead {
public:
    virtual ~PoliticalTakeoverOriginalRead()=default;
    virtual Outcome<std::vector<PoliticalResponsibilityRef>> responsibility_keys(
        const MilitaryOccupationFact&) const=0;
    virtual Outcome<PoliticalOriginalResponsibility> original_responsibility(
        const MilitaryOccupationFact&,const PoliticalResponsibilityRef&) const=0;
};
struct PoliticalTakeoverContext {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const OrganizationState& organizations;
    const OrganizationCatalog& organization_catalog;
    const PoliticsCatalog& politics_catalog;
    const PoliticalTakeoverCatalog& catalog;
    const PoliticalTakeoverState& state;
    const MilitaryState& military;
    const MilitaryCatalog& military_catalog;
    const MilitaryAccess& military_access;
    const PoliticalTakeoverOriginalRead& originals;
    const DynamicStepInput* phase=nullptr;
    const PoliticalSupplyAdoptionState* supply_adoptions=nullptr;
    const EconomicSupplyPendingPolicyRead* supply_policies=nullptr;
    // /2受信当前M及唯一历史Basis；旧/1不使用这些尾接头。
    const OrganizationAuthorityClock* actual_clock=nullptr;
    const OrganizationRuntime* organization_runtime=nullptr;
    const PoliticsState* politics_state=nullptr;
    PoliticalTakeoverProductionWriteContext* production=nullptr;
};
// 捕获后只能紧接原S2实际apply，不能夹入新责任或实例分配。
Outcome<StableId> register_political_takeover_in_transaction(CommonStore&,
    PoliticalTakeoverState&,const PoliticalTakeoverCatalog&,const MilitaryState&,
    const PoliticalTakeoverCapture&,const OccupationAppliedSource&,const StableId& rule_id,
    const OrganizationAuthorityClock* actual_clock=nullptr);
// 尚未提交时钟时只认本次真实apply结果；普通历史读不放宽未来时刻。
Outcome<std::vector<PoliticalResponsibilityExecution>> read_political_pending_responsibilities_in_candidate(
    const CommonStore&,const PoliticalTakeoverState&,const PoliticalTakeoverCatalog&,const OccupationAppliedSource&,
    const StableId& registration_fact_id);
Outcome<StableId> preview_political_responsibility_decisions(const PoliticalTakeoverContext&,
    const StableId& command_actor,const PoliticalTakeoverDecisionCommand&);
Outcome<StableId> decide_political_responsibilities_in_transaction(CommonStore&,
    PoliticalTakeoverState&,const PoliticalTakeoverContext&,const StableId& command_actor,
    const PoliticalTakeoverDecisionCommand&,PoliticalSupplyAdoptionState* supply_adoptions=nullptr,
    OrganizationRuntime* organization_runtime=nullptr);
Status validate_political_takeover_state(const PoliticalTakeoverContext&);
// 历史批准和当前执行资格分开，失权不能抹掉原决定。
class PoliticalTakeoverAccess final:public PoliticalTakeoverRead {
public:
    explicit PoliticalTakeoverAccess(PoliticalTakeoverContext context):context_(context){}
    Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId&) const override;
    Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(
        const StableId&,const PoliticalResponsibilityRef&,const WorldIdentity&) const override;
    Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(
        const PoliticalResponsibilityRef&,const WorldIdentity&) const override;
    Outcome<std::optional<PoliticalResponsibilityExecution>> political_responsibility_before_fact(
        const PoliticalResponsibilityRef&,const StableId&,Integer,const WorldIdentity&) const override;
private:
    PoliticalTakeoverContext context_;
};
}
