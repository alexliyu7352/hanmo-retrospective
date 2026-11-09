// S1精确运动附段；原Action/群旅仍唯一执行，Common整数位置只作显式兼容投影。
#pragma once
#include "travel.hpp"
#include "local_move_model.hpp"
#include "transport_authority.hpp"
#include "spatial_carrier_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
// 成本、工量、时间分别持型，不允许把余预算当财富或已行距离。
struct ExactTerrainCost { CanonicalFraction value; };
struct OriginalMovementAnchor { Location location; SourceRef actual_source; };
struct RoadFractionalAnchor { StableId edge_id,origin; CanonicalFraction progress; SourceRef actual_source; };
struct TerrainContinuousAnchor { StableId map_id,origin,x_bits,y_bits; SourceRef actual_prefix_close_source; StableId geometry_profile_ref; };
using MovementAnchor=std::variant<OriginalMovementAnchor,RoadFractionalAnchor,TerrainContinuousAnchor>;
// 原执行引用保持种类；group不制造个人Action或第二军事旅程。
struct PersonalMovementExecution { StableId action_id,journey_id; Integer segment_ordinal=0; };
struct GroupMovementExecution { StableId journey_id; SpatialCarrierRef carrier; Integer segment_ordinal=0; };
struct CustodyExitMovementExecution { StableId escape_action_id,exit_terms_id; Integer segment_ordinal=0; };
struct ShoreAssaultMovementExecution { StableId landing_operation_id,crossing_action_id; Integer segment_ordinal=0; };
using MovementExecutionRef=std::variant<PersonalMovementExecution,GroupMovementExecution,CustodyExitMovementExecution,ShoreAssaultMovementExecution>;
// 不可变原几何输入，真实实际分支产pieces；不保存ReadVersion借票。
struct TerrainCostPiece { TerrainCostPieceKind kind=TerrainCostPieceKind::moving_cost; ContactSegment segment; CanonicalFraction cost_from,cost_to; };
enum class TerrainOriginalClosingPolicy { carry_at_unit_boundary,arrival_discard };
struct TerrainCostBasis {
    StableId basis_ref; TerrainTravelParams original_inputs; MovementAnchor start_anchor,end_anchor;
    ExactTerrainCost parameter_limit; std::vector<TerrainCostPiece> pieces;
    Integer actual_consumed_total=0,reached_points=0,original_point_count=0,original_end_credit=0;
    TerrainOriginalClosingPolicy original_closing_policy=TerrainOriginalClosingPolicy::carry_at_unit_boundary;
    bool continued=false; StableId catalog_rule_ref,original_unit_ref;
};
struct RoadMovementBasis { StableId basis_ref; RoadTravelMotion original_inputs; MovementAnchor start_anchor,end_anchor; CanonicalFraction exact_start_progress,exact_end_progress; bool continued=false; StableId catalog_rule_ref,original_unit_ref; };
using MovementBasis=std::variant<RoadMovementBasis,TerrainCostBasis>;
struct RunningMovementMap { WorldMoment world_from; CanonicalFraction cursor_at_from,rate,budget_rate; SourceRef rate_source; };
struct HeldMovementMap { CanonicalFraction held_cursor; SourceRef actual_hold_source; WorldMoment effective_at; CanonicalFraction resume_rate,resume_budget_rate; SourceRef resume_rate_source; };
using ActivityWorldMap=std::variant<RunningMovementMap,HeldMovementMap>;
struct MovementActive {};
struct MovementHeld { SourceRef original_reason_source; };
struct MovementClosed { SourceRef original_result_ref; MovementAnchor actual_anchor; };
using MovementLifecycle=std::variant<MovementActive,MovementHeld,MovementClosed>;
struct MovementPrefixState {
    StableId id; MovementExecutionRef execution; MovementBasis basis; CanonicalFraction cursor;
    ActivityWorldMap activity_world_map; WorldMoment last_settled_at; StableId unit_ref;
    MovementLifecycle lifecycle;
};
struct NoTerrainBudget {};
struct TerrainUnitBudget { ExactTerrainCost opening_credit,funded,spent,discarded; std::vector<SourceRef> disposition_sources; };
struct MovementPaymentNotConsumed {};
struct MovementPaymentConsumed { SourceRef actual_expense_source; StableId payer_wallet_id; Integer amount=0; };
struct MovementUnitOpen {};
struct MovementUnitFinalized { SourceRef original_close_source; };
struct MovementActivitySegment { MomentSpan span; CanonicalFraction cursor_from,cursor_to,rate,budget_rate; SourceRef rate_source; StableId basis_ref; };
struct MovementUnitState {
    StableId id; MovementExecutionRef execution; Integer ordinal=0; HalfDayDuration active_elapsed;
    std::variant<NoTerrainBudget,TerrainUnitBudget> budget;
    std::variant<MovementPaymentNotConsumed,MovementPaymentConsumed> payment;
    std::vector<SourceRef> effort_basis_refs; std::optional<SourceRef> naval_finalized_source;
    std::variant<MovementUnitOpen,MovementUnitFinalized> status; std::vector<MovementActivitySegment> activity_spans;
};
// 原unit真实关闭源；技术slice只改唯一cursor，不制造永久“完成”事实。
enum class MovementCloseKind { carry,arrival,contact,cancelled };
struct TerrainCreditDispositionSource { StableId id,prefix_id,unit_ref,basis_ref; MovementExecutionRef execution; CanonicalFraction cursor,consumed; ExactTerrainCost before_credit,after_credit,discarded; MovementCloseKind kind=MovementCloseKind::carry; SourceRef original_close_source; WorldMoment occurred_at; };
// 完整ContactHit元组冻结双方原锚/后缀及原未量化求交结果；时间不再重求几何。
struct ExactContactMotionWitness {StableId unit_ref,basis_ref;ActivityWorldMap original_mapping;};
enum class ExactContactGeometryProfile { original_integer,terrain_local_v1,road_rational_v1 };
struct ExactMovementContact { StableId id; ContactHit original_hit; WorldMoment window_from; HalfDayDuration window_duration; WorldMoment occurred_at; StableId first_prefix_id,second_prefix_id;std::optional<ExactContactMotionWitness> first_motion_source,second_motion_source;
// 新Road输入的唯一比例；存在时旧ContactHit整数比例必须为0/0未提供标记，不能被旧mapper读取。
std::optional<CanonicalFraction> exact_road_fraction; ExactContactGeometryProfile geometry_profile=ExactContactGeometryProfile::original_integer; };
// Independent single-prefix/static-Field boundary source. No fabricated
// second moving endpoint or second Army. Persisted only in the explicitly
// selected movement_boundary_contacts/1 extension, never the old section.
struct MovementBoundaryContact {
 StableId id,prefix_id;
 SourceRef original_field_source;
 StableId field_hash;
 MomentSpan prediction_span;
 CanonicalFraction entry_fraction;
 WorldMoment occurred_at;
 ExactContactMotionWitness motion_source;
 MovementAnchor actual_anchor;
};
// Independent source adopted only by the original S1 resume writer. The
// former contact, exact closed unit and Field terminal are immutable causes;
// current movement/position remains in the original prefix and writer binding.
struct MovementBoundaryContinuationSource {
 StableId id,prefix_id,closed_unit_ref,new_unit_ref;
 SourceRef boundary_contact,field_contact,terminal_segment;
 WorldMoment occurred_at;
 bool held=false;
};
struct WorldMovementWriter { StableId prefix_id; };
struct BattleMovementWriter { StableId session_id,handoff_id; Integer generation=0; };
struct StableMovementWriter { MovementAnchor anchor; };
struct CarriedMovementWriter { SpatialCarrierRef carrier_ref; };
struct SpatialWriterBinding { StableId subject_kind,subject_id; std::variant<WorldMovementWriter,BattleMovementWriter,StableMovementWriter,CarriedMovementWriter> writer; SourceRef binding_source; };
struct MovementArrivalSource { StableId id,prefix_id,unit_ref; MovementExecutionRef execution; WorldMoment occurred_at; MovementAnchor actual_anchor; StableId basis_ref; CanonicalFraction cursor; Integer reached_points=0,original_point_count=0; std::optional<StableId> destination_settlement_id; };
// 真实单位首次付款的不可变源，钱包前后证据不是可花余额。
struct MovementExpenseSource { StableId id,prefix_id,unit_ref,payer_wallet_id; MovementExecutionRef execution; WorldMoment occurred_at; Integer amount=0,balance_before=0,balance_after=0; StableId original_cost_rule_ref; };
// S4唯一身体writer消费完整真实用力来源；旧fatigue直接写腿不并跑。
struct TravelExertionSource { StableId id,prefix_id,unit_ref; MovementExecutionRef execution; MomentSpan actual_span; HalfDayDuration active_elapsed; Integer original_fatigue_per_unit=0; std::vector<StableId> actual_participants; SourceRef original_close_source; };
// 单位参与/原费用规则由开段真实Action和当前成员捕获，历史prefix续段不覆盖它。
struct TravelExertionBasis { StableId id,unit_ref; MovementExecutionRef execution; std::optional<ActionCoreView> original_action; WorldMoment opened_at; std::vector<StableId> actual_participants; Integer original_fatigue_per_unit=0; SourceRef original_rule_source; MovementBasis actual_basis; CanonicalFraction opened_cursor;
 // travel/5: only the unique first personal basis owns this actual acceptance.
 std::optional<MomentOwnerSource> accepted_funding_plan;
};
struct MovementUnitCloseSource { StableId id,prefix_id,unit_ref; MovementExecutionRef execution; MovementCloseKind kind=MovementCloseKind::carry; WorldMoment occurred_at; CanonicalFraction cursor; MovementAnchor actual_anchor; SourceRef actual_cause; };
struct MovementBasisTransitionSource {StableId id,prefix_id,unit_ref;MovementExecutionRef execution;WorldMoment occurred_at;StableId old_basis_ref;CanonicalFraction old_cursor;StableId new_basis_ref;MovementAnchor actual_anchor;SourceRef original_reason_source;};
enum class PersonalMovementTerminalKind { arrived,cancelled };
// 原行动实际释放的终局；到场事实独立保存，不将位姿到达当成别域履约。
struct PersonalMovementTerminalSource {StableId id,prefix_id;ActionCoreView actual_action;SourceRef opening_source,actual_close_source;WorldMoment occurred_at;PersonalMovementTerminalKind kind=PersonalMovementTerminalKind::arrived;};
struct MovementUnitBoundarySource {StableId id,prefix_id,unit_ref,basis_ref;MovementExecutionRef execution;WorldMoment occurred_at;CanonicalFraction cursor;};
struct MovementContactInstallationSource {StableId id,contact_ref;SourceRef actual_business_source;Integer business_source_schema=0;WorldMoment occurred_at;};
struct MovementContactContinuationSource {StableId id,prefix_id,contact_ref,closed_unit_ref,new_unit_ref;SourceRef actual_resolution_source;Integer resolution_schema=0;WorldMoment occurred_at;bool held=false;};
enum class PersonalMovementControlOperation {pause,resume,wait};
struct PersonalMovementControlSource {StableId id,actor_id,action_id,prefix_id,unit_ref,basis_ref;PersonalMovementControlOperation operation=PersonalMovementControlOperation::pause;WorldMoment occurred_at;CanonicalFraction cursor;SourceRef original_execution_source;};
// Minimal retained original step witness for PATROL-ARRIVAL/2. It borrows the
// actual unit history and immutable basis; it is not a second movement cursor.
struct PatrolFinalMotionSource {StableId id,prefix_id,unit_ref,basis_ref;MovementExecutionRef execution;WorldMoment occurred_at;CanonicalFraction cursor;MovementAnchor actual_anchor;std::optional<SourceRef> final_contact_source;};
struct PatrolCheckpointArrival2 {StableId source_fact_id,journey_id;SourceRef final_motion_source_ref;SpatialCarrierRef actor_or_group_ref;SourceRef checkpoint_ref;WorldMoment arrived_at;};
// C20.4 finite contribution removed at one actual entry. S12 retains usage.
struct FrozenPassageSegmentEnd {MovementAnchor actual_end;};
struct FrozenPassageSafeBoundary {MomentOwnerSource original_boundary;};
struct RemovedSurgeBarrier {bool blocks_entry=true;};
struct RemovedViolentCurrent {Integer speed_bp=0,extra_fatigue_units=0;};
struct PassageModifierAdoption {
 StableId adoption_id;MovementExecutionRef execution;StableId segment_ref,source_id;
 Integer source_revision=0;StableId effect_id;Integer effect_revision=0;
 StableId modifier_rule_version;WorldMoment admitted_at;
 std::variant<FrozenPassageSegmentEnd,FrozenPassageSafeBoundary> frozen_completion_boundary;
 std::variant<RemovedSurgeBarrier,RemovedViolentCurrent> actual_removed_contribution;
 MomentOwnerSource effect_use_ref;
};
struct MovementExactState {
    OrderedTable<LocalMoveSource> local_moves;
    OrderedTable<MovementBoundaryContinuationSource> boundary_continuations;
    OrderedTable<MovementBoundaryContact> boundary_contacts;
    OrderedTable<PassageModifierAdoption> passage_modifier_adoptions;
    OrderedTable<PatrolFinalMotionSource> patrol_final_motions;OrderedTable<PatrolCheckpointArrival2> patrol_checkpoint_arrivals;
    CargoHandlingState cargo_handling;
    OrderedTable<PersonalMovementControlSource> personal_controls;OrderedTable<MovementContactContinuationSource> contact_continuations;OrderedTable<MovementUnitBoundarySource> unit_boundaries;OrderedTable<MovementContactInstallationSource> contact_installations;
    OrderedTable<PersonalMovementTerminalSource> personal_terminals;
    OrderedTable<MovementPrefixState> prefixes; OrderedTable<MovementUnitState> units;
    OrderedTable<SpatialWriterBinding> spatial_bindings; OrderedTable<ExactMovementContact> contacts;
    OrderedTable<TerrainCreditDispositionSource> credit_dispositions; OrderedTable<MovementArrivalSource> arrivals;
    OrderedTable<MovementExpenseSource> expenses; OrderedTable<TravelExertionSource> exertions; OrderedTable<TravelExertionBasis> exertion_bases; OrderedTable<MovementUnitCloseSource> unit_closes;OrderedTable<MovementBasisTransitionSource> basis_transitions;
};
} // 所有表由原WorldBundle完整候选拥有，不独立publish或保存一份世界时钟。
