// 原 22 个已存事件编号的私有兼容名；未来子型在自身头声明，不扩此表。
#pragma once
#include "field_event.hpp"
namespace hanmo::resident::field_event_kind {
inline constexpr FieldBattleEventKind Contact{0};
inline constexpr FieldBattleEventKind Casualties{1};
inline constexpr FieldBattleEventKind Rout{2};
inline constexpr FieldBattleEventKind Rallied{3};
inline constexpr FieldBattleEventKind CommanderLost{4};
inline constexpr FieldBattleEventKind OrderArrived{5};
inline constexpr FieldBattleEventKind Entered{6};
inline constexpr FieldBattleEventKind Withdrawn{7};
inline constexpr FieldBattleEventKind Ended{8};
inline constexpr FieldBattleEventKind CouncilAdopted{9};
inline constexpr FieldBattleEventKind RequestGranted{10};
inline constexpr FieldBattleEventKind RequestDenied{11};
inline constexpr FieldBattleEventKind Tactic{12};
inline constexpr FieldBattleEventKind Stratagem{13};
inline constexpr FieldBattleEventKind Revealed{14};
inline constexpr FieldBattleEventKind MissionTransition{15};
inline constexpr FieldBattleEventKind VolleyImpact{16};
inline constexpr FieldBattleEventKind NavigationObserved{17};
inline constexpr FieldBattleEventKind MotionApplied{18};
inline constexpr FieldBattleEventKind MissionApplied{19};
inline constexpr FieldBattleEventKind ContactPhaseApplied{20};
inline constexpr FieldBattleEventKind OrderResolved{21};
}
