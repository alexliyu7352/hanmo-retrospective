// 显式个人恢复的永久履约源；服务原账保存钱款，本账仅保存逐半日恢复与真实终止。
#pragma once
#include "growth_injury_sources.hpp"
namespace hanmo::resident {
enum class GrowthRecoveryMethod { safe_lodging=0,qualified_clinic=1 };
struct GrowthRecoveryCatalog {
    // 键为原Growth服务role；服务必须确实提供rest，不能由hp_gain自动认定治疗资格。
    OrderedTable<GrowthRecoveryMethod> service_roles;
};
struct GrowthRecoveryService {
    StableId actor_id,provider_actor_id,role_id,place_id;
    GrowthRecoveryMethod method=GrowthRecoveryMethod::safe_lodging;
    Integer start_tick=0,duration=0;
    FieldOrder field_order;
};
struct GrowthRecoveryStep {
    StableId kind="personal_recovery_credit",action_id;
    Integer tick=0,ordinal=0;
    GrowthRecoveryProgress before,after;
    Integer offered_points=0,used_points=0;
    FieldOrder field_order;
};
struct GrowthRecoveryClosure {
    StableId kind="personal_recovery_closed",action_id;
    Integer tick=0,provided_half_days=0;
    // completed/cancelled/patient_dead/provider_dead；死亡须exact原死因头。
    StableId reason,cause_fact_id;
    // 只在实际提交尾绑定；未绑定仅允许同候选中间态，最终World必须拒绝。
    StableId commit_result_id,commit_actor_id,commit_kind;
    Integer commit_request_seq=0,commit_revision=0;
    bool commit_advance=false;
    FieldOrder field_order;
};
struct GrowthRecoveryState {
    // services键就是原life_activity/action ID；credit与closure各有独立真实recovery:实例头。
    OrderedTable<GrowthRecoveryService> services;
    OrderedTable<GrowthRecoveryStep> credits;
    OrderedTable<GrowthRecoveryClosure> closures;
    FieldOrder field_order;
};
// 只核静态形状与进度数学，不以此替代原服务/伤因完整验证或并发活动准入。
Status validate_growth_recovery_shape(const GrowthRecoveryState&);
Integer growth_recovery_service_points(GrowthRecoveryMethod);
struct GrowthRecoveredActor {
    WorldIdentity source_identity;
    StableId actor_id,original_casualty_id,last_recovery_fact_id,before_fact_id;
    Integer observed_tick=0,recovered_tick=0;
};
// S2只用本人口完整身体来源决定具名伤员回队，不获得改injury、军籍或公职的权限。
// actual_before_fact为空只限稳定态或显式严格phase；冷非空仅military_recovery:/military_wounded_recovery/military真实头，S2先核payload.tick。
class GrowthRecoveredActorRead {
public:
    virtual ~GrowthRecoveredActorRead()=default;
    virtual Outcome<std::optional<GrowthRecoveredActor>> recovered_actor(const StableId& actor_id,const StableId& original_casualty_id,Integer observed_tick,const StableId& actual_before_fact_id,const DynamicStepInput* phase=nullptr) const=0;
};
}
