// 人物HP/疲劳的具名业务来源合同；尚未注册，不能用该DTO替代任何真实历史验证。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 只有已审原ActorMutation写者。军队单位疲劳/战斗临时unit.hp不在此表，须待真实写回Actor。
enum class GrowthPersonalConditionBusiness {
    terrain_travel,road_travel,estate_food,military_shortage,study_lesson,
    training_completion,long_rest_lesson,service_completion,
    personal_terminal,military_actor_casualty,recovery_credit
};
// 热原写栈先有业务键，实际source fact可能稍后才分配；不能伪造头或拿开始头代完工。
struct GrowthPersonalConditionBusinessKey {
    GrowthPersonalConditionBusiness kind=GrowthPersonalConditionBusiness::study_lesson;
    StableId original_business_key;
    Integer effective_tick=0,ordinal=0;
};
// 原Common真实ActorMutation只投影这些字段。相同值的实际写入也须记presence，不能漏掉封顶步骤。
struct GrowthPersonalConditionFields {
    std::optional<Integer> hp,fatigue;
};
struct GrowthPersonalConditionActorEffect {
    StableId actor_id;
    GrowthPersonalConditionFields before,after;
};
// 捕获稿本身不证明合法原因；只准同候选同scope在原业务载荷成功后精确绑定。
struct GrowthPersonalConditionCaptured {
    WorldIdentity source_identity;
    GrowthPersonalConditionBusinessKey business;
    // 中央实际执行捕获的顺序，不由业务方/快照指定；source头先后不等于Actor写入先后。
    Integer capture_sequence=0;
    std::vector<GrowthPersonalConditionActorEffect> actors;
};
// 来源读由唯一业务owner实现，不能从本捕获稿/当前Actor反推历史参与者或增减量。
// actual_fact_id是原业务永久头；重复课时用真实ordinal。Personal终局还须原result分配序。
struct GrowthPersonalConditionCause {
    WorldIdentity source_identity;
    GrowthPersonalConditionBusinessKey business;
    StableId actual_fact_id,actual_completion_id;
    std::vector<StableId> actor_ids;
};
enum class GrowthPersonalConditionOriginKind { installed_actor,character_creation,adoption,birth };
struct GrowthPersonalConditionActorOrigin {
    WorldIdentity source_identity;
    GrowthPersonalConditionOriginKind kind=GrowthPersonalConditionOriginKind::installed_actor;
    StableId actor_id,authored_actor_id,creation_fact_id,business_source_fact_id;
    Integer created_tick=0,hp=100,fatigue=0;
};
class GrowthPersonalConditionSourceRead {
public:
    virtual ~GrowthPersonalConditionSourceRead()=default;
    // Installed初值及动态创建各认真实原权源，不能从当前Actor反推100/0。
    virtual Outcome<GrowthPersonalConditionActorOrigin> condition_actor_origin(const StableId& actor)const=0;
    // 原业务全验在前；逐实际源证明本次全部人物写入，包括原顺序、clamp、Recovery伤势上限。
    virtual Outcome<GrowthPersonalConditionCause> condition_cause(
        const GrowthPersonalConditionBusinessKey&,const StableId& actual_fact_id)const=0;
    virtual Status verify_condition_effects(const GrowthPersonalConditionCause&,
        const std::vector<GrowthPersonalConditionActorEffect>&)const=0;
    // 从原业务账反向枚举，不能使用未来condition账自己的ID列表自证覆盖。
    virtual Outcome<std::vector<GrowthPersonalConditionCause>> condition_causes()const=0;
};
// 中央共享接缝需求（本头不定义Common观察器、不启profile/持久状态）：
// 1. 显式选中profile的静态保护随CommonCatalog复制，缺同候选observer/scope即拒HP/疲劳写。
// 2. apply_actor须先完整校验并暂存Actor，捕获失败不得发布其他字段；旧profile全走原路径。
// 3. create_actor仅正规Installed或真实Family/Creation/Birth原始100HP/0疲劳，动态身份同受保护。
// 4. scope的before/after来自同一次原写，不接受命令/读档传入快照；捕获后在实际业务源落账时绑定。
// 5. 未绑定稿/重复消费禁止提交；Common+原业务+新增永久源同一外层保存点，无嵌套savepoint。
// 6. clone/move清借用但保必须绑定配置；rollback恢复原scope草稿，World服务按克隆对象重新绑定。
// 7. 新独立永久源尾保留原业务引用；旧未记逐期参与者的档明确HistoryUnavailable，不补历史。
// 8. checkpoint最终从真实创建初态按完整源序重放并核今日Actor；本账从不成为第二个可写HP/疲劳。
}
