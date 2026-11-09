// KQ1: an observer-correct, bounded projection over original knowledge. Querying never learns.
#pragma once
#include "social_time2.hpp"
#include "knowledge_category.hpp"
#include "knowledge_source_ref.hpp"
#include "resident/domains/tasks/work_execution/work_result_source.hpp"
#include "resident/domains/interaction/runtime_offer_knowledge.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/interaction/known_contacts/known_contact_location.hpp"
namespace hanmo::resident {
enum class KnowledgeScope {self_known,local_disclosed,institutional_known};
enum class KnowledgeState {known,remembered,unknown};
struct KnowledgeQueryContext {WorldIdentity expected_identity;SubjectRef observer;KnowledgeScope scope=KnowledgeScope::self_known;std::optional<StableId> access_actor_id;std::optional<ActingAuthority> acting_authority;};
struct ExactKnownReport {StableId report_id;};struct ExactKnownOffer {OfferRef offer;};struct KnownContact {StableId subject_actor_id;};struct ExactLegacyMemory {StableId fact_id;};
struct ListKnown {KnowledgeCategory category=KnowledgeCategory::supply;std::optional<SubjectRef> known_subject_filter;};
using KnowledgeQuery=std::variant<ExactKnownReport,ExactKnownOffer,KnownContact,ExactLegacyMemory,ListKnown>;
using QueryLookupKey=KnowledgeQuery;
struct KnowledgePageRequest {KnowledgeQueryContext context;KnowledgeQuery query;std::uint32_t limit=16;std::optional<StableId> cursor;};
struct KnownActionOutcome {StableId report_id,fact_id,action_id,action_kind;Integer completed_tick=0;bool success=false;StableId code;std::vector<SubjectRef> disclosed_participants;std::optional<WorkResultSource> original_work_result;};
struct KnownOfferDiscovery {OfferRef offer;Integer discovered_tick=0;KnowledgeSource source;std::optional<StableId> last_known_terms_revision;};
struct KnownLegacySocialMemory {StableId fact_id,event_kind;std::vector<StableId> disclosed_actor_ids;std::optional<StableId> place_id,item_id;std::optional<Integer> quantity;std::optional<StableId> reason;};
// Professional content remains owned/defined by its original producer; it is never widened to JSON here.
struct KnownIntelligenceProbe;struct KnownCulturalContent;
using KnownPayload=std::variant<KnownSupply,KnownArrear,KnownActionOutcome,KnownOfferDiscovery,KnownContactLocation,KnownLegacySocialMemory,std::shared_ptr<const KnownIntelligenceProbe>,std::shared_ptr<const KnownCulturalContent>>;
struct KnowledgeEntry {KnowledgeState state=KnowledgeState::unknown;QueryLookupKey lookup_key;std::optional<KnownPayload> payload;std::optional<KnowledgeSourceRef> source;std::optional<Integer> observed_tick,learned_tick,valid_until_tick;std::optional<ReportReliability> reliability;};
struct KnowledgePage {WorldIdentity source_identity;SubjectRef observer;KnowledgeScope scope=KnowledgeScope::self_known;std::vector<KnowledgeEntry> entries;std::optional<StableId> next_cursor;};
// Authenticated execution context is constructed only by the resident entry point, not decoded from the query.
struct KnowledgeCaller {enum class Kind {controlled_actor,director_actor};Kind kind=Kind::controlled_actor;StableId actor_id;const DirectorState* director=nullptr;};
class ProfessionalKnowledgeRead {public:virtual ~ProfessionalKnowledgeRead()=default;
 virtual bool supports(KnowledgeCategory)const=0;
 virtual bool disclosed_subject(const KnownPayload&,const SubjectRef&)const=0;
 virtual Outcome<std::optional<KnownPayload>> disclosed_payload(const Report&,const KnownReport&,KnowledgeCategory)const=0;
};
struct KnowledgeReadAccess {
 const CommonStore& common;const SocialState& social;const InteractionState& interaction;const TaskState& tasks;
 const OrganizationRead& organizations;KnowledgeCaller caller;
 const DynamicResultRead* legacy_results=nullptr;const ExactResultTimeRead* exact_results=nullptr;
 const KnownContactLocationAccess* contacts=nullptr;const SocialFactRead* legacy_facts=nullptr;
 const ProfessionalKnowledgeRead* professional=nullptr;
 const RuntimeOfferKnowledgeState* runtime_offers=nullptr;
};
class KnowledgeRead {const KnowledgeReadAccess& access_;public:explicit KnowledgeRead(const KnowledgeReadAccess& a):access_(a){}Outcome<KnowledgePage> query(const KnowledgePageRequest&)const;};
}
