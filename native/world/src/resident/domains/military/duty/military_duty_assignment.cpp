#include "military_duty_assignment.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {namespace {
using namespace movement_detail;
bool work_kind(const SourceRef& value){return value.domain=="tasks"&&(value.kind=="work_accepted"||value.kind=="work_reassignment_accepted");}
MilitaryDutyAssignmentAssociation association(const CommonStore& common,const MilitaryDutyState& state,const SourceRef& source,Integer schema,Integer selected){
 if(selected!=4)fail("MilitaryDutyAssignmentProfileRequired",source.id);
 if(schema!=1||source.domain!="military"||source.kind!="military_duty_assignment"||source.id.empty())fail("MilitaryDutyAssignmentSourceRequired",source.id);
 const auto* row=state.assignments.find(source.id);const auto* header=common.state().facts.find(source.id);
 if(!row||row->id!=source.id||!header||header->id!=source.id||header->domain!=source.domain||header->kind!=source.kind)fail("MilitaryDutyAssignmentHeaderMismatch",source.id);
 // /4 currently has one real route contract. An absent/malformed Work link
 // cannot turn that source into non-Work and bypass the mandatory consumer.
 if(row->duty_kind!=MilitaryDutyKind::patrol||!row->original_work_assignment||row->original_work_assignment->source_schema!=1||!work_kind(row->original_work_assignment->source_ref))fail("MilitaryDutyAssignmentWorkSourceRequired",source.id);
 const auto& original=row->original_work_assignment->source_ref;
 const auto& route=row->route_definition;
 if(route.work_acceptance_ref.domain!="tasks"||route.work_acceptance_ref.kind!="work_accepted"||route.work_acceptance_ref.id.empty()||route.choice_ref!=route.work_acceptance_ref.id)fail("MilitaryDutyAssignmentRouteIdentityMismatch",source.id);
 const auto stop=take(read_actual_common_fact_position(common,source));
 if(take(read_actual_common_fact_position(common,original))>=stop||take(read_actual_common_fact_position(common,route.work_acceptance_ref))>=stop)fail("MilitaryDutyAssignmentSourceOrderMismatch",source.id);
 if(original.kind=="work_accepted"&&!same_source_ref(original,route.work_acceptance_ref))fail("MilitaryDutyAssignmentOriginalPlanMismatch",source.id);
 return {row,header,&*row->original_work_assignment};
}
}
Outcome<MilitaryDutyAssignmentAssociation> read_military_duty_assignment_association(const CommonStore& common,const MilitaryDutyState& state,const SourceRef& source,Integer schema,Integer selected){return movement_detail::guard<MilitaryDutyAssignmentAssociation>([&]{return association(common,state,source,schema,selected);});}
Outcome<MilitaryDutyWorkAssignmentAssociation> read_military_duty_work_assignment_association(const CommonStore& common,const MilitaryDutyState& state,const SourceRef& source,Integer schema,Integer selected){return movement_detail::guard<MilitaryDutyWorkAssignmentAssociation>([&]{
 if(source.domain=="military"&&source.kind=="military_duty_assignment"){
  const auto actual=association(common,state,source,schema,selected);
  return MilitaryDutyWorkAssignmentAssociation{&actual.original_work_assignment->source_ref,actual.original_work_assignment->source_schema,actual.assignment,actual.source_header};
 }
 if(work_kind(source)){if(schema!=1)movement_detail::fail("MilitaryDutyWorkAssignmentSchemaMismatch",source.id);return MilitaryDutyWorkAssignmentAssociation{&source,schema,nullptr,nullptr};}
 return MilitaryDutyWorkAssignmentAssociation{};
});}
}
