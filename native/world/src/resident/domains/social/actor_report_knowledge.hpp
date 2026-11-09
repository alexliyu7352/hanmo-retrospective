// Pure provenance of one already-known Report. No receipt, delivery or Fact is
// produced here; all returned rows borrow the same original World lease.
#pragma once
#include "social_time2.hpp"
#include "social_period_sources.hpp"
#include "resident/domains/interaction/communication_links.hpp"
namespace hanmo::resident {
enum class ActorReportKnowledgeKind {direct_observation,legacy_channel,personal_delivery,dossier_read};
struct ActorReportKnowledgeContext {
 const CommonStore& common;const SocialState& social;const ExactResultTimeRead& results;const ExactResultReadStamp& stamp;
 const CommunicationLinksState* communications=nullptr;const CommunicationSourceRead* channels=nullptr;
 const SocialPeriodSources* periods=nullptr;const WorldTimelineState* timeline=nullptr;const CatalogIdentity* catalog=nullptr;
 Integer initial_tick=0;
};
struct ActorReportKnowledgeReceiptView {
 const Report* report=nullptr;KnowledgeReceiptViewV2 knowledge;
 const SocialSourceTimeBinding* acquisition=nullptr;
 ActorReportKnowledgeKind kind=ActorReportKnowledgeKind::direct_observation;
 const PendingMessage* message=nullptr;const MessageBusinessLink* business=nullptr;
 const CommunicationDeliverySource* delivery=nullptr;const CommunicationDossierAccess* dossier=nullptr;
};
Outcome<std::optional<ActorReportKnowledgeReceiptView>> read_actor_report_knowledge_receipt(
 const ActorReportKnowledgeContext&,const StableId& actor,const StableId& actual_report_id);
// The consumer owner must first verify its immutable actual source/M. An equal
// Moment alone cannot order a collector or dossier read against a later writer.
Status validate_actor_report_knowledge_before_source(const ActorReportKnowledgeContext&,
 const ActorReportKnowledgeReceiptView&,const SourceRef& actual_consumer,const WorldMoment& actual_consumer_at);
}
