#pragma once
#include "source_retention.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <string_view>
namespace hanmo::resident {
class CommonStore;
struct HistoryRecord;
struct CommonCommandSourceView {
    const CommitResult* result=nullptr;
    const HistoryRecord* history=nullptr;
    std::string_view cache_key;
    Integer request_seq=0;
};
// common/command_result schema1 names the original successful cached result.
// The unique corresponding original HistoryRecord remains part of that source.
Outcome<CommonCommandSourceView> read_common_command_source(const CommonStore&,const SourceRef&);
// The source kind is fixed by this original reader; borrowed IDs avoid a
// temporary owning SourceRef in source-local retention visitors.
Outcome<CommonCommandSourceView> read_common_command_source(const CommonStore&,std::string_view result_id);
// 完整候选内的原 Common 成功结果封口。玩家提交与 Director Actor 共用此口；
// 它只写原 revision/request/results/history，不发布世界、不结束外层捕获、不裁剪 PIN。
Outcome<std::shared_ptr<const CommitResult>> seal_complete_command_result_in_candidate(
    WorldBundle&,const StableId& actor,Integer request_seq,const StableId& kind,const ReceiptDetails&);
SourceRetentionRegistration make_common_command_source_registration();
Status validate_complete_command_source_pins(const WorldBundle&);
// Full selection retains all actual PIN-1 references plus the original recent
// unpinned cache windows. No new result payload or reference count is persisted.
Status prune_complete_command_sources(WorldBundle&);
}
