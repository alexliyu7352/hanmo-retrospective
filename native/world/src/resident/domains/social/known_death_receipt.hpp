// A reference to actual personal knowledge of the one original LIFE4 death.
// This is neither the old physical Task notice/lot nor an inheritance grant.
#pragma once
#include "actor_report_knowledge.hpp"
#include "resident/domains/family/family_life_exact.hpp"
namespace hanmo::resident {
struct ActorKnownDeathReceiptRef {StableId report_id;DeathRef death;};
struct ActorKnownDeathReceiptView {
 const FactRecord* death_source=nullptr;const FamilyDeathRecord* death=nullptr;
 ActorReportKnowledgeReceiptView acquired;
};
Outcome<std::optional<ActorKnownDeathReceiptView>> read_actor_known_death_receipt(
 const ActorReportKnowledgeContext&,const FamilyDeathPolicy&,const FamilyDeathState&,const FamilyLifeCauseSources4&,
 const StableId& observer,const ActorKnownDeathReceiptRef&);
Outcome<ActorKnownDeathReceiptView> read_actor_known_death_receipt_before_source(
 const ActorReportKnowledgeContext&,const FamilyDeathPolicy&,const FamilyDeathState&,const FamilyLifeCauseSources4&,
 const StableId& observer,const ActorKnownDeathReceiptRef&,
 const SourceRef& actual_consumer,const WorldMoment& actual_consumer_at);
// Embedded by the actual consuming owner's explicit physical successor. There
// is no separate receipt table and no fallback to old Task/lot field identities.
namespace known_death_receipt_codec {
template<class Archive>Status fields(Archive&a,ActorKnownDeathReceiptRef&v){
 a.field("report_id",v.report_id);
 return a.object("death_ref",[&](auto&c){c.field("domain",v.death.domain);c.field("kind",v.death.kind);return c.field("id",v.death.id);});
}
}
}
