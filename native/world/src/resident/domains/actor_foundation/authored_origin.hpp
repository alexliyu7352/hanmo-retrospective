// 同版作者来源构造/读取；只在真实冷构造写入来源，读档从不调用初始化。
#pragma once
#include "model.hpp"
namespace hanmo::resident {
struct GrowthState;struct MovementExactState;
struct BoundaryValue;
class AuthoredRestTimeRead;
// Original validation scratch row, not a saved model or another body ledger.
// Named only so the sole checkpoint engine can describe its actual ownership.
struct AuthoredConditionCurrent {
 Integer hp,fatigue,injury;bool alive;Integer revision=0;
 std::optional<ConditionReceiptRefV2> latest;WorldMoment last_at;
};
// 冷作者附项使用完整AuthoredActorStateV2/InitialConditionSeed字段，属性/技能只读原initial行。
Outcome<ActorFoundationCatalog> decode_authored_actor_foundation_catalog(const BoundaryValue& selected_definition,const BoundaryValue& original_initial,const CatalogIdentity&,const StableId& scenario_id,std::shared_ptr<const GrowthCatalog>);
// 原native_actor_seed在Actor尚未插入Common时调用；不接受活人物容器。
Status initialize_authored_actor_foundation_actor(ActorRecord&,const ActorFoundationCatalog&,const StableId&);
// 必须完整匹配锁定作者初态，失败不修改Common/ID/RNG。
Outcome<AuthoredActorFoundationState> make_authored_actor_foundation_seed(const CommonState&,const ActorFoundationCatalog&);
// 纯身份reader不以今日身体冒初态；实际身体闭包由下方用途/validator核。
Outcome<CharacterOriginRecordV2> read_authored_character_origin_v2(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const StableId&);
// Complete-world authored subset: validates only the immutable authored roots,
// permitting separately proved dynamic actors without cloning/encoding the state.
Status validate_authored_start_context_sources(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&);
// 完整校验真实来源链；有rest历史时必须供原Growth和K2精确终态reader。
Status validate_authored_actor_foundation(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const GrowthState* =nullptr,const AuthoredRestTimeRead* =nullptr,const MovementExactState* =nullptr);
// 当前仅原rest双方身体用途，未知purpose/病效kind不能返回空成功。
Outcome<FrozenFacilityCapabilityInputs> capture_authored_rest_capability(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const StableId&,const ActorFoundationRuleRef&,const WorldMoment&,const GrowthState* =nullptr,const AuthoredRestTimeRead* =nullptr,const MovementExactState* =nullptr);
// 冷反验只沿真实seed/receipt链取指定head；不从今日身体倒造过去。
// K2仍负责证明该head就是真实边界读戳和同M先后，不能任意旧head冒当时输入。
Outcome<FrozenFacilityCapabilityInputs> read_authored_rest_capability_at_head(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const ActorConditionHead&,const ActorFoundationRuleRef&,const WorldMoment&,const GrowthState*,const AuthoredRestTimeRead*,const MovementExactState* =nullptr);
// 三个原字段身份比较供本owner codec/validator复用，不按地址或ID词序决定因果。
bool actor_foundation_rule_equal(const ActorFoundationRuleRef&,const ActorFoundationRuleRef&);
bool actor_foundation_origin_equal(const ScenarioOriginRef&,const ScenarioOriginRef&);
// Authored comparisons deliberately reject every dynamic branch.
bool actor_foundation_origin_equal(const InitialConditionSeedSource&,const ScenarioOriginRef&);
bool actor_foundation_origin_equal(const InitialConditionSeedSource&,const InitialConditionSeedSource&);
} // 未选Origin2的legacy完全不调用本叶。
