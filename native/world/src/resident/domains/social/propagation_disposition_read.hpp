// LIFE v1b §12.8. Read-only values adopted by S0, never a second Social state.
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "social_relation_cut.hpp"
namespace hanmo::resident {
struct SocialEvaluationContributionRef {StableId evaluation_id;Integer revision=0;};
struct RecordedRelationBasis {StableId from_actor_id,to_actor_id;std::optional<StableId> record_id;};
struct NoRecordedRelationBasis {StableId from_actor_id,to_actor_id;ActorFoundationRuleRef original_zero_rule_ref;};
using SocialRelationBasis=std::variant<RecordedRelationBasis,NoRecordedRelationBasis>;
struct RelationProjection {
 StableId from_actor_id,to_actor_id;SocialRelationBasis basis;
 Integer effective_trust=0,effective_affinity=0;
 std::vector<SocialEvaluationContributionRef> current_contribution_refs;
};
struct SocialPolicySource {
 enum class Lookup{actor_id,prototype_id};
 CatalogIdentity catalog_identity;Lookup lookup=Lookup::actor_id;StableId policy_actor_key;
};
struct KnownSocialPolicyBasis {
 enum class Kind{cooperative,selfish};
 SocialPolicySource policy_source;Kind kind=Kind::cooperative;bool share_news=false;Integer interval_ticks=0;
};
struct UnknownSocialPolicyBasis {enum class Reason{no_matching_authored_policy};Reason reason=Reason::no_matching_authored_policy;};
using SocialDispositionPolicyBasis=std::variant<UnknownSocialPolicyBasis,KnownSocialPolicyBasis>;
struct PropagationDispositionV1 {
 StableId sender_actor_id,candidate_receiver_actor_id;
 SocialDispositionPolicyBasis policy_basis;RelationProjection relation_projection;WorldMoment observed_at;
};
// The provider must validate the same candidate stamp before reading either
// actor, the immutable catalog lookup, and the original effective relation.
class PropagationDispositionRead {
public:
 virtual ~PropagationDispositionRead()=default;
 virtual Outcome<PropagationDispositionV1> read_propagation_disposition_v1(const StableId&,const StableId&,const ReadStamp&)const=0;
 // Historical verification uses the original source cut saved by the S0
 // decision; today's relation is never substituted for the adopted snapshot.
 virtual Status validate_adopted_propagation_disposition(const PropagationDispositionV1&,const SourceRef& actual_decision_source)const=0;
 // Explicit dynamic_social 7/8: the real pre-channel adoption cut is supplied
 // by its original owner, never reconstructed from a future decision row.
 virtual Status validate_adopted_propagation_disposition(const PropagationDispositionV1&,const SourceRef& actual_decision_source,const OriginalSocialRelationCut&)const=0;
};
}
