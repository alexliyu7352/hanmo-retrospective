// 胜方只在下一真实经济 Opening 续同军令；不新增旅程、位置或期间账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct ExactPhaseWriteContext;
// 必须处于原 Opening 候选/capture 内；任一军实际续行失败撤整候选。
Status
resume_native_field_winners_at_opening_in_candidate(WorldBundle &,
                                                    ExactPhaseWriteContext &);
} // namespace hanmo::resident
