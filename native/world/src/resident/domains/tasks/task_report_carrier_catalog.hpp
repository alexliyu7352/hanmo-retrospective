#pragma once
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/social/propagation_policy.hpp"
namespace hanmo::resident {
// MC1's original named-carrier declaration. These are immutable author fields,
// not a runtime Report, payment, route adoption or another accepted-task table.
struct TaskReportCarrierOfferDefinition {
 StableId id,rule_version,carrier_actor_id,role_id,dialogue_id,accept_node_id,destination_site_id;
 std::vector<StableId> allowed_sender_actor_ids,allowed_recipient_actor_ids;
 std::vector<KnowledgeCategory> allowed_report_categories;
};
struct TaskReportCarrierOfferCatalog {
 Integer version=1;CatalogIdentity identity;OrderedTable<TaskReportCarrierOfferDefinition> offers;
};
SourceRef task_report_carrier_offer_source(const TaskReportCarrierOfferDefinition&);
Status validate_task_report_carrier_offer_catalog(const TaskReportCarrierOfferCatalog&,const CommonCatalog&,const InteractionCatalog&);
Outcome<const TaskReportCarrierOfferDefinition*> read_task_report_carrier_offer(const TaskReportCarrierOfferCatalog&,const SourceRef&);
// The returned catalogue is compiled only from the same locked author bundle.
// Actor lists and the provider's real say node are checked before installation.
Outcome<TaskReportCarrierOfferCatalog> decode_task_report_carrier_offer_catalog(const BoundaryValue&,const CatalogIdentity&,const CommonCatalog&,const InteractionCatalog&);
Outcome<BoundaryValue> encode_task_report_carrier_offer_catalog(const TaskReportCarrierOfferCatalog&);
Status emit_task_report_carrier_offer_catalog(ProjectionWriter&,std::string_view,const TaskReportCarrierOfferCatalog&);
}
