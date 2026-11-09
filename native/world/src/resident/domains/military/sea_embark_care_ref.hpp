// S2 原登船照护 binding 的强型地址；不表示绑定存在，不代替原 S2 严格读取者。
#pragma once
#include "hanmo/resident/model_contract.hpp"

namespace hanmo::resident {
// FH §5.2 明定的 SeaEmbarkCareBindingRef/1，仅定位原 execution 的真实 binding。
struct SeaEmbarkCareBindingRef {
  StableId transfer_action_id;
  StableId binding_id;
};
} // namespace hanmo::resident
