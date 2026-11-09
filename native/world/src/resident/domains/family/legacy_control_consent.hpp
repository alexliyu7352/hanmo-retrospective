// 独立的生前条件控制许可；不是退隐赠产同意，不授予死亡知情或遗产权利。
#pragma once
#include "family_model.hpp"
#include "life_age_basis.hpp"
#include "biological_control_origin.hpp"
#include "resident/domains/interaction/born_dialogues/born_personal_dialogue_contract.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp"
#include "resident/domains/interaction/dialogue_node_source.hpp"
namespace hanmo::resident {
class FamilyContinuationPeopleRead4;
struct DiscipleControlRelationContext;
struct BiologicalControlDecisionContext;
struct CollateralControlDecisionContext;
struct LegacyControlConsentDefinition {
    StableId id,predecessor_actor_id,successor_actor_id,dialogue_id,accept_node_id,revoke_node_id;
    Integer terms_version=1;
};
struct LegacyControlConsentCatalog { bool enabled=false;OrderedTable<LegacyControlConsentDefinition> definitions;Integer version=1; };
// 永久依据不保存会话epoch作为跨档许可；范围固定为前代死亡后候选自己的原有人生控制。
struct LegacyControlDecisionSource {
    StableId request_id,decision,predecessor_actor_id,successor_actor_id,relation_fact_id;
    StableId dialogue_id,node_id,node_kind,session_id,scenario_id;
    WorldId world_id{};InteractionContext context;
    Integer terms_version=1,tick=0,revision=0,predecessor_birth_tick=0,successor_birth_tick=0;
    FieldOrder field_order;
    // 只由显式FamilyLegacy/2保存；旧/1与旧本人命令不能静默忽略这个来源。
    std::optional<BiologicalControlOrigin> biological_origin;
    // 动态本人许可永久钉原打开事实、模板和真实provider声明，不能换另一场同tick会面。
    std::optional<BornPersonalDialogueProof> born_dialogue;
    // 仅显式/3：原已认可旁支以relation_fact_id引用，不复造血缘或控制账。
    std::optional<KnownPersonalDialogueProof> collateral_dialogue;
    // Explicit /4 replaces the two birthday scalars with actual age references.
    std::optional<FamilyDecisionAgeSources4> age_sources;
};
struct LegacyControlDecisionQuote { AuthoredNodeSourceProof current_source;LegacyControlDecisionSource decision; };
// 作者必须各列本人接受与撤回的不同节点；旧退隐/赠产scope与未知字段明确拒绝。
Outcome<LegacyControlConsentCatalog> decode_legacy_control_consent_catalog(const BoundaryValue& config,const BoundaryValue& definitions);
// /2明确选择同一原许可账；纯亲生政策可不附造旧收养/弟子请求。
Outcome<LegacyControlConsentCatalog> decode_legacy_control_consent_catalog_v2(const BoundaryValue& config,const BoundaryValue& original_definitions);
// /3显式已认可旁支来源；不借旧/1或/2目录形状静默升级。
Outcome<LegacyControlConsentCatalog> decode_legacy_control_consent_catalog_v3(const BoundaryValue& config,const BoundaryValue& original_definitions);
Status validate_legacy_control_consent_catalog(const LegacyControlConsentCatalog&,const CommonCatalog&,const InteractionCatalog&);
// 历史只核原关系与本人实际办理来源，双方后来死亡不抹掉原决定，也不自动使许可生效。
Status validate_legacy_control_decision_source(const CommonStore&,const FamilyState&,const CommonCatalog&,const InteractionCatalog&,const LegacyControlConsentCatalog&,const LegacyControlDecisionSource&,const DiscipleControlRelationContext* disciple=nullptr,const StableId& actual_before_fact={},const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
// 生前真实会面才可取得接受/撤回报价；查询不写计划、控制或任何资产。
Outcome<LegacyControlDecisionQuote> preview_legacy_control_decision(const CommonStore&,const FamilyState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,const StableId& predecessor,const StableId& successor,const StableId& request_id,const StableId& decision,const DiscipleControlRelationContext* disciple=nullptr,const StableId& revocation_relation_id={},const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
Status revalidate_legacy_control_decision(const CommonStore&,const FamilyState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,const LegacyControlDecisionQuote&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
bool legacy_control_decision_equal(const LegacyControlDecisionSource&,const LegacyControlDecisionSource&);
// 独立新域才保存这些决定；JSON/typed均完整保留来源与字段序，旧Family字节不加存在位。
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source(const BoundaryValue&);
BoundaryValue encode_legacy_control_decision_source(const LegacyControlDecisionSource&);
class CodecVisitor;
Status visit_legacy_control_decision_source(CodecVisitor&,LegacyControlDecisionSource&);
// 新格式具名入口，不靠默认bool重载推测段形状；旧三个入口仍逐字旧格式。
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v2(const BoundaryValue&);
BoundaryValue encode_legacy_control_decision_source_v2(const LegacyControlDecisionSource&);
Status visit_legacy_control_decision_source_v2(CodecVisitor&,LegacyControlDecisionSource&);
// /3明确新增本人私人模板来源，旧两个入口拒绝该字段且不多写一个存在位。
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v3(const BoundaryValue&);
BoundaryValue encode_legacy_control_decision_source_v3(const LegacyControlDecisionSource&);
Status visit_legacy_control_decision_source_v3(CodecVisitor&,LegacyControlDecisionSource&);
Outcome<LegacyControlConsentCatalog> decode_legacy_control_consent_catalog_v4(const BoundaryValue&,const BoundaryValue&);
Outcome<LegacyControlDecisionSource> decode_legacy_control_decision_source_v4(const BoundaryValue&);
BoundaryValue encode_legacy_control_decision_source_v4(const LegacyControlDecisionSource&);
Status visit_legacy_control_decision_source_v4(CodecVisitor&,LegacyControlDecisionSource&);

}
