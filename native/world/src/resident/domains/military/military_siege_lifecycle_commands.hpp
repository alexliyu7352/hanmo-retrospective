// 原围城操作的具名永久参与写者；返回原纯规则结果，资产和Actor仍由同一外层候选办理。
#pragma once
#include "military_siege_lifecycle.hpp"
namespace hanmo::resident {
// 创建/加入只接受已真实落下的原campaign_arrival；调用者仍须原军令/到场preview。
Outcome<SiegeRecord> create_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeCreateRequest&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const StableId& arrival_fact_id);
Outcome<SiegeRecord> join_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const StableId& army,Integer side,const StableId& arrival_fact_id);
// 工程头必须先真实追加；原材料effect应用失败时，必须连此参与事实一起回滚。
Outcome<SiegeResolution> step_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeOrders&,const DynamicStepInput&);
// 原会战完成回执才可归还节点；不以今天phase或缺Action推断结束。
Outcome<SiegeRecord> finish_military_siege_battle_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& node,const StableId& battle_fact_id,const DynamicStepInput* phase=nullptr);
// 本人原战略命令与原强制终止理由存为原输入，不伪造另一个外部授权ID。
Outcome<SiegeResolution> manage_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& actor,const SiegeStrategicCommand&);
Outcome<SiegeResolution> terminate_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const SiegeObservation&,const StableId& reason,const DynamicStepInput* phase=nullptr);
// 原安全离城事件已实际完成返回军令后，仅登记真实退出节点，不授予新返程权限。
Status record_departed_military_siege_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord& before_departure,const SiegeRecord& after_departure,const SiegeTopology&);
// 原M8本人确认回执已经写入才关闭；原M5整编事实已经写入才记整编终局。
Outcome<SiegeRecord> close_military_siege_with_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord&,const SiegeTopology&,const StableId& actor,const StableId& release_fact_id);
Status record_integrated_military_siege_lifecycle(CommonStore&,MilitarySiegeLifecycleState&,
 const SiegeRecord& before,const SiegeRecord& after,const SiegeTopology&,const StableId& transfer_fact_id,Integer tick);
}
