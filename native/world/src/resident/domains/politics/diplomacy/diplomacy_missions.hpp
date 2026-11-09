// 具名使者责任签发和历史读；当前首片显式签发本人责任，任务受领仍须另一次本人命令。
#pragma once
#include "diplomacy_present.hpp"
namespace hanmo::resident {
class TaskEnvoyContactRead;
struct DiplomaticMissionCommand {StableId offer_id,source_fact_id;Integer expected_revision=0;};
Outcome<DiplomaticMissionApproval> preview_diplomatic_mission(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,const DiplomacyState&,const StableId& actor,const DiplomaticMissionCommand&);
Outcome<StableId> authorize_diplomatic_mission_in_transaction(CommonStore&,const OrganizationState&,const OrganizationCatalog&,const DiplomacyCatalog&,DiplomacyState&,const StableId& actor,const DiplomaticMissionCommand&);
// 已接受任务到场时重新核原政治条款与当前代表；不会复用只在发令治所成立的受领口。
Status preview_diplomatic_mission_contact(const DiplomaticPresentRead&,const StableId& authority_fact_id,const StableId& envoy_actor_id,const FaceToFaceContactProof&);
// 仅原actions_step候选在S3真正交接动作完工/release前调用；不接受玩家自报Proof。
// 阶段调用消费此typed结果；正常失权/重新会面不作为整个世界候选的硬失败。
Outcome<DiplomaticTaskPresentationAttempt> attempt_diplomatic_envoy_task_in_transaction(CommonStore&,DiplomacyState&,const DiplomaticPresentRead&,const TaskEnvoyContactRead&,const StableId& task_id,const StableId& action_id,const DynamicStepInput&);
Outcome<StableId> present_diplomatic_envoy_task_in_transaction(CommonStore&,DiplomacyState&,const DiplomaticPresentRead&,const TaskEnvoyContactRead&,const StableId& task_id,const StableId& action_id,const DynamicStepInput&);
// 所有读只借同一世界的实际状态。历史来源不重开接受窗，也不自动开始Task/行动。
class DiplomaticMissionAccess final:public DiplomaticMissionRead,public DiplomacyPresentationRead {
public:
    DiplomaticMissionAccess(const CommonStore& c,const OrganizationState& o,const OrganizationCatalog& oc,const DiplomacyCatalog& dc,const DiplomacyState& s):common_(c),organizations_(o),organization_catalog_(oc),catalog_(dc),state_(s){}
    // 只给同一次捕获的单半日内部读取到to；普通构造仍只能读已提交世界钟。
    DiplomaticMissionAccess(const CommonStore& c,const OrganizationState& o,const OrganizationCatalog& oc,const DiplomacyCatalog& dc,const DiplomacyState& s,const DynamicStepInput& step):common_(c),organizations_(o),organization_catalog_(oc),catalog_(dc),state_(s),phase_(step),phase_identity_(c.state().identity){}
    Outcome<DiplomaticMissionApproval> diplomatic_mission_for_acceptance(const StableId&,const StableId&)const override;
    Outcome<DiplomaticMissionApproval> diplomatic_mission_source(const StableId&)const override;
    Outcome<DiplomaticMissionContinuity> diplomatic_mission_continuity(const StableId&,const StableId&)const override;
    Outcome<DiplomaticPresentationSource> diplomatic_presentation_source(const StableId&)const override;
private:
    const CommonStore& common_;const OrganizationState& organizations_;const OrganizationCatalog& organization_catalog_;const DiplomacyCatalog& catalog_;const DiplomacyState& state_;
    std::optional<DynamicStepInput> phase_;WorldIdentity phase_identity_;
};
}
