// 玩家主将仅提交本军唯一实际出口格；命令顺序/缓存/事务仍由原中央封口。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct FieldArmyExitSelect {
  StableId battle_id, army_id;
  Integer exit_cell = 0;
};
} // namespace hanmo::resident
