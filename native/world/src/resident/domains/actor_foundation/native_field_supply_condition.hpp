// 同候选 Field Opening 原供粮写后绑定；不拥有另一份身体或供粮状态。
#pragma once
#include "resident/domains/military/military_actor_condition_write.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
class CompleteExactWorldBindingOwner;
Outcome<std::unique_ptr<MilitaryActorConditionWrite>>
make_native_field_supply_condition_writer(WorldBundle &,
                                          CompleteExactWorldBindingOwner &);
} // namespace hanmo::resident
