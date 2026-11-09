// P24.3/V12.2: actual original participant effects, before any new evaluation.
// This borrowed read does not create DirectEffectSource, learn a report, infer a
// topic, or apply/reverse a delta. New evaluation ownership remains separate.
#pragma once
#include "social_relation_execution.hpp"
namespace hanmo::resident {
struct OriginalSocialDirectEffectRead {
 const SocialRelationResult* original_result=nullptr;
 OriginalSocialRelationBusinessSourceView original_business;
 OriginalSocialRelationRule original_rule=OriginalSocialRelationRule::theft_victim;
 // receiver is original_result->from_actor, target is its to_actor. Both are
 // proved by the original actual cause, not supplied as a purported consent.
};
// Empty means a fully validated original news reaction, which is indirect.
// Invalid/missing original result or source is an error, never an empty proof.
Outcome<std::optional<OriginalSocialDirectEffectRead>> read_original_social_direct_effect(
 const OriginalSocialRelationContext&,const SourceRef& actual_result,Integer schema);
// The knowledge/request owner must supply its independently verified actual
// adoption cut. This checks the result was already present there, including
// exact same-M order. It does not accept a caller's time as a knowledge source.
Outcome<std::optional<OriginalSocialDirectEffectRead>> read_original_social_direct_effect_at_cut(
 const OriginalSocialRelationContext&,const SourceRef& actual_result,Integer schema,
 const OriginalSocialRelationCut&);
}
