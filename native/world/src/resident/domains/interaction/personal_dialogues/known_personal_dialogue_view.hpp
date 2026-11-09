// 有限本人页，仅眼前可见成年人与显式模板；不读取或显示亲缘。
#pragma once
#include "known_personal_dialogue_boundary.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_known_personal_dialogues(const KnownPersonalDialogueContext&,Integer cursor=0);
}
