#include "group_movement.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Outcome<ActualGroupCrossingPositiveSpan> read_actual_group_crossing_first_positive_span(
 const CommonStore& common,const SpatialCarrierState& carriers,const MovementExactState& movement,
 const GroupMovementExecutionState& executions,const TravelCatalog& catalog,
 const BoundaryCrossingReceipt& crossing,ExactArithmeticBudget budget){return movement_detail::guard<ActualGroupCrossingPositiveSpan>([&]{
 using namespace movement_detail;
 if(carriers.crossings.find(crossing.receipt_id)!=&crossing)fail("BorrowedGroupCrossingRequired",crossing.receipt_id);
 const auto* head=common.state().facts.find(crossing.receipt_id);
 if(!head||head->domain!="travel"||head->kind!="boundary_crossing")fail("GroupCrossingSourceHeadRequired",crossing.receipt_id);
 take(validate_group_boundary_crossing_source(common,executions,carriers,movement,catalog,crossing,budget));
 const auto* execution=std::get_if<GroupMovementExecution>(&crossing.execution);
 if(!execution)fail("GroupCrossingExecutionRequired",crossing.receipt_id);
 const MovementUnitState* unit=nullptr;
 for(const auto& row:movement.units.rows())if(execution_key(row.second.execution)==execution_key(crossing.execution)&&row.second.ordinal==crossing.entry_ordinal-1){if(unit)fail("AmbiguousGroupCrossingUnit",crossing.receipt_id);unit=&row.second;}
 if(!unit||unit->activity_spans.empty())fail("GroupCrossingPositiveActivityRequired",crossing.receipt_id);
 const GroupTransportExpenseSource* original=nullptr;
 for(const auto& row:executions.expenses.rows())if(row.second.unit_ref==unit->id){if(original)fail("DuplicateGroupExpense",unit->id);original=&row.second;}
 const auto* cause=original?std::get_if<GroupExpensePrefixCause>(&original->actual_cause):nullptr;
 if(!original||!cause||original->journey_id!=execution->journey_id||!same_moment(original->occurred_at,crossing.entry_at,budget)||!same_moment(cause->first_actual_span.from,crossing.entry_at,budget))fail("GroupCrossingFirstSpanSourceRequired",crossing.receipt_id);
 const auto* expense_head=common.state().facts.find(original->id);
 if(!expense_head||expense_head->domain!="travel"||expense_head->kind!="group_transport_expense")fail("GroupExpenseSourceHeadRequired",original->id);
 take(make_moment_span(cause->first_actual_span.from,cause->first_actual_span.to,budget));
 const auto& applied=unit->activity_spans.front();
 if(take(compare_world_moments(cause->first_actual_span.from,cause->first_actual_span.to,budget))>=0||!same_moment(applied.span.from,cause->first_actual_span.from,budget)||take(compare_world_moments(cause->first_actual_span.to,applied.span.to,budget))>0||compare(applied.rate,{},budget)<=0||compare(applied.cursor_to,add(applied.cursor_from,mul(applied.rate,take(elapsed_world_duration(applied.span.from,applied.span.to,budget)).value,budget),budget),budget))fail("GroupCrossingFirstSpanNotApplied",unit->id);
 const auto* prefix=movement.prefixes.find(original->prefix_ref);
 if(!prefix||execution_key(prefix->execution)!=execution_key(crossing.execution))fail("GroupExpenseOriginalPrefixRequired",original->id);
 return ActualGroupCrossingPositiveSpan{{{"travel","group_transport_expense",original->id},1},unit->id,cause->first_actual_span};
});}
}
