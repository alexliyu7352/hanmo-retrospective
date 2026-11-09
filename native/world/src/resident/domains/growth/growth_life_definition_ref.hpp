// 原 LC01 锁内定义地址。只有作者显式输入正版本才存在，不为旧行补版本1。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct GrowthLifeDefinitionRef1 {
  StableId id;
  Integer version = 0;
};
} // namespace hanmo::resident
