// 原宏观期间完成后的 NPC Field 零时窗口；全部后果仍属同一个未发布候选。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct ExactPhaseWriteContext;
// 只推进真实已开放的本窗；玩家窗口保持暂停，不发第二个 Advance 回执。
// 任一来源、身体或 S1 采用失败，外层必须丢弃整 World 候选。
Status settle_native_field_npc_windows_in_candidate(WorldBundle &,
                                                    ExactPhaseWriteContext &);
} // namespace hanmo::resident
