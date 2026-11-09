// FSL原三个命令绑定字段，纯值声明不带Actor/Family/Interaction根。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct LifeActualCommandBinding {
  StableId command_kind;
  Integer request_seq = 0;
  StableId result_id;
};
} // namespace hanmo::resident
