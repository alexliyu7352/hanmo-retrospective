// 原关系/人生来源的同JSON字段出口；结构门与原编码器共用。
#pragma once
#include "family_apprenticeship.hpp"
#include "voluntary_path_state.hpp"
#include "collateral_control_state.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_apprenticeship_statement_source_fields(ProjectionWriter&,const ApprenticeshipStatementSource&);
Status emit_apprenticeship_proposal_fields(ProjectionWriter&,const ApprenticeshipProposal&);
Status emit_apprenticeship_reply_fields(ProjectionWriter&,const ApprenticeshipReply&);
Status emit_apprenticeship_exit_fields(ProjectionWriter&,const ApprenticeshipExit&);
Status emit_apprenticeship_commit_fields(ProjectionWriter&,const ApprenticeshipCommitBinding&);
Status emit_family_apprenticeship_fields(ProjectionWriter&,const FamilyApprenticeshipState&);
Status emit_biological_control_origin_fields(ProjectionWriter&,const BiologicalControlOrigin&);
Status validate_biological_control_origin_shape(const BiologicalControlOrigin&);
Status emit_collateral_family_origin_fields(ProjectionWriter&,const CollateralFamilyOrigin&);
Status emit_collateral_recognition_source_fields(ProjectionWriter&,const CollateralRecognitionSource&);
Status emit_voluntary_path_source_fields(ProjectionWriter&,const VoluntaryPathSource&);
Status emit_voluntary_path_cancellation_fields(ProjectionWriter&,const VoluntaryPathCancellation&);
Status emit_voluntary_path_commit_fields(ProjectionWriter&,const VoluntaryPathCommitBinding&);
Status emit_voluntary_path_state_fields(ProjectionWriter&,const VoluntaryPathState&);
}
