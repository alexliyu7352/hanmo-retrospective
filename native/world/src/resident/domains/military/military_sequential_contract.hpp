// 显式顺序轮只改变原M14逐轮算法，不新增第二名册、行动账或随机流。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitarySequentialRoundRule { StableId profile="entry_initiative_05_v1"; };
struct MilitarySequentialTurn { StableId unit_id,commander_actor_id;Integer initiative=0,rotation_rank=0; };
}
