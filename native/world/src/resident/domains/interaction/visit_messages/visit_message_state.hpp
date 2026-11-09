// 留拜帖只保存真实托留与领读记录；没有第二任务、库存、关系或预约账。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/interaction/report_disclosure_types.hpp"
#include "resident/domains/interaction/s3_leaf_life.hpp"
#include <map>
#include <string_view>
namespace hanmo::resident {
inline constexpr const char* visit_message_posted_kind="visit_message_posted";
inline constexpr const char* visit_message_received_kind="visit_message_received";
inline constexpr const char* visit_message_withdrawn_kind="visit_message_withdrawn";
// 本人真实会面来源在当次提交核验，历史保原节点，不借今日任职者替换过去收件人。
struct VisitMessageContact {
    StableId actor_id,custodian_actor_id,place_id,role_id,session_id,dialogue_id,node_id;
    Integer tick=0;
};
// v1唯一内容是“本人来过，希望另行会面”，不允许绑定任意死亡/战争/秘密事实。
struct VisitMessagePosted {
    StableId fact_id,policy_id,sender_actor_id,recipient_actor_id;
    VisitMessageContact contact;
};
struct VisitMessageTerminal {
    StableId fact_id,message_fact_id;
    VisitMessageContact contact;
};
enum class VisitMessageLifeRole {sender,recipient,custodian};
struct VisitMessageLifeSource {DeathRef death_ref;StableId message_fact_id;VisitMessageLifeRole role=VisitMessageLifeRole::sender;S3LeafLifeEffect effect;};
// Fixed /2 adds original life_sources; MC1 /3 follows that complete prefix.
inline constexpr const char* visit_report_deposited_kind="visit_report_custody_deposited";
inline constexpr const char* visit_report_picked_up_kind="visit_report_custody_picked_up";
inline constexpr const char* visit_report_withdrawn_kind="visit_report_custody_withdrawn";
struct VisitReportCustodyDeposit {
 StableId fact_id,policy_id,report_id;VisitMessageContact contact;
 MomentOwnerSource contact_source;SourceRef sender_knowledge_source;
 ReportDisclosurePermission permission;WorldMoment deposited_at;
};
struct VisitReportCustodyTerminal {
 StableId fact_id,deposit_ref;VisitMessageContact contact;
 MomentOwnerSource contact_source;WorldMoment occurred_at;
};
struct VisitMessageState {
    Integer version=1;
    OrderedTable<VisitMessagePosted> posted;
    OrderedTable<VisitMessageTerminal> received,withdrawn;
    // 纯派生索引不编码；发信人不列入received，因此托留不等于得知远方送达。
    CheckpointMap<StableId,std::vector<StableId>> posted_by_sender,received_by_recipient;
    OrderedTable<VisitMessageLifeSource> life_sources;
    OrderedTable<VisitReportCustodyDeposit> report_deposits;
    OrderedTable<VisitReportCustodyTerminal> report_pickups,report_withdrawals;
};
Outcome<std::vector<StableId>> read_visit_message_append_delta(const VisitMessageState&,const VisitMessageState&);
void rebuild_visit_message_indexes(VisitMessageState&);
Status visit_visit_messages_checkpoint(CodecVisitor&,VisitMessageState&);
BoundaryValue encode_visit_messages(const VisitMessageState&);
class ProjectionWriter;
Status emit_visit_messages_projection(ProjectionWriter&,std::string_view,const VisitMessageState&);
Outcome<VisitMessageState> decode_visit_messages(const BoundaryValue&);
}
