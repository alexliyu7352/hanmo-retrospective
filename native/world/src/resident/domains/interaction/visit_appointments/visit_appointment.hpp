// 原拜帖已实领后才可发出明确约期邀请；本人收回与本人接受仍是两个独立动作。
#pragma once
#include "visit_appointment_contract.hpp"
#include "resident/domains/family/actor_presence_read.hpp"
#include <functional>
namespace hanmo::resident {
using VisitAppointmentPlayerHistoryRead=std::function<Outcome<StableId>(Integer,const StableId&)>;
struct VisitAppointmentAuthorshipRead;struct VisitAppointmentMeetingState;struct VisitAppointmentLifeContext;class FamilyContinuationPeopleRead4;class CurrentMomentRead;struct VisitAuthoredCommandRead;
struct VisitAppointmentHistoricalSources {
    FamilyDeathOriginRead deaths;
    VisitAppointmentPlayerHistoryRead player_before_fact;
    const VisitAppointmentAuthorshipRead* authored=nullptr;
    const FamilyContinuationPeopleRead4* people=nullptr;
    const CurrentMomentRead* clock=nullptr;
    const VisitAppointmentLifeContext* life=nullptr;
    const VisitAuthoredCommandRead* actual_commands=nullptr;
};
struct VisitAppointmentQuote {
    WorldIdentity source_identity;
    VisitAppointmentCommand command;
    StableId actor_id,policy_id,message_fact_id,received_fact_id,invitation_fact_id,collection_fact_id;
    std::optional<AuthoredProviderSourceProof> declaration;
    VisitAppointmentTerms terms;
    std::optional<WorldMoment> actual_at; // quote only; original invitation remains Calendar.
};
// 原Common/人物/Birth/Legacy和拜帖须先完整核验；三源完成后再验独立会面及具名commitment_history晚门。
Status validate_visit_appointments(const CommonStore&,const CommonCatalog&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionCatalog&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentHistoricalSources&,const VisitReplyCatalog* ordinary=nullptr);
// Original immutable source leg, before life effects and meeting completion.
Status validate_visit_appointment_original_sources(const CommonStore&,const CommonCatalog&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionCatalog&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const VisitAppointmentHistoricalSources&,const VisitReplyCatalog* ordinary=nullptr);
Outcome<VisitAppointmentQuote> preview_visit_appointment(const CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,const VisitAppointmentState&,const StableId& actor,const VisitAppointmentCommand&,const VisitReplyCatalog* ordinary=nullptr,const VisitAppointmentMeetingState* meetings=nullptr,const VisitAppointmentLifeContext* life=nullptr);
// 整体候选末尾再绑定真实Common结果及做全验；中途失败由调用者撤回Common和整个Interaction。
Outcome<StableId> apply_visit_appointment_in_candidate(CommonStore&,const VisitMessageState&,const VisitMessageCatalog&,const PersonalAppointmentCatalog&,const PersonalAppointmentState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const VisitAppointmentCatalog&,VisitAppointmentState&,const StableId& actor,const VisitAppointmentCommand&,const VisitAppointmentQuote&,const VisitReplyCatalog* ordinary=nullptr,const VisitAppointmentMeetingState* meetings=nullptr,const VisitAppointmentLifeContext* life=nullptr);
StableId visit_appointment_command_kind(const VisitAppointmentCommand&);
BoundaryValue encode_visit_appointment_command(const VisitAppointmentCommand&);
Outcome<VisitAppointmentCommand> decode_visit_appointment_command(const StableId&,const BoundaryValue&);
}
