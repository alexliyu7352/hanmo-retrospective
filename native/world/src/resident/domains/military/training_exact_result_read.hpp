#pragma once
#include "training_exact.hpp"
namespace hanmo::resident {
// All returned rows are borrowed from this candidate's original S2 table. The
// accepted authority, participants, period witnesses, fees and S0 service chain
// are re-read before any result is exposed. Completion may be success or the
// original cancellation/ineligibility/shortage result; elapsed alone is no result.
Outcome<const DynamicMilitaryTrainingFact2*> read_verified_exact_military_training_result(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactTrainingReadContext&,
 const SourceRef& actual_completed_source,Integer source_schema);

// Finite source dispatch, schema 2 only. military_training_completed is the
// actual original row's owned field, not a Common fact or an append-order cut.
// Other kinds below have their actual original Common headers. Consumers which
// need a causal cut must read the row's real lesson/cancellation/eligibility cause.
// military_training -> started_at; military_training_completed -> completed_at;
// military_training_lesson -> closed_at; military_training_participation -> to;
// military_training_period_witness -> intersection_to;
// military_training_drill_frozen -> frozen_at; military_training_cancelled -> completed_at.
Outcome<WorldMoment> read_verified_exact_military_training_source_moment(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactTrainingReadContext&,
 const SourceRef&,Integer source_schema);
}
