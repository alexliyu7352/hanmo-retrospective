// 此纯提案读只借同一候选；人物状态、Dialogue、拜帖和Director仍由原领域唯一写入。
#pragma once
#include "visit_reception_claims.hpp"
#include "visit_message.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct VisitReplyNpcRead;struct VisitAppointmentNpcRead;struct VisitMessageLifeContext;
struct VisitReceptionRead {
    const CommonStore& common;const InteractionState& interactions;const InteractionCatalog& interaction_catalog;
    const InteractionAccess& access;const VisitMessageState& messages;const VisitMessageCatalog& message_catalog;
    const VisitReceptionCatalog& catalog;const TaskState& tasks;const VisitReceptionState& claims;
    const VisitReplyNpcRead* replies=nullptr;const VisitAppointmentNpcRead* invitations=nullptr;
    const CurrentMomentRead* actual_clock=nullptr;const NativeActorSocialOccupancyRead* actual_occupancy=nullptr;
    const VisitMessageLifeContext* message_life=nullptr;
};
Outcome<std::optional<DecisionProposal>> propose_visit_reception(const VisitReceptionRead&,const DirectorState&,const KnownSubjectView&,const StableId& intent_id,const DecisionProposal&);
// 只有原计划已认领的同一会话可跨交谈占用；已受领任务/行动仍先行。
Outcome<bool> visit_reception_conversation_ready(const VisitReceptionRead&,const DirectorState&,const StableId& actor_id,const StableId& intent_id);
// 原Talk实际采用后，在原保存点内把本提案绑定刚创建的真实session；失败不遗留认领。
Outcome<DecisionProposal> bind_visit_reception_adopted(const VisitReceptionRead&,const StableId& intent_id,const DecisionProposal&);
Status validate_visit_reception_plan(const VisitReceptionRead&,const FinitePlan&,const StableId& intent_id);
}
