#pragma once
#include "military_duty_contract.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident::duty_detail {
SourceRef ref(const char*,const StableId&);
WorldMoment until(const MilitaryDutyTerms&,ExactArithmeticBudget);
const MilitaryDutyRule& rule(const MilitaryDutyContext&,const MilitaryDutyTerms&);
const MilitaryDutyTerms& terms(const MilitaryDutyState&,const StableId&);
bool same_refs(const std::vector<MomentOwnerSource>&,const std::vector<MomentOwnerSource>&);
bool same_optional(const std::optional<MomentOwnerSource>&,const std::optional<MomentOwnerSource>&);
bool same_qualification(const MilitaryDutyQualification&,const MilitaryDutyQualification&);
void validate_condition_consumption_current(const CommonStore&,const MilitaryDutyContext&,const MomentSpan&,const MilitaryDutyQualification&,const CommonActualFactPrefix&);
bool same_participants(const std::vector<MilitaryParticipationRef>&,const std::vector<MilitaryParticipationRef>&);
void append(std::vector<MomentOwnerSource>&,const std::vector<MomentOwnerSource>&);
void check_qualification(const MilitaryDutyTerms&,const MilitaryDutyQualification&);
bool qualified(const MilitaryDutyRule&,const MilitaryDutyQualification&);
void fold(MilitaryDutyWindowState&,const MilitaryDutyQualification&);
const MilitaryStationActionSource* station(const MilitaryDutyState&,const StableId&);
const MilitaryStationTerminalSource* terminal(const MilitaryDutyState&,const StableId&);
WorldMoment source_moment(const MilitaryDutyState&,const MilitaryDutyContext&,const MomentOwnerSource&);
}
