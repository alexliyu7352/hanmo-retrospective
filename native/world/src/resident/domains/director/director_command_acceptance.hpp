// Actual Common command identity joined to the original adopted decision.
#pragma once
#include "director_subject2.hpp"
#include "hanmo/resident/command_contract.hpp"
namespace hanmo::resident {
struct DirectorAcceptedCommandKey {
 StableId actor_id;Integer registration_epoch=0,decision_seq=0;
 StableId command_kind;Integer request_seq=0;StableId result_id;
};
struct DirectorAcceptedCommandView {
 const DirectorAdoptionResult2* decision=nullptr;
 const DirectorAcceptedCommand* command=nullptr;
 const CommitResult* actual_result=nullptr;
};
// The result must already be the successful original Common result at actor:seq
// and have its original history row. This routine never allocates result IDs.
Outcome<DirectorAcceptedCommand> capture_director_accepted_command(
 const CommonStore&,const DirectorCatalog&,const CatalogIdentity&,const StableId& policy_id,
 const StableId& actor,Integer actual_request_seq,const CommitResult&,const MomentOwnerSource& actual_effect,const WorldMoment&);
// Narrow historical join; callers retain their own original business payload
// validation. No current admission, today's policy selection, or player is used.
Outcome<DirectorAcceptedCommandView> read_director_accepted_command(
 const CommonStore&,const DirectorState&,const DirectorCatalog&,const CatalogIdentity&,const DirectorAcceptedCommandKey&);
Status validate_director_accepted_commands(const CommonStore&,const DirectorState&,const DirectorCatalog&,const CatalogIdentity&);
// Called after the real executor commit and before the original adoption append.
Status validate_director_command_application(const CommonStore&,const DirectorCatalog&,const SubjectDecisionProposal2&,const SubjectDecisionApplied2&,const WorldMoment&);
}
