// 有限私人会面只从当前真实provider声明成立；导航、留帖、任职与收信本身均不等于接受预约。
#pragma once
#include "personal_appointment_state.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct PersonalAppointmentTime2Context;
struct ActionElapsedStore;
struct PersonalAppointmentAccept {StableId policy_id;};
struct PersonalAppointmentCancel {StableId appointment_id;};
struct PersonalAppointmentMeet {StableId appointment_id;};
struct PersonalAppointmentQuote {
    AuthoredProviderSourceProof consent;
    StableId policy_id,appointment_id;
    Integer earliest_tick=0,latest_tick=0,duration_ticks=1;
    std::optional<WorldMoment> actual_quoted_at;
};
// 对外只给本人已经当面接受的固定约期，不授某人当前位置或未来占用的写权。
struct PersonalAppointmentCommitment {
    StableId appointment_id,actor_id,provider_actor_id,place_id;
    Integer earliest_tick=0,latest_tick=0,duration_ticks=1;
    StableId status;
};
struct PersonalAppointmentCommitments {
    WorldIdentity source_identity;
    StableId observer_actor_id;
    Integer from_tick=0,through_tick=0;
    std::vector<PersonalAppointmentCommitment> appointments;
};
const PersonalAppointmentPolicy* personal_appointment_policy(const PersonalAppointmentCatalog&,const StableId&);
Status validate_personal_appointment_catalog(const PersonalAppointmentCatalog&,const InteractionCatalog&);
Outcome<PersonalAppointmentState> make_initial_personal_appointment_state(const PersonalAppointmentCatalog&,const InteractionCatalog&);
Outcome<PersonalAppointmentCatalog> decode_personal_appointment_catalog(const BoundaryValue&,const InteractionCatalog&);
BoundaryValue encode_personal_appointment_catalog(const PersonalAppointmentCatalog&);
Status emit_personal_appointment_catalog_fields(ProjectionWriter&,const PersonalAppointmentCatalog&);
Outcome<Integer> decode_personal_appointment_profile_version(const BoundaryValue&,const std::vector<StableId>& capabilities);
Outcome<bool> decode_personal_appointment_profile(const BoundaryValue&,const std::vector<StableId>& capabilities);
Status validate_personal_appointment_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PersonalAppointmentCatalog*,const PersonalAppointmentState*);
// 正式JSON导出/导入对原共同头及活动参数逐值绑定，不把普通Task或会话当替代来源。
Outcome<BoundaryValue> personal_appointment_fact_payloads(const CommonStore&,const PersonalAppointmentState*);
Outcome<BoundaryValue> personal_appointment_action_payloads(const CommonStore&,const PersonalAppointmentState*);
Status validate_personal_appointment_json_payloads(const CommonStore&,const PersonalAppointmentState*,const BoundaryValue& source);
// 原始账完整冷验后，当前状态只由原事实和时钟派生，过期不会自动追加取消或虚构缺席原因。
Status validate_personal_appointments(const CommonStore&,const InteractionState&,const InteractionCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const DynamicStepInput* phase=nullptr,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentCommitments> read_personal_appointment_commitments(const CommonStore&,const PersonalAppointmentState&,const StableId& actor_id,Integer from_tick,Integer through_tick,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentQuote> preview_personal_appointment_accept(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const StableId&,const PersonalAppointmentAccept&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<StableId> accept_personal_appointment_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,PersonalAppointmentState&,const PersonalAppointmentQuote&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<PersonalAppointmentQuote> preview_personal_appointment_cancel(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const StableId&,const PersonalAppointmentCancel&,const PersonalAppointmentTime2Context* exact=nullptr);
Outcome<StableId> cancel_personal_appointment_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,PersonalAppointmentState&,const PersonalAppointmentQuote&,const PersonalAppointmentTime2Context* exact=nullptr);
// 真会面开始前仍须双方到原地点且当前同意，过早、错过、别的会话或Action占用均不能跳过。
Outcome<PersonalAppointmentQuote> preview_personal_appointment_meet(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const StableId&,const PersonalAppointmentMeet&,const PersonalAppointmentTime2Context* exact=nullptr);
// 同原Common原语取得双方Action，外层失败回滚Common与本段；不另建一份人物忙碌账。
Outcome<StableId> start_personal_appointment_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,PersonalAppointmentState&,const PersonalAppointmentQuote&,const PersonalAppointmentTime2Context* exact=nullptr,ActionElapsedStore* elapsed=nullptr);
// 原phase先把剩余半日减至0；完成后由统一release释放Common Action及双方占用。
struct PersonalAppointmentCompletion {StableId fact_id;bool success=false;StableId reason;};
Outcome<PersonalAppointmentCompletion> finish_personal_appointment_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PersonalAppointmentCatalog&,PersonalAppointmentState&,const ActionCoreView&,const DynamicStepInput&);
}
