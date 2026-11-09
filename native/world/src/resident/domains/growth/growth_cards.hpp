// 卡片记录取得来源；本领、掌握和当前配置仍唯一属于共同人物，不从图鉴复制能力。
#pragma once
#include "resident/storage/common_codec.hpp"

namespace hanmo::resident {
struct GrowthState;
// 七类定义完整分类；本片只接武功与心法的真实能力引用，其余效果明确拒绝。
enum class GrowthCardCategory { actor,skill,technique,heart,tactic,title,insight };
enum class GrowthCardSourceKind { scenario_initial,training };
struct GrowthCardDefinition {
    StableId id,name_key,description_key,hint_key,icon_id,personal_skill_id;
    GrowthCardCategory category=GrowthCardCategory::technique;
    FieldOrder field_order{};
};
struct GrowthCardInitialGrant { StableId actor_id,card_id;FieldOrder field_order{}; };
struct GrowthCardCatalog {
    bool enabled=false;
    OrderedTable<GrowthCardDefinition> definitions;
    std::vector<GrowthCardInitialGrant> initial_grants;
};
// 一人一卡只保存第一次取得的合法来源；后续熟练度继续从原训练事实和ActorCombat读取。
struct GrowthCardAcquisition {
    StableId card_id,actor_id,source_id;
    GrowthCardSourceKind source_kind=GrowthCardSourceKind::training;
    Integer acquired_tick=0;
    FieldOrder field_order{};
};
struct GrowthCardsState { OrderedTable<OrderedTable<GrowthCardAcquisition>> owned;FieldOrder field_order{}; };
// 类型与名字之间只有闭合映射，未知分类不能降为装饰卡或执行自定义脚本。
StableId growth_card_category_name(GrowthCardCategory);
Outcome<GrowthCardCategory> growth_card_category(const StableId&);
Status validate_growth_card_catalog(const GrowthCardCatalog&,const CommonCatalog&);
// 作者定义和情景初态授权分开；未知效果或重复策略不接入运行规则。
Outcome<GrowthCardCatalog> decode_growth_card_catalog(const BoundaryValue& definitions,const BoundaryValue& scenario_config,const CommonCatalog&);
// 初态只承认作者明确列出的已学本领；无出生/复活/重新捏脸或奖励副作用。
Status initialize_growth_cards(const CommonState&,GrowthCardsState&,const GrowthCardCatalog&);
// 只有已经实际结算的原学习/传授事实可以取得卡片，重复来源不重复发卡或改人物。
Outcome<std::vector<StableId>> record_growth_training_cards(const CommonState&,const GrowthState&,GrowthCardsState&,const GrowthCardCatalog&,const StableId& fact_id);
Status validate_retained_legacy_growth_card_rows(const CommonState&,const GrowthState&,const GrowthCardsState&,const GrowthCardCatalog&);
Status validate_growth_cards(const CommonState&,const GrowthState&,const GrowthCardsState&,const GrowthCardCatalog&);
// 本人有限卡片页；图鉴发现只决定展示，不能改变learned/mastery/configured。
Outcome<BoundaryValue> growth_cards_view(const CommonState&,const GrowthCardsState&,const GrowthCardCatalog&,const CommonCatalog&,const StableId& actor_id,const std::vector<StableId>& discovered,Integer offset=0,Integer limit=32);
// 卡片持久块仅由新显式module调用，旧growth检查点不增加存在位或字段。
Outcome<GrowthCardsState> decode_growth_cards(const BoundaryValue&);
BoundaryValue encode_growth_cards(const GrowthCardsState&);
Status visit_growth_cards_checkpoint(CodecVisitor&,GrowthCardsState&);
}
