// 拜帖回复只记本人真实决定和原接待点的返交，不兼任预约、当前位置或第二消息库存。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_state.hpp"
namespace hanmo::resident {
inline constexpr const char* visit_reply_module="visit_message_replies";
inline constexpr const char* visit_reply_posted_kind="visit_message_reply_posted";
inline constexpr const char* visit_reply_collected_kind="visit_message_reply_collected";
// 首片两个写口只限受控本人；每个来源必须在真实Common结果生成后永久绑定。
struct VisitReplyPlayerReceipt {StableId result_id,request_actor_id;Integer request_sequence=0,result_revision=0;};
// 仅回复2持久化原NPC认领来源；回复1的visitor不得追加空字段。
struct VisitReplyAuthoredOrigin {StableId intent_id,reception_claim_fact_id;};
struct VisitReplyPosted {
    StableId fact_id,policy_id,message_fact_id,received_fact_id,response;
    VisitMessageContact contact;
    std::optional<VisitReplyPlayerReceipt> player_receipt;
    std::optional<VisitReplyAuthoredOrigin> authored_origin;
};
struct VisitReplyCollected {
    StableId fact_id,reply_fact_id;
    VisitMessageContact contact;
    std::optional<VisitReplyPlayerReceipt> player_receipt;
};
inline constexpr bool visit_reply_version(Integer v) noexcept{return v>=1&&v<=4;}
inline constexpr bool visit_reply_authored_version(Integer v) noexcept{return v==2||v==4;}
inline constexpr bool visit_reply_life_version(Integer v) noexcept{return v==3||v==4;}
enum class VisitReplyLifeRole {responder,requester,custodian};
struct VisitReplyLifeSource {DeathRef death_ref;StableId reply_fact_id;VisitReplyLifeRole role=VisitReplyLifeRole::responder;S3LeafLifeEffect effect;};
struct VisitReplyState {
    Integer version=1;
    OrderedTable<VisitReplyPosted> posted;
    OrderedTable<VisitReplyCollected> collected;
    // Fixed S3: /3 follows /1; /4 follows /2. These rows have no new Common fact.
    OrderedTable<VisitReplyLifeSource> life_sources;
    // 发信人只有实领后进入此索引；原收件人不能借索引得知远方回执已被领走。
    CheckpointMap<StableId,std::vector<StableId>> posted_by_author,collected_by_sender;
};
void rebuild_visit_reply_indexes(VisitReplyState&);
Status visit_visit_replies_checkpoint(CodecVisitor&,VisitReplyState&);
BoundaryValue encode_visit_replies(const VisitReplyState&);
Outcome<VisitReplyState> decode_visit_replies(const BoundaryValue&);
Outcome<BoundaryValue> encode_visit_reply_fact(const VisitReplyState&,const StableId& fact_id);
}
