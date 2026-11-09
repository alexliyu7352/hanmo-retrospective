#include "resident/kernel/world_timeline.hpp"
#pragma once
#include "social_knowledge_key.hpp"
#include "propagation_disposition_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct SocialPropagationAdoption {
 SocialPropagationAdoption()=default;
 SocialPropagationAdoption(const WorldMoment&,const CurrentCommonFactPrefix&);
 SocialPropagationAdoption(const SocialPropagationAdoption&);
 // Default valid Moment construction may allocate before the source is touched.
 SocialPropagationAdoption(SocialPropagationAdoption&&);
 SocialPropagationAdoption& operator=(const SocialPropagationAdoption&);
 SocialPropagationAdoption& operator=(SocialPropagationAdoption&&)noexcept;
 void swap(SocialPropagationAdoption&)noexcept;
 friend void swap(SocialPropagationAdoption& a,SocialPropagationAdoption& b)noexcept{a.swap(b);}
 // Nonwire ordinary-field fee, acquired before any owned source text.
 CheckpointMemoryCharge copy_memory;
 WorldMoment adopted_at;CommonActualFactPrefix original_prefix;
};
enum class SocialPropagationOutcome {declined,deferred,queued,delivered_locally};
// V§12.1 原两根完整字段，不保存第二消息/知识状态。
struct SocialPropagationDecision {
 SubjectRef subject;Integer registration_epoch=0,decision_seq=0;StableId report_id;
 FixedLockedRuleInput policy_ref;SocialKnowledgeKey source_known_key;
 std::vector<RelationProjection> source_relation_basis;
 std::vector<KnownSocialPolicyBasis> disposition_policy_basis;
 Integer evaluated_at=0;std::vector<SubjectRef> considered_recipients,selected_recipients;
 SocialPropagationOutcome outcome=SocialPropagationOutcome::declined;
 std::vector<StableId> actual_message_refs;Integer next_review_tick=0;std::vector<SourceRef> causal_source_refs;
 // Explicit dynamic_social7/8 tail only; old fifteen fields remain unchanged.
 std::optional<SocialPropagationAdoption> actual_adoption;
};
struct SocialPropagationCooldown {
 SubjectRef subject;StableId report_id;SubjectRef recipient;FixedLockedRuleInput policy_ref;
 Integer last_actual_dispatch_tick=0,next_eligible_tick=0;SourceRef source_decision_ref;
};
}
