// Original Social field writers, borrowed by JSON and bounded restore shape.
#pragma once
#include "social_model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct SocialPeriodSources;
Status emit_social_state_fields(ProjectionWriter&,const SocialState&);
Status emit_dynamic_social_fields(ProjectionWriter&,const DynamicSocialState&);
Status emit_dynamic_social_reports_fields(ProjectionWriter&,const DynamicSocialState&,bool strict_enums=false);
Status emit_dynamic_social_known_fields(ProjectionWriter&,const DynamicSocialState&,bool strict_enums=false);
Status emit_dynamic_social_time2_fields(ProjectionWriter&,const DynamicSocialState&);
Status emit_social_period_sources_fields(ProjectionWriter&,const SocialPeriodSources&);
}
