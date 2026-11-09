// ST§3原两种实际接受；唯一执行键始终是原Growth action_id。
#pragma once
#include "study_lesson_capability_model.hpp"
#include "resident/kernel/work_execution.hpp"
#include "resident/domains/family/death_ref.hpp"
namespace hanmo::resident {
struct StudySkillStart {StableId action_id;};
struct TeachStudyRequestStart {StableId action_id,request_id,session_id;};
using StudyAcceptedExecutionRef=std::variant<StudySkillStart,TeachStudyRequestStart>;
struct StudyFactTermsRef {StableId action_id;};
struct StudyTimeBindingRef {StableId action_id;};
struct StudyServiceBindingV2 {
 StudyAcceptedExecutionRef accepted_execution_ref;StudyFactTermsRef terms_ref;
 WorldMoment accepted_at;LifeCatalogFieldRef provider_relation_rule_ref;StudyTimeBindingRef study_time_binding_ref;
};
// ST/v1b唯一原槽；永久能力分支在原capacity producer完成前仍显式不准入。
enum class StudyParticipantRole {teacher,student};
struct StudyVerifiedParticipantDeath {DeathRef death_ref;StableId actor_id;StudyParticipantRole role=StudyParticipantRole::teacher;};
struct UnsupportedStudyPermanentCapacitySource {};
using StudyStopCause=std::variant<StudyVerifiedParticipantDeath,UnsupportedStudyPermanentCapacitySource>;
struct StudyTerminal {StableId action_id;}; // 固定ActualServiceTermination槽，growth_study_service/1。
struct StudyOriginalParticipantUnavailable {StableId action_id;StudyFactTermsRef terms_ref;StableId actor_id;StudyParticipantRole role=StudyParticipantRole::teacher;StudyStopCause cause_ref;};
struct StudyChallengeRef {StableId action_id;};

using StudyGrowthContinuationRef=std::variant<StudyChallengeRef,StudyPerformanceAdjustmentRef>;
struct StudyLessonCharge {StableId action_id;Integer lesson_ordinal=0;StudyFactTermsRef accepted_terms_ref;};
// 原CommitResult后绑定投影；不是第二提交账，也不是tactic专用收据的类型别名。
struct StudyReleaseCommitRef {StableId result_id,actor_id,kind;Integer request_seq=0,revision=0;WorldMoment from,to;bool advance=false;};
struct StudyReservationReleaseResult {StableId wallet_id;ReservationRef reservation_ref;Integer amount_before=0,amount_after=0;StudyReleaseCommitRef original_release_commit_ref;};
struct UnsupportedStudyOriginalResponsibilityRef {};
struct StudyServiceTerminalV2 {
 StableId action_id;StudyAcceptedExecutionRef accepted_execution_ref;StudyFactTermsRef terms_ref;StudyTerminal terminal_ref;
 StudyStopCause stop_cause;WorldMoment terminated_at;std::vector<StudyLessonResultRef> provided_lesson_refs;
 StudyOriginalParticipantUnavailable continuity_check;std::vector<StudyGrowthContinuationRef> growth_continuation_refs;
 std::vector<StudyLessonCharge> fee_source_refs;std::vector<StudyReservationReleaseResult> reservation_release_results;
 std::vector<UnsupportedStudyOriginalResponsibilityRef> responsibility_refs;StudyTimeBindingRef study_time_binding_ref;
};
struct StudyRuleBinding {
 StableId action_id;LifeCatalogFieldRef study_rule_ref;ActorFoundationRuleRef condition_credit_rule_ref;Integer origin_reader_policy=2;
 std::optional<StudyAcceptedExecutionRef> accepted_execution_ref;WorkExecutionBinding execution_binding;
};
struct StudyScheduledGateRef {Integer lesson_ordinal=0;LifeCatalogFieldRef original_registered_gate_ref;};
struct StudyProvidedPeriodRef {Integer lesson_ordinal=0;StudyFixedLessonRef original_work_period_ref;StudyLessonResultRef original_lesson_result_ref;};
struct StudyPauseInterval {
 Integer pause_ordinal=0;SourceRef original_pause_source_ref;WorldMoment opened_at;
 std::optional<WorldMoment> closed_at;std::optional<SourceRef> closure_source_ref;
};
struct StudyTerminalTime {
 enum class Kind{normal_completed,self_directed_cancelled,ordinary_failure,actual_service_termination};Kind terminal_kind=Kind::normal_completed;
 WorldMoment at;SourceRef original_terminal_ref;
};
struct StudyTimeBinding {
 StableId action_id;WorkExecutionBinding execution_binding;WorldMoment start_at;
 std::vector<StudyScheduledGateRef> scheduled_gate_refs;std::vector<StudyProvidedPeriodRef> provided_period_refs;
 std::vector<StudyPauseInterval> pause_intervals;std::optional<StudyTerminalTime> terminal_time;
};
struct StudyConditionBoundary {
 enum class Kind{start,pause,resume,lesson_applied,service_terminal};
 StableId action_id;Integer boundary_ordinal=0;Kind boundary_kind=Kind::start;WorldMoment at;
 CharacterConditionSourceRef student_condition_ref;std::optional<CharacterConditionSourceRef> teacher_condition_ref;
 std::variant<SourceRef,StudyLessonResultRef> original_boundary_result_ref;
};
// Legacy/Group只在真正原producer完成时具象；同七根空表不意味着已支持其非空源。
struct UnsupportedLegacyStudyBinding {};
struct UnsupportedGroupTeacherCapabilitySource {};
struct UnsupportedGroupLessonCapabilitySource {};
struct StudyCapabilityState {
 OrderedTable<StudyRuleBinding> study_rule_bindings;
 OrderedTable<UnsupportedLegacyStudyBinding> legacy_study_bindings;
 OrderedTable<StudyTimeBinding> study_time_bindings;
 std::vector<LessonCapabilitySource> lesson_capability_sources;
 std::vector<StudyConditionBoundary> study_condition_boundaries;
 std::vector<UnsupportedGroupTeacherCapabilitySource> group_teacher_capability_sources;
 std::vector<UnsupportedGroupLessonCapabilitySource> group_lesson_capability_sources;
};

}
