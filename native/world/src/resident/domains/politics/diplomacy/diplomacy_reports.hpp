// 外交v2返报接缝；先核已发生递交与原责任，不沿过期的提议窗取消返报。
#pragma once
#include "diplomacy_missions.hpp"
#include "diplomacy_report_contract.hpp"
#include "diplomacy_v2_state.hpp"
namespace hanmo::resident {
class TaskEnvoyReportContactRead;
Outcome<DiplomaticMissionContinuity> diplomatic_report_responsibility(
    const DiplomaticPresentRead&, const StableId& authority_fact_id,
    const StableId& envoy_actor_id, const StableId& presentation_fact_id);
Status preview_diplomatic_report_contact(const DiplomaticPresentRead&,
    const StableId& authority_fact_id, const StableId& envoy_actor_id,
    const StableId& presentation_fact_id, const FaceToFaceContactProof&);
// 唯一报告写账；必须置于原报告Action减零、释放前的完整候选内。
Outcome<DiplomaticEnvoyReportAttempt> attempt_diplomatic_envoy_report_in_transaction(
    CommonStore&, DiplomacyV2State&, const DiplomaticPresentRead&,
    const TaskEnvoyReportContactRead&, const StableId& task_id, const StableId& action_id, const DynamicStepInput&);
class DiplomaticReportAccess final : public DiplomaticEnvoyReportRead {
public:
    DiplomaticReportAccess(const DiplomaticPresentRead& read, const DiplomacyV2State& reports)
        : read_{read.common,read.organizations,read.organization_catalog,read.catalog,read.state}, reports_(reports) {}
    DiplomaticReportAccess(const DiplomaticPresentRead& read, const DiplomacyV2State& reports, const DynamicStepInput& step)
        : read_{read.common,read.organizations,read.organization_catalog,read.catalog,read.state}, reports_(reports), phase_(step), phase_identity_(read.common.state().identity) {}
    DiplomaticReportAccess(const DiplomaticHistoryRead& read, const DiplomacyV2State& reports) : read_(read), reports_(reports) {}
    Outcome<DiplomaticEnvoyReportSource> diplomatic_envoy_report_source(const StableId&) const override;
private:
    DiplomaticHistoryRead read_;
    const DiplomacyV2State& reports_;
    std::optional<DynamicStepInput> phase_;
    WorldIdentity phase_identity_;
};
Status validate_diplomatic_report_state(const DiplomaticHistoryRead&, const DiplomacyV2State&);
inline Status validate_diplomatic_report_state(const DiplomaticPresentRead& r, const DiplomacyV2State& s) { return validate_diplomatic_report_state(DiplomaticHistoryRead{r.common,r.organizations,r.organization_catalog,r.catalog,r.state},s); }
}
