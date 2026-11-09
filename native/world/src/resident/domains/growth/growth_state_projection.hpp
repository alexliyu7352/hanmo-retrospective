// Original Growth JSON export and capacity shape use the same finite emitters.
#pragma once
#include "growth_result_ref_fields.hpp"
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_growth_command_fields(ProjectionWriter&,const GrowthCommand&);
Status emit_growth_trade_terms_fields(ProjectionWriter&,const TradeTerms&);
Status emit_growth_teaching_agreement_fields(ProjectionWriter&,const GrowthTeachingAgreement&);
Status emit_growth_study_teaching_agreement_fields(ProjectionWriter&,const GrowthStudyTeachingAgreement&);
Status emit_growth_cards_fields(ProjectionWriter&,const GrowthCardsState&);
Status emit_innate_study_source_fields(ProjectionWriter&,const InnateStudySource&);
Status emit_growth_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,bool regional_life,bool cards,bool teaching_requests,bool study_teaching_requests,bool innate_learning,bool fixed_service=false,bool complete=false,bool exact_periods=false,bool exact_command_cuts=false,bool exact_rest_mouths=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
// Catalog-aware counterparts preserve the original encode entry selection gates.
Status emit_growth_complete_fixed_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
Status emit_growth_exact_period_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
Status emit_growth_exact_command_cut_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
Status emit_growth_exact_rest_mouth_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
Status emit_growth_capability_chain_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
// Call the whole-state same emitter first for original cross-record selection gates.
// These row emitters then merge into the actual Common header order without trees.
Status emit_growth_action_payload_fields(ProjectionWriter&,const GrowthState&,const ActionCoreView&);
Status emit_growth_fact_payload_fields(ProjectionWriter&,const GrowthState&,const FactRecord&,bool fixed_service,bool complete,bool exact_periods=false,bool exact_command_cuts=false,bool exact_rest_mouths=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Status emit_growth_additional_state_fields(ProjectionWriter&,const GrowthState&,bool cards,bool fixed_service,bool complete,bool exact_periods=false,bool exact_command_cuts=false,bool exact_rest_mouths=false,bool capability_chain=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
namespace growth {
Status emit_growth_frozen_fields(ProjectionWriter&,const ChallengeFrozen&);
Status emit_growth_challenge_fields(ProjectionWriter&,const GrowthChallengeInstance&);
Status emit_growth_challenge_state_fields(ProjectionWriter&,const GrowthChallengeInstance&);
Status emit_growth_challenge_seed_fields(ProjectionWriter&,const GrowthChallengeInstance&);
Status emit_growth_challenge_context_fields(ProjectionWriter&,const StudyChallengeContext&);
Status emit_growth_bargain_fact_fields(ProjectionWriter&,const BargainFact&);
}
}

namespace hanmo::resident {
Status emit_growth_trade_grant_state_projection(ProjectionWriter&,std::string_view,const GrowthState&,const GrowthCatalog&);
}
