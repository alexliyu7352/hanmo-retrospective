// Finite accepted-intent payload in the ORIGINAL missions row. No new root,
// timing balance, command envelope or admission of dynamic_military/3.
#pragma once
#include "dynamic_action_model.hpp"
namespace hanmo::resident {
struct MilitaryPlayerDirectIntent3 {
 StableId controller_actor_id;Integer control_epoch=0;
 // common/command_result/1: the actual sealed command, never this row's head.
 MomentOwnerSource actual_command_source;
};
struct MilitaryCommittedDecisionIntent3 {
 // military/dynamic_session_state/2, actual DecisionCommitted payload.
 MomentOwnerSource decision_source_ref;
 StableId controller_ref;Integer control_binding_generation=0;
};
struct MilitaryDeliveredOrderIntent3 {
 // Original tactical-order owner fixes each actual source kind and schema.
 // Neither the request nor this row can choose a delivery/response policy.
 MomentOwnerSource order_ref,actual_response_source;
};
using MilitaryIntentAdoption3=std::variant<MilitaryPlayerDirectIntent3,
 MilitaryCommittedDecisionIntent3,MilitaryDeliveredOrderIntent3>;
struct MilitaryAcceptedIntent3 {
 StableId id,session_ref;Integer generation=0;
 StableId formation_ref,mission_ref,unit_ref;WorldMoment accepted_at;
 Integer action_instance_ordinal=0;MilitaryOrder original_order;
 StableId timing_definition_ref;Integer timing_definition_version=0;
 MomentOwnerSource control_binding_source,mission_scope_source;
 MilitaryIntentAdoption3 adoption;
};
// The actual containing mission is the sole owner of accepted_intents. This
// header deliberately declares no standalone accepted-intent state/table.
}
