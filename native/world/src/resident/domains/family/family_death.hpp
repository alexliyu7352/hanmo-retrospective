// 死亡记录只引用已经验证的真实损失来源；记录本身不继承资产、军职或玩家控制。
#pragma once
#include "resident/storage/common_codec.hpp"
#include <functional>
#include "family_life_exact_types.hpp"
namespace hanmo::resident {
// Author selection is explicit. The original decoder always selects physical 1;
// complete 4 has its own strict decoder and does not upgrade existing records.
struct FamilyDeathPolicy { bool enabled=false; Integer selected_version=1; };
Outcome<FamilyDeathPolicy> decode_family_death_policy(const BoundaryValue&);
Outcome<FamilyDeathPolicy> decode_family_death_policy4(const BoundaryValue&);
Status validate_family_death_policy_selection(const FamilyDeathPolicy&,const CatalogIdentity&);
// 显式生命模式下本人死亡即停所有业务命令；保存/标题不经此门，NPC与旧模式不受影响。
Status guard_family_death_player_command(const CommonStore&,const FamilyDeathPolicy&,const StableId& actor_id);
// 军事owner提供的有限投影；完整战斗/海军历史先由其自身验证，S4不复制名单账。
struct FamilyDeathOrigin { StableId actor_id,casualty_fact_id,battle_id;Integer tick=0; };
using FamilyDeathOriginRead=std::function<Outcome<std::optional<FamilyDeathOrigin>>(const StableId&)>;
struct FamilyDeathRecord {
    StableId kind="family_death",actor_id,scenario_id,cause_kind="military_casualty",cause_fact_id,battle_id;
    WorldId world_id{};Integer death_tick=0,recorded_tick=0;FieldOrder field_order;
    // 新4只使用此payload及上方actor/world/scenario身份；旧cause/tick槽必须为空/0。
    std::optional<FamilyDeathPayload4> precise;
};
// 独立后续生命账本，不扩写当前已冻结的Family1..16或另存人物alive真值。
struct FamilyDeathState { OrderedTable<FamilyDeathRecord> deaths;FieldOrder field_order; };
// 仅同世界实际死亡者与精确原因账相符才可作为遗留事务来源。
Status validate_family_death_record(const CommonStore&,const FamilyDeathRecord&,const FamilyDeathOriginRead&);
Status validate_family_deaths(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,bool enabled);
// 稳定World/冷读额外核真实死亡覆盖；同删本域payload和Common头也不能藏起已发生的终点。
Status validate_family_death_coverage(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&,bool enabled);
// 在clock_commit后消费实际损失来源；重入复用原ID，晚失败回滚Common头与本账。
Outcome<StableId> record_family_death(CommonStore&,FamilyDeathState&,const FamilyDeathOriginRead&,const StableId& actor_id);
// 一次实际伤亡批次全部登记或全部撤回；不接受无界或重复人物列表。
Outcome<std::vector<StableId>> record_family_deaths(CommonStore&,FamilyDeathState&,const FamilyDeathOriginRead&,const std::vector<StableId>& actor_ids);
// 稳定阶段只收录owner可证明且尚未登记的命名死亡；作者初态无来源者保持原状。
Outcome<std::vector<StableId>> record_verified_family_deaths(CommonStore&,FamilyDeathState&,const FamilyDeathOriginRead&);
// 历史读取不授予遗产处分权；没有具名来源返回空值而不是臆造死亡证明。
const FamilyDeathRecord* family_death_for_actor(const FamilyDeathState&,const StableId& actor_id);
// 当前人物专页不泄漏无关NPC死亡或冒充遗产授权；查询保持全部账本与RNG不变。
Outcome<BoundaryValue> query_family_death_page(const CommonStore&,const FamilyDeathState&,const FamilyDeathOriginRead&);
// 新域JSON与typed冷边界均拒绝坏字段序和重复键，完整来源仍由上述业务校验。
Outcome<FamilyDeathState> decode_family_deaths(const BoundaryValue&);
BoundaryValue encode_family_deaths(const FamilyDeathState&);
class CodecVisitor;
Status visit_family_deaths_checkpoint(CodecVisitor&,FamilyDeathState&);
}
