// 职责：保存原 Session 定义五槽输入的唯一不可变目录。
// 不补 duration/RNG，不制造入场快照或另一份会话 current。
#pragma once

#include "dynamic_session.hpp"

namespace hanmo::resident {
struct BoundaryValue;

// 职责：用原 definition_lock_ref 定位原 DTO，不新造同义记录。
// 缺表与空表均不提供实际定义，真实使用必须找到作者给出的原行。
struct MilitarySessionDefinitionCatalog {
  OrderedTable<DynamicMilitarySessionDefinitionRead> definitions;
};

// 职责：在真实冷读任务中解唯一 object 键表，逐行调用原五槽规则。
// 错形状、重复键、错锁 id、未知或缺字段均保原严格失败。
Outcome<MilitarySessionDefinitionCatalog>
decode_military_session_definition_catalog(const BoundaryValue &);

// 职责：用同一有限字段序输出原五槽作者记录，不编码 runtime。
// 保持原 HalfDayDuration 精确 fraction 和可选 hold，不作轮数换算。
Outcome<BoundaryValue> encode_military_session_definition(
    const DynamicMilitarySessionDefinitionRead &);

// 职责：核 key 与原 definition_lock_ref 一致，再复用原 Session 校验。
// 合法空表保持空，不由当前会话 deadline 反推作者定义。
Status validate_military_session_definition_catalog(
    const MilitarySessionDefinitionCatalog &, ExactArithmeticBudget = {});

// 职责：借原 immutable 行并核其原约束，指针不得超出 owner 生命周期。
// 未知锁明确失败，不猜默认目标、duration 或 RNG backend。
Outcome<const DynamicMilitarySessionDefinitionRead *>
read_military_session_definition(const MilitarySessionDefinitionCatalog &,
                                 const StableId &definition_lock_ref,
                                 ExactArithmeticBudget = {});
} // namespace hanmo::resident
