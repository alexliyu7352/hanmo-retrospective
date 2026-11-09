// 军官调任只借真实政治签发与受命来源；此合同不写军籍、不改军团或公共资产。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 当前指挥来源是原作者、最后一次真实编军或已应用调任，不能只比较同名军官而忽略中间变更。
enum class MilitaryUnitCommandSourceKind { authored,army_formation,officer_assignment,field_assignment };
struct MilitaryUnitCommandSource {
    MilitaryUnitCommandSourceKind kind=MilitaryUnitCommandSourceKind::authored;
    StableId id;
};
struct MilitaryUnitCommandBinding { StableId commander_actor_id;MilitaryUnitCommandSource command_source; };
struct MilitaryUnitCommandObservation {
    WorldIdentity source_identity;
    StableId unit_id,organization_id,garrison_id,army_id,commander_actor_id;
    MilitaryUnitCommandSource command_source;
};
struct MilitaryOfficerAssignmentTarget {
    StableId organization_id,garrison_id,unit_id,previous_actor_id,next_actor_id;
    MilitaryUnitCommandSource previous_source;
};
// DTO由同候选S6读口产生；接受者须为next_actor，签发岗位和专项授权必须仍允许此唯一目标。
struct MilitaryOfficerAppointmentProof {
    WorldIdentity source_identity;
    MilitaryOfficerAssignmentTarget target;
    StableId authorization_fact_id,acceptance_fact_id,issuer_actor_id,office_id,grant_id;
    Integer authority_epoch=0,issued_tick=0,accepted_tick=0,observed_tick=0;
};
class MilitaryOfficerAppointmentRead {
public:
    virtual ~MilitaryOfficerAppointmentRead()=default;
    virtual Outcome<MilitaryOfficerAppointmentProof> appointment_for_application(
        const StableId& authorization_fact_id,const StableId& acceptance_fact_id,const StableId& accepting_actor_id)const=0;
    // 仅真实military_officer_assignment/military头之前的原签发与受命；S2再核本域载荷和完成tick。
    // 这不是任意时刻授权口，也不能拿当前Office/epoch回填历史。
    virtual Outcome<MilitaryOfficerAppointmentProof> appointment_before_application(
        const StableId& authorization_fact_id,const StableId& acceptance_fact_id,
        const StableId& assignment_fact_id,Integer completed_tick)const=0;
};
}
