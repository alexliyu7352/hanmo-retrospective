// 本人表达实际事实前的有限原身体读面；不持久化head、prefix或第二身体账。
#pragma once
#include "character_capability.hpp"
namespace hanmo::resident {
// caller先验证实际Family A或S3 B的原payload及其中Moment/参与者；本口再核
// 同Common头、Origin真实先后及原身体前缀。不是任意字符串或任意Moment权限口。
// 已选 source2 必须有原 writer 同 ID 应用头；缺头拒绝，不取当前 head 猜过去。
// 当前有限消费者为Collateral source2、Care教育source2及S3实际Direct B；
// 首次 political_continuity/1 仅 foundation intent/support 两真源用原
// character_expression 目的，政治 caller 另核原 schema1/payload/参与者/M。
Outcome<CharacterCapabilityView> read_family_personal_capability_before_fact(
    const CharacterConditionSources &, const StableId &actor_id,
    const ActorFoundationRuleRef &actual_expression_purpose,
    const SourceRef &actual_consumer, const WorldMoment &actual_consumer_at);
} // namespace hanmo::resident
