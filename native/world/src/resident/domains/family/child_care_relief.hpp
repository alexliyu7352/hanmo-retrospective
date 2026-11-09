// 有限志愿照护独立账；只记录自己的接受、原Action及实际消费，不复制出生账或公共库存。
#pragma once
#include "child_care_relief_source.hpp"
#include <set>
namespace hanmo::resident {
struct ChildCareReliefAgreement {StableId kind="family_care_relief_accepted";ChildCareReliefSource source;Integer reserved_food=0;FieldOrder field_order;};
struct ChildCareReliefStart {StableId kind="family_care_relief_started",accepted_id,action_id,provider_actor_id,child_actor_id,place_id;Integer tick=0,revision=0,duration=0,period_index=0;FieldOrder field_order;};
struct ChildCareReliefReceipt {StableId kind="family_care_relief_provided",accepted_id,action_id,provider_actor_id,child_actor_id,place_id,outcome="completed",code,interruption_fact_id;Integer started_tick=0,tick=0,revision=0,period_index=0,food_consumed=0;FieldOrder field_order;};
// 双方可停止尚未提供的志愿帮助；只能释放本新合同的提供者私粮，原出生合同永不受此closure修改。
struct ChildCareReliefClosure {StableId kind="family_care_relief_closed",accepted_id,actor_id,reason,source_fact_id;Integer tick=0,revision=0,released_food=0;FieldOrder field_order;};
struct ChildCareReliefState {OrderedTable<ChildCareReliefAgreement> agreements;OrderedTable<ChildCareReliefStart> starts;OrderedTable<ChildCareReliefReceipt> care;OrderedTable<ChildCareReliefClosure> closures;FieldOrder field_order;};
struct ChildCareReliefAccess {
    // 已通过完整控制链与Military/Naval/FamilyLife的同候选纯读；空口必须拒绝，不能猜空闲。
    std::function<Outcome<StableId>(Integer,const StableId&)> player_before;
    std::function<Status(const StableId&,Integer,const StableId&)> decision_maker_before;
};
struct ChildCareReliefTimeline {
    OrderedTable<StableId> active_by_child,pending_by_agreement;
    OrderedTable<Integer> remaining_food,completed_periods;
    std::set<StableId> closed_agreements,completed_actions;
};
// 预约归属只认本账真实接受payload；任意同名Common头不能创建可消费权。
bool child_care_relief_reservation_exists(const CommonStore&,const ChildCareReliefState&,const ReservationRef&);
Outcome<ChildCareReliefTimeline> validate_child_care_relief(const CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,const ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& finishing_action={},const StableId& starting_action={});
// 只写Common＋本域；World/NPC已有保存点时只能调用in_candidate，失败须整体丢弃。
Outcome<StableId> accept_child_care_relief_in_candidate(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const ChildCareReliefQuote&);
Outcome<StableId> withdraw_child_care_relief_in_candidate(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor_id,const StableId& accepted_id);
Outcome<StableId> accept_child_care_relief(CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const ChildCareReliefQuote&);
Outcome<StableId> withdraw_child_care_relief(CommonStore&,const InteractionCatalog&,const ChildCareReliefCatalog&,const FamilyBirthState&,const FamilyDeathState&,ChildCareReliefState&,const ChildCareReliefAccess&,const StableId& actor_id,const StableId& accepted_id);
Outcome<ChildCareReliefState> decode_child_care_relief(const BoundaryValue&);
BoundaryValue encode_child_care_relief(const ChildCareReliefState&);
Status visit_child_care_relief_checkpoint(CodecVisitor&,ChildCareReliefState&);
}
