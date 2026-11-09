// 师承独立 World 边界：完整原人物/军事/课程先验之后才读关系，提交前绑定真实结果。
#pragma once
#include "apprenticeship_commands.hpp"
#include "apprenticeship_receipts.hpp"
namespace hanmo::resident {
class WorldBundle;
bool apprenticeship_command_kind(const StableId&);
Status validate_family_apprenticeship_world(const WorldBundle&);
Status bind_family_apprenticeship_result_world(WorldBundle&,const FamilyApprenticeshipState* before,const StableId& actor,Integer sequence,const CommitResult&);
}
