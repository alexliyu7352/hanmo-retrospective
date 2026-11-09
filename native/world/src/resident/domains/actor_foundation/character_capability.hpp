// CharacterCapabilityRead/1：只读唯一身体与原已选用途规则；调用方另核权、接触、占用。
#pragma once
#include "character_condition_read.hpp"
#include "resident/kernel/read_stamp.hpp"
#include <array>
namespace hanmo::resident {
struct CharacterCapabilityView {
 StableId actor_id;ActorFoundationRuleRef purpose_ref;ActorConditionHead condition_head;
 FrozenBodyScalars body;WorldMoment observed_at;bool allowed=false;Integer efficiency_bp=0;
 std::vector<CharacterConditionSourceRef> limiting_condition_refs;
 std::vector<CharacterEfficiencyComponent> registered_efficiency_components;
 ActorFoundationRuleRef rule_ref;
};
// study/provide_teaching按原growth_lesson_xp的确定min规则；rest沿原身体门。
// 工作/表达purpose必须由锁内具名binding选择原reducer；未选目的不借本查询放宽权限。
Outcome<CharacterCapabilityView> read_character_capability_at_head(const CharacterConditionSources&,const ActorConditionHead&,const ActorFoundationRuleRef& purpose,const std::optional<StableId>& skill_id,const WorldMoment& actual_boundary);
// 热读必须含本Actor资源版本和当前ConditionHead域版本；stamp不进入持久结果。
Outcome<CharacterCapabilityView> read_character_capability(const CharacterConditionSources&,const StableId& actor_id,const ActorFoundationRuleRef& purpose,const std::optional<StableId>& skill_id,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
// 固定FSL输入沿同一个字段结构冻结，不生成新的能力事实。
Outcome<FrozenFacilityCapabilityInputs> freeze_character_capability(const CharacterCapabilityView&);
}

namespace hanmo::resident {
Status validate_character_capability_catalog(const ActorFoundationCatalog&);
// Compiled complete-profile construction only; no new authored reducer string.
Status register_original_personal_training_purpose(ActorFoundationCatalog&);
Status register_original_growth_service_work_purpose(ActorFoundationCatalog&);
// 原scalar交易只沿alive身体门；接触/可用性/权限仍由原业务独立校验。
Status register_original_growth_trade_participation_purpose(ActorFoundationCatalog&);
// P29: same selected normal-work rule for actual owner and accepted staff.
Status register_original_private_shop_operation_purpose(ActorFoundationCatalog&);
// C01/C02: actual ordinary Craft worker; skill/recipe/equipment stay original.
Status register_original_craft_job_work_purpose(ActorFoundationCatalog&);
// FH5.3B care/5 only: original normal-work eligibility, no H/cost scaling.
Status register_original_care_handover_work_purpose(ActorFoundationCatalog&);
}
namespace hanmo::resident {
// 半开实际供给窗：本次真实输入head在from采用，to处结算后的变化可供后继窗。
// 中间任一真实Condition变化都必须先由原owner分段，不能以旧能力包跨过。
Outcome<CharacterCapabilityView> read_character_capability_for_span(const CharacterConditionSources&,const ActorConditionHead& actual_start_head,const ActorFoundationRuleRef& purpose,const std::optional<StableId>& skill_id,const MomentSpan& actual_span);
}
namespace hanmo::resident {
// 全窗只读遍历：沿原Condition revision切实际正片，同Moment先消费全部已应用前驱。
// 无新状态、无用力结算；visitor可保存原head/source，不得改同一来源候选。
class CharacterCapabilitySpanVisitor {
public:
 virtual ~CharacterCapabilitySpanVisitor()=default;
 virtual Status visit_capability_span(const MomentSpan&,const CharacterCapabilityView&)=0;
};
Status visit_character_capability_span(const CharacterConditionSources&,const ActorConditionHead& actual_start_head,const ActorFoundationRuleRef& purpose,const std::optional<StableId>& skill_id,const MomentSpan&,CharacterCapabilitySpanVisitor&);
}

namespace hanmo::resident {
// Same-stack application only: this consumes the unique writer's real capture
// identity/ordinal and scalar draft. It is not a replacement for full source
// validation at installation, cold restore, or the stable candidate exit.
Outcome<FrozenFacilityCapabilityInputs> freeze_character_capability_after_bound_condition(
 const CharacterConditionSources&,const FrozenFacilityCapabilityInputs& before,
 const std::optional<StableId>& required_skill,const BoundActorConditionWrite&);
}

namespace hanmo::resident {
// Borrowed actual receipt and compiled registered purpose. This derives only
// the two original boolean gates; it neither records a capacity transition nor
// treats an efficiency decrease as an inability. A skill-dependent purpose
// needs its own original typed selection and cannot enter this finite reader.
struct CharacterPurposeReceiptChange {
 const ConditionReceiptV2* receipt=nullptr;
 const CharacterPurposeBinding* purpose_binding=nullptr;
 bool before_allowed=false,after_allowed=false;
};
Outcome<CharacterPurposeReceiptChange> read_character_purpose_change_at_receipt(
 const CharacterConditionSources&,const ConditionReceiptRefV2&,
 const ActorFoundationRuleRef& actual_selected_purpose);
}

namespace hanmo::resident {
// Compiled Care5/Condition4 only. Original family_child_care::reachable alive
// gate; availability, lawful activity and occupation remain separate sources.
Status register_original_care_participant_purpose(ActorFoundationCatalog&);
}
