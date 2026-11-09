// 逐次真实舟行的持久来源；不保存第二份航线、钱包或当前位置，不按世界时钟补算航损。
#pragma once
#include "naval_assets.hpp"
#include "resident/domains/travel/travel_model.hpp"
namespace hanmo::resident {
// 远航长度由作者明示；启动时沿原Travel报价判断，后来船损不倒改已接受的出航资格。
struct NavalNavigationPolicy {
 StableId profile="committed_navigation_v1";
 Integer wear_interval=10,wear_damage=5,long_voyage_min_half_days=10,long_voyage_min_integrity=30;
};
// 原编队实际船组和启程船体；冷读必须与原事实前资产/军籍重放一致，不能把此快照当权源。
struct NavalVoyageVessel {
 StableId vessel_id,unit_id,captain_actor_id;Owner owner;Integer integrity=0;FieldOrder field_order;
};
// 一份原terrain_journey恰有一个启程条款，改道使用原Travel新journey而不是覆盖旧条款。
struct NavalVoyageFact {
 StableId id,journey_id,action_id,actor_id,fleet_id,army_id,formation_source_id,command_source_id,payer_wallet_id,map_id,origin;
 Integer tick=0,action_start_tick=0,quoted_half_days=0,budget=0,initial_credit=0;std::vector<NavalVoyageVessel> vessels;FieldOrder field_order;
};
// 累计值沿同一真实船只跨换将/接管延续，维修不重置已航行半日，所有累计均由历史折叠。
struct NavalNavigationVesselStep {
 StableId vessel_id,previous_step_id;Integer completed_before=0,completed_after=0,integrity_before=0,integrity_after=0;FieldOrder field_order;
};
// movement为最终实际落点；接敌另保该成功Travel步的原未截断结果及具名接触源。
struct NavalNavigationStepFact {
 StableId id,voyage_source_id,fleet_id,army_id,contact_source_id;
 Integer tick=0;TerrainExecution movement;std::optional<TerrainExecution> uncut;
 std::vector<NavalNavigationVesselStep> vessels;bool interrupted=false;FieldOrder movement_field_order,uncut_field_order,field_order;
};
struct NavalNavigationState {std::vector<NavalVoyageFact> voyages;std::vector<NavalNavigationStepFact> steps;FieldOrder field_order;};
// 原step成功后才生成临时输入；暂停/hold/资格失败不创建此对象，不能拿contact预探替代执行。
struct NavalNavigationExecution {TerrainExecution movement;std::optional<TerrainExecution> uncut;StableId contact_source_id;};
// 失能只结束原Travel并保实际浮泊；由中央送原计划/等候对白的明确失败完成，不冒到达。
struct NavalNavigationTermination {StableId source_id,action_id,actor_id,reason="NavalNavigationDisabled";Integer tick=0;};
}
