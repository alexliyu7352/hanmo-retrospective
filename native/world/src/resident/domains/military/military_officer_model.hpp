// 营内军官交接独立记录指挥来源；兵员、部队、公库仍保留原Military/Common唯一身份。
#pragma once
#include "military_officer_contract.hpp"
namespace hanmo::resident {
struct MilitaryOfficerAssignmentPolicy { StableId profile="political_appointment_v1"; };
struct MilitaryOfficerAssign {
    StableId request_key,authorization_fact_id,acceptance_fact_id;
    FieldOrder field_order;
};
struct MilitaryOfficerAssignmentFact {
    StableId id,request_key,actor_id,authorization_fact_id,acceptance_fact_id,place_id;
    MilitaryOfficerAssignmentTarget target;
    StableId issuer_actor_id,office_id,grant_id;
    Integer authority_epoch=0,issued_tick=0,accepted_tick=0,tick=0;
    FieldOrder source_field_order,target_field_order,field_order;
};
struct MilitaryOfficerAssignmentQuote { bool replayed=false;MilitaryOfficerAssignmentFact fact; };
}
