// One compiled military phase delegates the real original sub-leaves. This
// interface does not supply missing gameplay sources or select by row presence.
#pragma once
#include "duty/military_duty_phase.hpp"
#include "dynamic_military_phase.hpp"
#include "military_action_time_read.hpp"
#include "military_calendar_phase.hpp"
#include "naval_repair_exact_phase.hpp"
#include <memory>
namespace hanmo::resident {
struct ExactMilitaryStateValidationContext;
inline constexpr const char *military_exact_phase_profile_id =
    "military_original_exact_phases";
// An explicit phase_providers entry selects the calendar/Duty composite. The
// physical dynamic module is a prerequisite, not a substitute for that entry.
// Individual elapsed actions retain their own exact action_time_profiles.
struct MilitaryExactPhaseSelection {
  MilitaryActionTimeReadSelection actions;
  bool naval_repair = false;
  bool station = false;
  Integer dynamic_schema = 0;
  // 显式 Field1 原阶段，不因此选择旧六角动态根或 Duty。
  bool field_original = false;
};
Outcome<MilitaryExactPhaseSelection>
read_military_exact_phase_selection(const PreparedCatalog &);
std::vector<ActionTimeRegistration>
military_exact_action_time_registrations(const MilitaryExactPhaseSelection &);
// Every pointer addresses a genuinely selected original context in this same
// lease. Unselected contexts are absent; no empty World/state is constructed.
struct MilitaryExactReadBinding {
  const CommonStore &common;
  const MilitaryState &military;
  const MilitaryCatalog &catalog;
  const MilitaryAccess &access;
  const ActionElapsedStore &elapsed;
  const CurrentMomentRead &clock;
  const MilitaryActionTimeState *action_time = nullptr;
  const MilitaryExactState *dynamic = nullptr;
  const NavalState *naval = nullptr;
  const ExactOfficeReadContext *office = nullptr;
  const ExactFormationReadContext *formation = nullptr;
  const ExactTrainingReadContext *training = nullptr;
  const ExactReturnReadContext *returning = nullptr;
  const ExactResupplyReadContext *resupply = nullptr;
  const ExactNavalRepairReadContext *naval_repair = nullptr;
  const DynamicMilitaryVerifiedSourceContext *dynamic_sources = nullptr;
  const MilitaryDutyContext *duty = nullptr;
  ExactArithmeticBudget budget{};
  const ExactMilitaryStateValidationContext *field_supply = nullptr;
};
struct MilitaryExactWriteBinding {
  MilitaryExactReadBinding read;
  CommonStore &common;
  MilitaryState &military;
  ActionElapsedStore &elapsed;
  MilitaryCalendarWriteBinding &calendar;
  MilitaryActionTimeState *action_time = nullptr;
  MilitaryExactState *dynamic = nullptr;
  NavalState *naval = nullptr;
  const ExactOfficeWriteContext *office = nullptr;
  const ExactFormationWriteContext *formation = nullptr;
  const ExactTrainingWriteContext *training = nullptr;
  const ExactReturnWriteContext *returning = nullptr;
  const ExactResupplyWriteContext *resupply = nullptr;
  const ExactNavalRepairWriteContext *naval_repair = nullptr;
  const DynamicMilitaryGroupWriteContext *dynamic_sources = nullptr;
};
class MilitaryExactReadLease {
public:
  virtual ~MilitaryExactReadLease() = default;
  virtual MilitaryExactReadBinding bindings() const = 0;
};
class MilitaryExactWriteLease {
public:
  virtual ~MilitaryExactWriteLease() = default;
  virtual MilitaryExactWriteBinding bindings() = 0;
};
struct MilitaryExactWorldFactory {
  Outcome<std::shared_ptr<const MilitaryExactReadLease>> (*read)(
      const WorldBundle &, const WorldMoment &) = nullptr;
  Outcome<std::shared_ptr<MilitaryExactWriteLease>> (*write)(
      WorldBundle &, const WorldMoment &, ExactPhaseWriteContext &) = nullptr;
};
MilitaryExactWorldFactory native_military_exact_world_factory() noexcept;
ExactPhaseRegistration
    make_military_exact_phase_registration(MilitaryExactWorldFactory,
                                           MilitaryExactPhaseSelection);
} // namespace hanmo::resident
