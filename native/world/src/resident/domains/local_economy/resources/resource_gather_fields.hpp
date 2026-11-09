#pragma once
#include "resource_gather_model.hpp"
#include "resource_site_recovery_fields.hpp"
#include "resident/kernel/fixed_service_work_period_codec_fields.hpp"
namespace hanmo::resident {


}
namespace hanmo::resident::resource_gather_fields {
using namespace exact_fields;
using resource_recovery_fields::begin;
template<class C>Status terms(C&c,ResourceGatherAcceptedTerms&v){begin(c);
 required_ref(c,"acceptance_id",v.acceptance_id);
 required_ref(c,"claim_id",v.claim_id);
 required_ref(c,"site_id",v.site_id);
 required_ref(c,"action_id",v.action_id);
 required_ref(c,"actor_id",v.actor_id);
 required_ref(c,"item_id",v.item_id);
 required_ref(c,"recipient_inventory_id",v.recipient_inventory_id);
 c.object("harvest_entitlement_source",[&](auto&x){begin(x);return resource_recovery_fields::original(x,v.harvest_entitlement_source);});
 c.object("policy_ref",[&](auto&x){begin(x);return resource_recovery_fields::rule_ref(x,v.policy_ref);});
 {Integer tag=static_cast<Integer>(v.method);c.field("method",tag);if(tag<0||tag>2)return c.reject("UnsupportedGatherMethod");if(c.reading())v.method=static_cast<GatherMethod>(tag);}
 c.field("requested_units",v.requested_units);
 c.field("reserved_source_units",v.reserved_source_units);
 c.field("accepted_half_days",v.accepted_half_days);
 c.object("accepted_at",[&](auto&x){begin(x);return moment(x,v.accepted_at);});
 c.object("return_or_stop_policy",[&](auto&x){begin(x);return source(x,v.return_or_stop_policy);});
 return c.status();}
template<class C>Status claim(C&c,ResourceGatherClaim&v){begin(c);
 required_ref(c,"claim_id",v.claim_id);
 required_ref(c,"accepted_terms_ref",v.accepted_terms_ref);
 c.field("remaining_reserved_units",v.remaining_reserved_units);
 c.optional("terminal_disposition_ref",v.terminal_disposition_ref,[](auto&x,auto&r){begin(x);return source(x,r);});
 return c.status();}
template<class C>Status receipt(C&c,ResourceGatherReceipt&v){begin(c);
 required_ref(c,"receipt_id",v.receipt_id);
 required_ref(c,"claim_id",v.claim_id);
 c.object("actual_period",[&](auto&x){begin(x);return fixed_period_fields::ref(x,v.actual_period);});
 c.object("applied_at",[&](auto&x){begin(x);return moment(x,v.applied_at);});
 {Integer tag=static_cast<Integer>(v.method);c.field("method",tag);if(tag<0||tag>2)return c.reject("UnsupportedGatherMethod");if(c.reading())v.method=static_cast<GatherMethod>(tag);}
 c.field("harvested_units",v.harvested_units);
 c.field("site_depletion_units",v.site_depletion_units);
 c.field("loss_units",v.loss_units);
 c.sequence("worker_input_sources",v.worker_input_sources,[](auto&x,auto&r){begin(x);return resource_recovery_fields::original(x,r);});
 c.sequence("output_lot_refs",v.output_lot_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.sequence("lot_properties_refs",v.lot_properties_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.sequence("tool_wear_refs",v.tool_wear_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.sequence("condition_application_refs",v.condition_application_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.sequence("resource_effect_refs",v.resource_effect_refs,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.object("observation_result_ref",[&](auto&x){begin(x);return source(x,v.observation_result_ref);});
 c.optional("zero_output_reason",v.zero_output_reason,[](auto&x,auto&r){begin(x);return source(x,r);});
 c.object("policy_ref",[&](auto&x){begin(x);return resource_recovery_fields::rule_ref(x,v.policy_ref);});
 return c.status();}
template<class C>Status disposition(C&c,ResourceGatherDisposition&v){begin(c);
 required_ref(c,"disposition_id",v.disposition_id);
 required_ref(c,"claim_id",v.claim_id);
 c.object("original_action_disposition",[&](auto&x){begin(x);return source(x,v.original_action_disposition);});
 c.object("occurred_at",[&](auto&x){begin(x);return moment(x,v.occurred_at);});
 c.field("released_source_units",v.released_source_units);
 c.field("terminal",v.terminal);
 return c.status();}
template<class C>Status root(C&c,ResourceGatherOwnerState&v){begin(c);
 c.sequence("accepted_terms",v.accepted_terms,[](auto&x,auto&r){begin(x);return terms(x,r);});
 c.sequence("claims",v.claims,[](auto&x,auto&r){begin(x);return claim(x,r);});
 c.sequence("receipts",v.receipts,[](auto&x,auto&r){begin(x);return receipt(x,r);});
 c.sequence("dispositions",v.dispositions,[](auto&x,auto&r){begin(x);return disposition(x,r);});
 if(c.status().ok){auto valid=validate_resource_gather_owner_values(v);if(!valid.ok)return valid;}
 return c.status();}
}
