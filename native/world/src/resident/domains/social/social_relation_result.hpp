// Original Social /2 application evidence; RelationRecord remains the only
// mutable base. This is independent of dynamic_social and social_evaluations.
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/world_timeline.hpp"
#include <string_view>
namespace hanmo::resident {
enum class OriginalSocialExchangeRole {theft_victim,theft_actor,refusal,aid_receiver,aid_helper};
struct SocialExchangeRelationCause {StableId event_id;OriginalSocialExchangeRole role=OriginalSocialExchangeRole::theft_victim;};
struct SocialNewsRelationCause {StableId news_event_id;};
struct SocialTaskReportRelationCause {StableId task_id,settlement_id;MomentOwnerSource actual_terminal;};
struct SocialGrowthServiceRelationCause {StableId action_id;MomentOwnerSource actual_terminal;};
using OriginalSocialRelationCause=std::variant<SocialExchangeRelationCause,SocialNewsRelationCause,SocialTaskReportRelationCause,SocialGrowthServiceRelationCause>;
// Borrowed address of the actual business source in the original closed cause.
// The pointed-to ID lives in that cause. Shape/address selection is not source
// validation; consumers must still use the original source reader.
struct OriginalSocialRelationBusinessSourceView {
 std::string_view domain,kind;const StableId* id=nullptr;Integer schema=0;
};
Outcome<OriginalSocialRelationBusinessSourceView> original_social_relation_business_source(
 const OriginalSocialRelationCause&);
struct SocialRelationResult {
 StableId result_id,relation_record_key,from_actor,to_actor;
 OriginalSocialRelationCause original_cause;
 Integer trust_before=0,affinity_before=0,trust_delta=0,affinity_delta=0;
 Integer trust_after=0,affinity_after=0;WorldMoment applied_at;
};
// Immutable values from the actual locked data.relations author table only.
struct OriginalSocialRelationSeed {StableId from_actor,to_actor;Integer trust=0,affinity=0;};
struct OriginalSocialRelationCatalog {CatalogIdentity identity;OrderedTable<OriginalSocialRelationSeed> relations;};
// Pure original arithmetic shared by the old writer, selected writer and fold.
Integer clamp_original_social_relation(Integer before,Integer delta) noexcept;
bool same_original_social_relation_cause(const OriginalSocialRelationCause&,const OriginalSocialRelationCause&) noexcept;
Status validate_social_relation_result_shape(const SocialRelationResult&);
}
