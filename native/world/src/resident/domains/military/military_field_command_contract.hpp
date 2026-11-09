// 野外伤亡后的指挥交接独立于驻营调任：专项政治签受只定权源，军旅仍核现场和唯一占用。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// unit只接替一个实际在军单位；army只由已经具名统带该军一部的本人接替总指挥。
enum class MilitaryFieldCommandKind { unit,army };
// 原始形成、驻营交接和本域后续交接有不同来源，不覆写ArmyFormationTerms或Naval形成者。
enum class MilitaryFieldCommandSourceKind { unit_authored,army_formation,garrison_assignment,field_assignment };
struct MilitaryFieldCommandSource {MilitaryFieldCommandSourceKind kind=MilitaryFieldCommandSourceKind::army_formation;StableId id;};
struct MilitaryArmyCommandBinding {StableId commander_actor_id;MilitaryFieldCommandSource command_source;};
struct MilitaryFieldCommandDeath {StableId actor_id,casualty_fact_id,battle_id;Integer tick=0;};
struct MilitaryFieldCommandTarget {
    MilitaryFieldCommandKind kind=MilitaryFieldCommandKind::unit;
    StableId organization_id,army_id,campaign_id,formation_source_id;
    // unit模式是待接替部队；army模式是接受者已真实统带的本军部队。
    StableId unit_id,previous_actor_id,next_actor_id;
    MilitaryFieldCommandSource previous_source;
    // army模式必须提供接受者当下部队指挥来源；unit模式无此字段，不以尚未发生的任命作前置。
    std::optional<MilitaryFieldCommandSource> candidate_unit_source;
    MilitaryFieldCommandDeath predecessor_death;
    // 空表示未编舰；非空必须指向同军唯一活动fleet，不能从水面坐标猜上船。
    StableId fleet_id;
};
// 观察不授权、不写位置；政治签发再限定同组织专项Grant和接受者。
struct MilitaryFieldCommandObservation {
    WorldIdentity source_identity;
    MilitaryFieldCommandKind kind=MilitaryFieldCommandKind::unit;
    StableId organization_id,army_id,campaign_id,formation_source_id,origin_garrison_id;
    StableId unit_id,commander_actor_id,fleet_id,vessel_id;
    MilitaryFieldCommandSource command_source;
    bool army_active=false;
    std::optional<MilitaryFieldCommandDeath> commander_death;
};
struct MilitaryFieldCommandAppointmentProof {
    WorldIdentity source_identity;
    MilitaryFieldCommandTarget target;
    StableId authorization_fact_id,acceptance_fact_id,issuer_actor_id,office_id,grant_id;
    Integer authority_epoch=0,issued_tick=0,accepted_tick=0,observed_tick=0;
};
class MilitaryFieldCommandAppointmentRead {
public:
    virtual ~MilitaryFieldCommandAppointmentRead()=default;
    virtual Outcome<MilitaryFieldCommandAppointmentProof> appointment_for_application(
        const StableId& authorization_fact_id,const StableId& acceptance_fact_id,const StableId& accepting_actor_id)const=0;
    // 只接受military_field_command_assigned/military真实头的前边界，不能将当前Office/epoch回填历史。
    virtual Outcome<MilitaryFieldCommandAppointmentProof> appointment_before_application(
        const StableId& authorization_fact_id,const StableId& acceptance_fact_id,
        const StableId& assignment_fact_id,Integer completed_tick)const=0;
};
// 物理接触观察由同候选S2生成，不是政治许可或任意坐标输入。水上仅同原vessel binding；
// 陆上要求同一实际AtPlace或同一合法野外点。既有死者行旅必须已由原伤亡出口终止。
struct MilitaryFieldCommandContact {
    WorldIdentity source_identity;
    // unit_id是所读actor当时真实所属部队；issuer可不同于target.unit_id，受命人须精确相同。
    StableId army_id,unit_id,accepting_actor_id,previous_actor_id,fleet_id,vessel_id;
    // 历史只给实际军籍/同船来源；未有独立位置事实时保持empty，不回填今日坐标。
    std::optional<Location> army_anchor,actor_location;
    Integer observed_tick=0;StableId boundary_fact_id;
    bool existing_army_member=false;
};
}
