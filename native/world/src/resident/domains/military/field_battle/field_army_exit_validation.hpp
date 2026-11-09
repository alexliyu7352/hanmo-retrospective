// 原Field整军退出唯一全表封口；单向source reader不替代生产/后果/采用反覆盖。
#pragma once
#include "field_army_exit_ready.hpp"
namespace hanmo::resident {
struct FieldGroupHandoffReadContext;
Status validate_field_army_exit_sources(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const FieldGroupHandoffReadContext &);
} // namespace hanmo::resident
