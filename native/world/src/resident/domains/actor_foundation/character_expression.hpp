// P原本人表达限制；05§H/07§3 Named买方只消费此身体门，原接触/权/占用仍S5。
#pragma once
#include "character_capability.hpp"
namespace hanmo::resident {
struct CharacterExpressionView {StableId actor_id;ActorConditionHead condition_head;bool allowed=false;std::vector<CharacterConditionSourceRef> limiting_condition_refs;ActorFoundationRuleRef rule_ref;WorldMoment observed_at;};
// 唯一技术注册为character_expression/1，实际来源仍原ConditionHead/seed/receipt。
Outcome<CharacterExpressionView> read_character_expression(const CharacterConditionSources&,const StableId& actor_id,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
Outcome<CharacterExpressionView> read_character_expression_at_head(const CharacterConditionSources&,const ActorConditionHead& actual_head,const WorldMoment& actual_boundary);
Outcome<CharacterExpressionView> read_character_expression_for_span(const CharacterConditionSources&,const StableId& actor_id,const MomentSpan& actual_span);
Outcome<CharacterExpressionView> read_character_expression_for_span(const CharacterConditionSources&,const ActorConditionHead& actual_start_head,const MomentSpan& actual_span);
}
