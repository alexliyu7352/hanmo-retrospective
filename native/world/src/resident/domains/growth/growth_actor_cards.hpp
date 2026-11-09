// 人物卡只投影真实已结算成果，供跨局资格账消费；不改变本局人物或任务。
#pragma once
#include "resident/domains/tasks/task_completion_evidence.hpp"
#include "growth_envoy_sources.hpp"

namespace hanmo::resident {
struct GrowthActorCardDefinition {
    StableId id,target_actor_id,name_key,description_key,hint_key;
    StableId condition="completed_procurement_twice_v1";
};
struct GrowthActorCardCatalog {
    bool enabled=false;
    OrderedTable<GrowthActorCardDefinition> definitions;
    Integer version=1;
};
// 永久取得来源不保存当前执行代；旧世界载入后仍以真实任务源重建同一证据。
struct GrowthActorUnlockEvidence {
    StableId card_id,target_actor_id,earner_actor_id,scenario_id;
    WorldId world_id{};
    Integer acquired_tick=0;
    std::vector<TaskCompletionEvidence> tasks;
    std::optional<GrowthEnvoyRoundTripEvidence> envoy;
};
struct GrowthActorCardRow {
    StableId card_id,target_actor_id;
    Integer completed_count=0;
    std::optional<GrowthActorUnlockEvidence> acquired;
};
// 临时页面绑定完整当前身份；分页不允许旁观者借用另一个人物的成果。
struct GrowthActorCardsPage {
    WorldIdentity source_identity;
    StableId actor_id;
    Integer offset=0,limit=32,total=0,acquired_total=0;
    std::vector<GrowthActorCardRow> cards;
};
Status validate_growth_actor_card_catalog(const GrowthActorCardCatalog&,const CommonCatalog&);
Outcome<GrowthActorCardsPage> project_growth_actor_cards(const CommonState&,const CommonCatalog&,const TaskState&,const TaskCatalog&,const GrowthActorCardCatalog&,const StableId& actor_id,Integer offset=0,Integer limit=32);
// v2借同一世界的实际使者读口；旧入口仍只能读取原采购来源。
Outcome<GrowthActorCardsPage> project_growth_actor_cards_with_envoy(const CommonState&,const CommonCatalog&,const TaskState&,const TaskCatalog&,const GrowthActorCardCatalog&,const EnvoyTaskState&,const TaskEnvoyRead&,const StableId& actor_id,Integer offset=0,Integer limit=32);
// 内部取得owner读取真实本人来源；不裁当前控制人，也不把此读口作为公开旁观页。
Outcome<std::vector<GrowthActorUnlockEvidence>> read_growth_actor_unlock_evidence(const CommonState&,const CommonCatalog&,const TaskState&,const TaskCatalog&,const GrowthActorCardCatalog&,const StableId& actor_id,const EnvoyTaskState* =nullptr,const TaskEnvoyRead* =nullptr);
// 有限UI边界仅编码本域投影及来源，不返回整个TaskState或Common世界。
Outcome<BoundaryValue> encode_growth_actor_cards_page(const GrowthActorCardsPage&,const GrowthActorCardCatalog&,const CommonCatalog&);
}
