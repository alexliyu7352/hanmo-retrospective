// PIN-1 derived edges from the actual Social/Task/Affairs/Director owners. No mutable refcount or second history.
#pragma once
#include "source_retention.hpp"
#include "resident/domains/social/social_time2.hpp"
#include "resident/domains/interaction/communication_links.hpp"
#include "resident/domains/interaction/runtime_offer_knowledge.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/director/director_subject2.hpp"
namespace hanmo::resident {
struct K6SourcePinAccess {const CommonStore& common;const SocialState& social;const AffairsState& affairs;const TaskState& tasks;const DirectorState& director;const CommunicationLinksState* communication=nullptr;const RuntimeOfferKnowledgeState* offers=nullptr;const InteractionState* interaction=nullptr;};
Outcome<std::vector<SourcePin>> enumerate_k6_source_pins(const K6SourcePinAccess&);
Status validate_k6_source_pin(const K6SourcePinAccess&,const SourcePin&);
}
