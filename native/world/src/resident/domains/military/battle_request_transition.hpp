// 请求事务候选不持另一份世界账；上层必须把返回战场与唯一回执一起发布。
#pragma once
#include "battle_requests.hpp"
namespace hanmo::resident {
// 上层沿原军职权源核出的统帅范围，绝不接受玩家传入响应名单。
struct MilitaryRequestAuthority {
 StableId commander_actor_id,army_id,office_id,formation_source,appointment_source;
 std::vector<StableId> ai_unit_ids;
};
// 唯一历史回执保请求、裁定及原计划；命令幂等复用它，不再次调用裁定器。
struct MilitaryRequestReceipt {
 StableId id,actor_id,commander_actor_id,army_id,office_id,formation_source,appointment_source;
 Integer tick=0,round=0;
 MilitaryBattleRequest request;
 MilitaryRequestDecision decision;
 std::optional<MilitaryOrder> previous_order;
 FieldOrder field_order;
};
// 命令缓存只保存可公开答复；完整旧AI计划与权源仍只在request_facts唯一历史中。
struct MilitaryRequestAnswer {
 StableId receipt_id,battle_id,request_key,request_kind,unit_id,target_unit_id,commander_actor_id,reason,response_unit_id;
 Integer round=0,tick=0;bool accepted=false;
 std::optional<MilitaryOrder> response_order;
 FieldOrder field_order;
};
// replay时战场原样返回；新请求只替换一条已获权AI计划，不提前消耗时间或战法。
struct MilitaryRequestTransition {
 MilitaryBattleRecord battle;
 MilitaryRequestReceipt receipt;
 bool replay=false;
};
// receipt_id仅是预留候选ID；此纯门失败时不追加历史、不消耗世界ID或额度。
Outcome<MilitaryRequestTransition> prepare_military_request_transition(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const MilitaryBattleRequest&,const StableId& actor_id,const MilitaryRequestAuthority&,const MilitaryRequestPolicy&,const std::vector<MilitaryRequestReceipt>& history,const StableId& receipt_id,Integer tick,const MilitaryDamageSources& = {});
}
