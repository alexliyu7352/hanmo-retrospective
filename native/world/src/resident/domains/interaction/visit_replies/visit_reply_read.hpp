// 只有原发信人真实领回后才可读这份答复；源身份/tick分开，不把托回等同远方获知。
#pragma once
#include "visit_reply.hpp"
#include "visit_reply_receipts.hpp"
namespace hanmo::resident {
struct VisitReplyReadContext {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const VisitMessageState& messages;
    const VisitMessageCatalog& message_catalog;
    const InteractionCatalog& interactions;
    const VisitReplyCatalog& catalog;
    const VisitReplyState& state;
    const FamilyDeathOriginRead& deaths;
    const VisitReplyAuthorshipRead* authorship=nullptr;
    const VisitReplyLifeContext* life=nullptr;
    const VisitAuthoredCommandRead* actual_commands=nullptr;
};
struct CollectedVisitReplySource {
    WorldIdentity source_identity;
    StableId observer_actor_id,reply_fact_id,collection_fact_id,message_fact_id;
    StableId original_sender_actor_id,reply_author_actor_id,custodian_actor_id,collection_place_id,response;
    Integer replied_tick=0,received_tick=0;
};
Status validate_visit_reply_read(const VisitReplyReadContext&);
Outcome<std::optional<CollectedVisitReplySource>> read_collected_visit_reply(const VisitReplyReadContext&,const StableId& observer,const StableId& exact_reply_fact_id);
}
