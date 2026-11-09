#pragma once
#include "military_duty_contract.hpp"
namespace hanmo::resident {
// Retained association only, safe before callbacks into full Work/authority
// readers. It grants no military permission, supply commitment or qualification.
struct MilitaryDutyAssignmentAssociation {
    const MilitaryDutyAssignmentSource* assignment=nullptr;
    const FactRecord* source_header=nullptr;
    const MomentOwnerSource* original_work_assignment=nullptr;
};
Outcome<MilitaryDutyAssignmentAssociation> read_military_duty_assignment_association(
    const CommonStore&,const MilitaryDutyState&,const SourceRef&,Integer source_schema,Integer selected_dynamic_military);
struct MilitaryDutyWorkAssignmentAssociation {
    const SourceRef* source=nullptr;Integer source_schema=0;
    const MilitaryDutyAssignmentSource* assignment=nullptr;
    const FactRecord* assignment_header=nullptr;
};
// Legacy direct tasks sources retain their original input reference. The new
// military kind resolves to its retained /4 row. Unknown non-Work kinds return
// no Work association, but a malformed new military source always fails.
// Neither view may outlive its actual input/retained Common and Duty roots.
Outcome<MilitaryDutyWorkAssignmentAssociation> read_military_duty_work_assignment_association(
    const CommonStore&,const MilitaryDutyState&,const SourceRef&,Integer source_schema,Integer selected_dynamic_military);
}
