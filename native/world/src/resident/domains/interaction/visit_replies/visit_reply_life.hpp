// Fixed S3 §4.2: only the original uncollected reply responsibility consumes Death4.
#pragma once
#include "visit_reply.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_life.hpp"
#include "resident/domains/family/family_continuation_people4.hpp"
namespace hanmo::resident {
class NativeActorSocialOccupancyRead;
struct VisitReplyLifeContext {
 const VisitMessageLifeContext& messages;
 const CommonCatalog& common_catalog;
 const VisitReplyCatalog& policies;
 const VisitReplyState& state;
 const FamilyContinuationPeopleRead4& people;
 const FamilyDeathOriginRead& legacy_deaths;
 const VisitReplyAuthorshipRead* authorship=nullptr;
 const NativeActorSocialOccupancyRead* occupancy=nullptr;
};
// This leg validates original Common headers, contacts, receipt and author identity,
// but does not re-enter this leaf's death consumption table.
Status validate_visit_reply_original_sources(const CommonStore&,const CommonCatalog&,const VisitMessageState&,const VisitMessageCatalog&,const InteractionCatalog&,const VisitReplyCatalog&,const VisitReplyState&,const FamilyDeathOriginRead&,const VisitReplyAuthorshipRead*,const VisitReplyLifeContext*);
Status validate_visit_reply_life_binding(const VisitReplyLifeContext&);
Status apply_visit_reply_death_in_candidate(CommonStore&,VisitReplyState&,const VisitReplyLifeContext&,const DeathRef&);
Outcome<const VisitReplyLifeSource*> read_visit_reply_life_source(const VisitReplyLifeContext&,const StableId& result_id);
Status validate_visit_reply_life_sources(const VisitReplyLifeContext&);
Status validate_visit_reply_life_command(const VisitReplyLifeContext&,const VisitReplyCommand&);
}
