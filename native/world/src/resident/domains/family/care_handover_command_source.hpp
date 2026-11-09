// The actual original Common command/result and player/NPC route, not S0 proof.
#pragma once
#include "care_handover_execution.hpp"
#include <string_view>
namespace hanmo::resident {
struct DirectorState;struct DirectorCatalog;
struct CareHandoverCommandSourceContext {
 const CareHandoverSourceContext& handovers;
 const DirectorState* director=nullptr;const DirectorCatalog* director_catalog=nullptr;
};
// Only called after the real successful Common result/history and, for an NPC,
// original Director adoption have been installed in this same candidate.
Outcome<LifeActorRoute> capture_care_handover_command_route(
 const CareHandoverCommandSourceContext&,const StableId& actual_boundary_id);
Status validate_care_handover_command_source(const CareHandoverCommandSourceContext&,
 const CareHandoverExecution&,const CareHandoverExecutionBoundary1&);
struct CareHandoverStartedBusinessIdentity {
 const StableId* actor_id=nullptr;const StableId* action_id=nullptr;
 const LifeActualCommandBinding* command_binding=nullptr;
 const WorldMoment* started_at=nullptr;
};
// Borrowed original source identity for sealed-command cold readers. It does
// not call its own Director route or S0/FSL validation.
Outcome<CareHandoverStartedBusinessIdentity> read_care_handover_started_business_identity(
 const CareHandoverCommandSourceContext&,const MomentOwnerSource&);
struct CareHandoverCommandResultReference {
 std::string_view handover_id,boundary_id,actor_id,command_kind,result_id;
 Integer request_seq=0,boundary_ordinal=0;
 // The original Director adoption key is borrowed, never re-mapped to a fake fact.
 const LifeNpcActorRoute* original_npc_route=nullptr;
};
class CareHandoverCommandResultSink {
public:virtual ~CareHandoverCommandResultSink()=default;
 virtual Status actual_reference(const CareHandoverCommandResultReference&)=0;
};
Status visit_care_handover_command_result_references(
 const CareHandoverCommandSourceContext&,CareHandoverCommandResultSink&);
}
