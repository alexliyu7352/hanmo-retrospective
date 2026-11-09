// 原两本人命令在唯一continuity身体中的有限分派；不接第二表或替代全World门。
#pragma once
#include "political_continuity_catalog.hpp"
#include "political_foundation_personal_commands.hpp"
namespace hanmo::resident {
// 预览只读；kind与variant必须逐项相符，不分配ID或把未来结果写入quote。
Status preview_political_foundation_personal_command(
    const CommonStore &, const PoliticalContinuityState &,
    const PoliticalContinuityCatalog &, const FoundationSupportContext &,
    const StableId &actual_actor, const StableId &kind,
    const FoundationPersonalCommand &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);
// 唯一原writer只写同一未发布候选。外层需重建实际来源、note_write_boundary，
// 然后完整World验证后唯一publish；任意失败必须丢弃整个候选，不只撤销本表。
Outcome<SourceRef> apply_political_foundation_personal_command_in_candidate(
    CommonStore &, PoliticalContinuityState &,
    const PoliticalContinuityCatalog &, const FoundationSupportContext &,
    const StableId &actual_actor, const StableId &kind,
    const FoundationPersonalCommand &, const ReadStamp &,
    const ReadStampAuthority &, const DomainReadVersionSource &);
} // namespace hanmo::resident
