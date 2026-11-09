// 有限地方对白使用原会话、typed任务和真实来源；关闭展示不取消世界责任。
#pragma once
#include "interaction_model.hpp"
namespace hanmo::resident {
struct BoundaryValue;
// 冷目录入口只接受封闭作者键，旧目录不调用也不追加持久字节。
Status configure_regional_life(InteractionCatalog&,const BoundaryValue&);
Status initialize_regional_life(InteractionState&,bool outcome_topics=false);
// 普通人物和岗位主题共用来源解析，成功报价不登记曝光。
Outcome<InteractionQuote> preview_regional_talk(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& observer,const StableId& topic,const StableId& provider={},const StableId& role={});
Status validate_regional_binding(const CommonRead&,const InteractionCatalog&,const InteractionAccess&,const RegionalOpportunityBinding&);
Status expose_regional_binding(const CommonRead&,InteractionState&,const InteractionCatalog&,const InteractionAccess&,const RegionalOpportunityBinding&);
// 只返回已冻结的任务；中央仍需原任务prepare/apply，不重新从当前候选换Offer。
Outcome<std::shared_ptr<const TaskCommand>> pending_regional_task(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& actor);
Status validate_regional_interaction(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess* access=nullptr);
StableId regional_source_episode(const RegionalSourceRef&);
bool same_regional_binding(const RegionalOpportunityBinding&,const RegionalOpportunityBinding&);
// root导出与typed checkpoint共用有限来源模型；不保存任意JSON执行负载。
Status decode_regional_interaction(const BoundaryValue&,InteractionState&);
BoundaryValue encode_regional_interaction(const InteractionState&);
Status visit_regional_interaction(CodecVisitor&,InteractionState&);
// 曝光不永久钉住历史结果；中央先保会话等因果pin，回收来源时清除对应旧曝光。
void prune_regional_exposures(InteractionState&,const std::vector<StableId>& retained_fact_ids);
}
