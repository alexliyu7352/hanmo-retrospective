// Borrowed construction of the original private Repair command proposal.
// This is not a persistent row, another physical scope or an authority source.
#pragma once
#include "repair_service_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct RepairItemProposalBuildView {
 bool returning=false;
 std::string_view actor,job_id;
 const MomentOwnerSource& contract_ref;
 const MomentOwnerSource& contact;
 const MomentOwnerSource& destination_source;
 const ItemConditionHead& condition;
 const WorldMoment& at;
};
}
namespace hanmo::resident::repair_item_proposal_fields {
// Only ordinary copied proposal fields. The actual unique-transfer envelope
// moves its independently retained original payload; it is never copied here.
template<class C>void fields(C&c,const RepairItemProposalBuildView&v){
 c("returning",v.returning);c("actor",v.actor);c("job_id",v.job_id);
 c("contract_ref",v.contract_ref);c("contact",v.contact);c("destination_source",v.destination_source);
 c("condition",v.condition);c("at",v.at);
}
}
