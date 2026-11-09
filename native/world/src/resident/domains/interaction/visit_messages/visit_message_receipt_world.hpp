// 三命令预览与提交同源守门，真实cache后绑定；晚冷核在完整控制链之后。
#pragma once
#include "visit_message_receipts.hpp"
#include "visit_message.hpp"
#include "visit_message_receipt_profile.hpp"
namespace hanmo::resident {
struct WorldBundle;struct CompleteExactWorldReadBindings;
Status validate_visit_message_receipt_world(const WorldBundle&,const CompleteExactWorldReadBindings* actual=nullptr);
// 原NPC若本半日刚发生控制切换，显式等下一半日；不写出本段无法证明的银行。
Status guard_visit_message_receipt_producer_world(const WorldBundle&,const StableId& actor,const VisitMessageCommand&,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<VisitMessageResultSources> visit_message_new_result_world(const WorldBundle& before,const WorldBundle& after,
    const StableId& request_actor,const StableId& actual_committed_kind,const ReceiptDetails&);
Status bind_visit_message_result_world(WorldBundle&,const VisitMessageResultSources& actual_new_sources,
    const StableId& request_actor,Integer request_sequence,const CommitResult&,const CompleteExactWorldReadBindings* actual=nullptr);
Outcome<MomentOwnerSource> read_visit_message_director_command_effect_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& command,const ReceiptDetails&,const CompleteExactWorldReadBindings* actual=nullptr);
Status validate_visit_message_director_actor_effect_world(const WorldBundle&,const StableId& actor,const SourceRef&,const CompleteExactWorldReadBindings* actual=nullptr);
Status bind_visit_message_director_actor_result_world(WorldBundle&,const DirectorAdoptionResult2&,const CompleteExactWorldReadBindings* actual=nullptr);
}
