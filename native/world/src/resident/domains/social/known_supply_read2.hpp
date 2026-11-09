#pragma once
#include "social_time2.hpp"
namespace hanmo::resident {
// The market owner supplies its real inventory/site mapping. This reader proves only
// the subject's existing knowledge of that stock observation, never a sale or consent.
struct KnownSupplyObservation2 {
 // The one selected owning receipt has its own fresh-copy charge. Borrowed
 // report/observation fields still require this original Social owner to live.
 CheckpointMemoryCharge memory;
 KnownSupplyObservation2()=default;
 KnownSupplyObservation2(const KnownSupplyObservation2&)=delete;
 KnownSupplyObservation2&operator=(const KnownSupplyObservation2&)=delete;
 KnownSupplyObservation2(KnownSupplyObservation2&&)=default;
 KnownSupplyObservation2&operator=(KnownSupplyObservation2&&)=delete;
 const Report* report=nullptr;
 const SupplyObservation* observation=nullptr;
 KnowledgeReceiptV2 receipt;
 WorldMoment observed_at;
};
// Borrowed fields remain valid only within the same unmodified Social owner scope.
// No report, memory, message, observation or pin is created by this query.
Outcome<std::optional<KnownSupplyObservation2>> read_known_supply_observation2(
 const CommonStore&,const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,
 const SubjectRef&,const StableId& inventory_id,const StableId& site_id,const StableId& item_id,
 const WorldMoment& actual);
}
