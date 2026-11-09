// 本人亲递重验原S3当面接触；呈递只产生政治来源，不替任何一方表示接受。
#pragma once
#include "diplomacy_offers.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
namespace hanmo::resident {
struct DiplomaticPresentCommand {StableId offer_id,source_fact_id;Integer expected_revision=0,terms_revision=0;};
struct DiplomaticPresentQuote {StableId offer_id,representative_grant_id;Integer offer_revision=0,terms_revision=0;ActingAuthority representative;FaceToFaceContactProof contact;};
// Read只借当前同一世界和原互动；没有复制人物、会话或第二时钟。
struct DiplomaticPresentRead {
    const CommonStore& common;const OrganizationState& organizations;const OrganizationCatalog& organization_catalog;
    const DiplomacyCatalog& catalog;const DiplomacyState& state;
    const InteractionState& interaction;const InteractionCatalog& interaction_catalog;const InteractionAccess& interaction_access;
};
Outcome<DiplomaticPresentQuote> preview_diplomatic_presentation(const DiplomaticPresentRead&,const StableId& actor,const DiplomaticPresentCommand&);
// 调用者持全候选；实际写前还要重新核相同Proof和授权，不能把开始时的接触永久缓存。
Outcome<StableId> present_diplomatic_offer_in_transaction(CommonStore&,DiplomacyState&,const DiplomaticPresentRead&,const StableId& actor,const DiplomaticPresentCommand&,const DiplomaticPresentQuote&);
}
