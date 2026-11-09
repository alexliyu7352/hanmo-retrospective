// 原身体冻结值的局部冷读接缝；不改变原字段表或全局codec头的依赖图。
#pragma once
#include "actor_foundation_codec.hpp"
namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
// 调用者必须借正在使用的同job预算；原decoder仍唯一拥有全部Frozen字段。
Outcome<FrozenFacilityCapabilityInputs>
decode_frozen_character_capability_inputs(const BoundaryValue &,
                                          ScopedCheckpointJsonDecode &);
} // namespace hanmo::resident
