// 原拜帖三表不改字节；本独立段只给真实业务源绑定本次实际提交结果，不复制帖文或会话。
#pragma once
#include "resident/storage/common_codec.hpp"
#include <string_view>
namespace hanmo::resident {
inline constexpr const char* visit_message_receipt_module="visit_message_receipts";
inline bool visit_message_receipt_version(Integer version){return version==1||version==2;}
// 玩家直接办理与原Director在Advance内办理分开；NPC仍须实际原claim及事实前控制来源。
struct VisitMessageResultBinding {
    StableId fact_id,producer,request_actor_id,result_id,claim_fact_id,intent_id,result_kind;
    Integer request_sequence=0,result_revision=0,from_tick=0;
};
// 本次真实前后快照产生的短期批次；从不进入checkpoint。
struct VisitMessageResultSources { std::vector<StableId> fact_ids;Integer from_tick=-1; };
struct VisitMessageReceiptState {
    Integer version=1;
    OrderedTable<VisitMessageResultBinding> bindings;
};
Status visit_visit_message_receipts_checkpoint(CodecVisitor&,VisitMessageReceiptState&);
BoundaryValue encode_visit_message_receipts(const VisitMessageReceiptState&);
Outcome<VisitMessageReceiptState> decode_visit_message_receipts(const BoundaryValue&);
class ProjectionWriter;
Status emit_visit_message_receipts_projection(ProjectionWriter&,std::string_view,const VisitMessageReceiptState&);
}
