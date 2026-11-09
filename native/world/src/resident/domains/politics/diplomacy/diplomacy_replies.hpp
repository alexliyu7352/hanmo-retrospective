// 双方真实在场的明确外交回复；命令不能携带同意事实、任意谈判分数或生效标记。
#pragma once
#include "diplomacy_present.hpp"
namespace hanmo::resident {
struct DiplomaticReplyCommand {StableId offer_id,source_fact_id;Integer expected_revision=0,terms_revision=0;DiplomaticReplyKind decision=DiplomaticReplyKind::refuse;};
struct DiplomaticReplyQuote {StableId offer_id,grant_id;Integer offer_revision=0,terms_revision=0;ActingAuthority authority;DiplomaticReplyKind decision=DiplomaticReplyKind::refuse;FaceToFaceContactProof contact;};
Outcome<DiplomaticReplyQuote> preview_diplomatic_reply(const DiplomaticPresentRead&,const StableId& actor,const DiplomaticReplyCommand&);
Outcome<StableId> reply_diplomatic_offer_in_transaction(CommonStore&,DiplomacyState&,const DiplomaticPresentRead&,const StableId& actor,const DiplomaticReplyCommand&,const DiplomaticReplyQuote&);
}
