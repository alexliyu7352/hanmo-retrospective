// 独立遗留控制许可账；三类事实各自不可变，控制结果不包含产权或知情变更。
#pragma once
#include "legacy_control_consent.hpp"
#include "family_legacy_source.hpp"
namespace hanmo::resident {
struct ControlContinuationState;
class OriginalContinuationConsentWrite;
enum class FamilyLegacyVersion { original1=1,biological2=2,collateral3=3,declared_age4=4 };
struct FamilyLegacyConsentFact { StableId kind="family_legacy_control_consent";LegacyControlDecisionSource source;FieldOrder field_order; };
struct FamilyLegacyRevocationFact { StableId kind="family_legacy_control_revocation",consent_fact_id;LegacyControlDecisionSource source;FieldOrder field_order; };
struct FamilyLegacyActivationFact { StableId kind="family_legacy_control_activated",consent_fact_id,death_fact_id;Integer tick=0;FieldOrder field_order; };
struct FamilyLegacyState {
    OrderedTable<FamilyLegacyConsentFact> consents;
    OrderedTable<FamilyLegacyRevocationFact> revocations;
    OrderedTable<FamilyLegacyActivationFact> activations;
    FieldOrder field_order;
};
// 独立family_legacy/1冷边界；所有表、来源和键序显式保存，不扩展旧Family段。
Outcome<FamilyLegacyState> decode_family_legacy(const BoundaryValue&);
BoundaryValue encode_family_legacy(const FamilyLegacyState&);
Status visit_family_legacy_checkpoint(CodecVisitor&,FamilyLegacyState&);
// /2仍是同一三表控制账，只在来源内显式选择亲代tag；外层module版本决定入口。
Outcome<FamilyLegacyState> decode_family_legacy_v2(const BoundaryValue&);
BoundaryValue encode_family_legacy_v2(const FamilyLegacyState&);
Status visit_family_legacy_checkpoint_v2(CodecVisitor&,FamilyLegacyState&);
Outcome<FamilyLegacyState> decode_family_legacy_v3(const BoundaryValue&);
BoundaryValue encode_family_legacy_v3(const FamilyLegacyState&);
Status visit_family_legacy_checkpoint_v3(CodecVisitor&,FamilyLegacyState&);
// 借原规则核当前/历史可玩处境；空before_fact_id仅用于当前报价，冷史须核精确事实边界。
using FamilyLegacyPlayableRead=std::function<Status(const StableId& actor_id,Integer tick,const StableId& before_fact_id)>;
struct FamilyLegacyActivationQuote {
    WorldIdentity source_identity;
    StableId consent_fact_id,death_fact_id,predecessor_actor_id,successor_actor_id;
};
// 合并旧Family与新遗留控制链，按Common事实序保历史玩家，不回填当前控制身份。
Outcome<StableId> family_player_before_fact_with_legacy(const CommonStore&,const FamilyState&,const FamilyLegacyState&,Integer tick,const StableId& before_fact_id,const ControlContinuationState* controls=nullptr);
Status validate_family_legacy(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const CommonCatalog&,const InteractionCatalog&,const LegacyControlConsentCatalog&,const FamilyLegacyState&,const FamilyLegacyPlayableRead& historical_playable={},const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
// 真实本人节点再验证后，只落接受或撤回账；没有资产、控制或获知副作用。
Outcome<StableId> record_family_legacy_decision(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const LegacyControlConsentCatalog&,FamilyLegacyState&,const LegacyControlDecisionQuote&,const StableId& target_consent_id={},const FamilyLegacyPlayableRead& historical_playable={},const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr,OriginalContinuationConsentWrite* continuation_writer=nullptr);
// 本人死亡稳定选择只激活尚未撤回的零资产许可；须另由原规则核下一人物的合法处境。
Outcome<FamilyLegacyActivationQuote> preview_family_legacy_activation(const CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const CommonCatalog&,const InteractionCatalog&,const LegacyControlConsentCatalog&,const FamilyLegacyState&,const FamilyLegacyPlayableRead&,const StableId& consent_fact_id,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
// 保存点涵盖控制与所有事实；晚失败完整撤回，不改变两人的既有钱包、行动、关系与公权。
Outcome<StableId> activate_family_legacy_control(CommonStore&,const FamilyState&,const FamilyDeathState&,const FamilyDeathOriginRead&,const CommonCatalog&,const InteractionCatalog&,const LegacyControlConsentCatalog&,FamilyLegacyState&,const FamilyLegacyPlayableRead&,const FamilyLegacyActivationQuote&,const DiscipleControlRelationContext* disciple=nullptr,const BiologicalControlDecisionContext* biological=nullptr,const CollateralControlDecisionContext* collateral=nullptr,const FamilyContinuationPeopleRead4* complete_people=nullptr);
Outcome<FamilyLegacyState> decode_family_legacy_v4(const BoundaryValue&);
BoundaryValue encode_family_legacy_v4(const FamilyLegacyState&);
Status visit_family_legacy_checkpoint_v4(CodecVisitor&,FamilyLegacyState&);

}
