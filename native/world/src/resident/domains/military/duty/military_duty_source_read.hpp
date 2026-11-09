#pragma once
#include "military_duty_contract.hpp"
namespace hanmo::resident {
Outcome<Integer> read_military_duty_source_schema(const SourceRef&);
// The complete original Duty proof is checked before returning any moment.
// Arrival, binding and a zero-provided period never imply Work completion.
Outcome<WorldMoment> read_verified_military_duty_source_moment(
    const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,
    const MomentOwnerSource&);
}
