// Runtime offer knowledge is an original Interaction receipt, never inferred from today's open stock.
#pragma once
#include "resident/model/offer_ref.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct RuntimeOfferDiscovery {StableId id;SubjectRef observer;OfferRef offer;SourceRef original_disclosure_ref;StableId disclosed_terms_revision;WorldMoment learned_at;std::optional<StableId> local_place_id;};
struct RuntimeOfferKnowledgeState {OrderedTable<RuntimeOfferDiscovery> discoveries;};
class RuntimeOfferDisclosureRead {public:virtual ~RuntimeOfferDisclosureRead()=default;virtual Outcome<RuntimeOfferDiscovery> read_runtime_offer_disclosure(const SourceRef&)const=0;};
Status record_runtime_offer_discovery_in_candidate(const CommonStore&,RuntimeOfferKnowledgeState&,const SourceRef&,const RuntimeOfferDisclosureRead&,const WorldMoment&);
Status validate_runtime_offer_knowledge(const CommonStore&,const RuntimeOfferKnowledgeState&,const RuntimeOfferDisclosureRead&);
Outcome<RuntimeOfferKnowledgeState> decode_runtime_offer_knowledge(const BoundaryValue&);
Outcome<BoundaryValue> encode_runtime_offer_knowledge(const RuntimeOfferKnowledgeState&);
struct CodecVisitor;Status visit_runtime_offer_knowledge_checkpoint(CodecVisitor&,RuntimeOfferKnowledgeState&);
}
