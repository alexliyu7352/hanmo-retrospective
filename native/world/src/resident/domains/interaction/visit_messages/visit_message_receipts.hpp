// 显式新段的前后差分、实际结果绑定和全史冷验；旧visit_messages/1单独选择仍保旧解释。
#pragma once
#include "visit_message_receipt_state.hpp"
#include "visit_message_receipt_control.hpp"
#include "visit_reception_claims.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
// 外层权限仅由不可变Director目录选择；不得从checkpoint、业务actor或空delta猜测。
enum class VisitMessageSubmitterMode { unavailable,controlled_player,legacy_actor };
struct VisitMessageDirectorCommandSources {
 const DirectorState& state;const DirectorCatalog& catalog;const CatalogIdentity& identity;
};
struct VisitMessageReceiptRead {
    const VisitMessageState& messages;
    const VisitMessageCatalog& catalog;
    const VisitReceptionCatalog* reception=nullptr;
    const VisitReceptionState* claims=nullptr;
    VisitMessagePlayerHistoryRead player_before_fact;
    VisitMessageAdvanceSubmitterRead advance_submitter_before_fact;
    VisitMessageSubmitterMode submitter_mode=VisitMessageSubmitterMode::unavailable;
 const VisitMessageDirectorCommandSources* director_commands=nullptr;
 const CurrentMomentRead* actual_clock=nullptr;
 const InteractionCatalog* interactions=nullptr;const InteractionState* interaction_state=nullptr;
 const NativeActorSocialOccupancyRead* actual_occupancy=nullptr;
};
// 原表旧行与旧绑定须逐字保留；仅三种玩家命令或原六种真实半日外层可产新增，返回本次原事实序。
Outcome<std::vector<StableId>> visit_message_new_result_sources(
    const VisitMessageState& before,const VisitMessageState& after,
    const VisitMessageReceiptState& before_receipts,const VisitMessageReceiptState& after_receipts,
    const StableId& request_actor,const StableId& command_kind,const ReceiptDetails&,Integer tick,Integer actual_before_tick);
// 真实Common cache/history与result分配后，同候选一次绑定全部本次业务；不分配新ID或FactHeader。
// 任一步失败由外层原Interaction/Common保存点撤回；不得在业务writer完成前调用。
Status bind_visit_message_actual_result(CommonStore&,VisitMessageReceiptState&,const VisitMessageReceiptRead&,
    const std::vector<StableId>& source_fact_ids,const StableId& request_actor,Integer request_sequence,const CommitResult&,Integer actual_before_tick);
// The actual, newly appended original source is checked before seal/adoption.
// Historical effect validation instead requires the immutable receipt/adoption.
Outcome<MomentOwnerSource> read_visit_message_director_command_effect(const CommonStore&,const VisitMessageReceiptRead&,const StableId& actual_new_fact,const StableId& actor,const StableId& command,const ReceiptDetails&,const WorldMoment&);
Status validate_visit_message_director_actor_effect(const CommonStore&,const VisitMessageReceiptState&,const VisitMessageReceiptRead&,const StableId& actor,const SourceRef&);
// /2: only after the original seal and actual adoption append. The adoption is borrowed
// from the same Director state; its original command result supplies the exact moment.
Status bind_visit_message_director_actor_result(CommonStore&,VisitMessageReceiptState&,const VisitMessageReceiptRead&,const DirectorAdoptionResult2&);
// 全部三表逐源恰有一绑定。NPC原claim与事实前控制身份均重验，不借今天玩家或今日岗位证明旧史。
Status validate_visit_message_actual_results(const CommonStore&,const VisitMessageReceiptState&,const VisitMessageReceiptRead&);
Outcome<WorldMoment> read_visit_message_adopted_source_moment(const CommonStore&,const VisitMessageReceiptState&,const VisitMessageReceiptRead&,const SourceRef&,Integer source_schema);

}
