#pragma once
#include "social_model.hpp"
#include "social_period_sources.hpp"
namespace hanmo::resident {
struct ExactPeriodContext;
struct OriginalSocialEventSources {
 const CommonStore& common;const SocialState& social;
 const SocialPeriodSources& periods;const CatalogIdentity& identity;
};
Status validate_social_event_original_application_shape(const SocialEventOriginalApplication&);
Status validate_original_social_event_period(const OriginalSocialEventSources&,
 const ExactPeriodContext&,const WorldMoment&);
// Source-local original payload/header/Calendar identity. No relation fold,
// dynamic knowledge validator, or whole-period consumption callback.
Outcome<const SocialEvent*> read_original_social_calendar_event(
 const OriginalSocialEventSources&,const SourceRef&,Integer schema,const WorldMoment& actual_now);
Status validate_original_social_event_coverage(const OriginalSocialEventSources&,const WorldMoment&);
Status bind_original_social_calendar_event_in_candidate(CommonStore&,SocialState&,
 const OriginalSocialEventSources&,const StableId& event_id,const ExactPeriodContext&,
 const WorldMoment& actual_now);
}
