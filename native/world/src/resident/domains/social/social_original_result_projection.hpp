#pragma once
#include "social_relation_execution.hpp"
#include "social_event_original_source.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
// P10 supporting-only: these actual application headers never create a Report,
// observation, direct observer, or a second relation/evaluation event.
bool original_social_supporting_fact_kind(const FactRecord&)noexcept;
Outcome<WorldMoment> read_original_social_supporting_source_moment(
 const OriginalSocialRelationContext&,const OriginalSocialEventSources&,
 const MomentOwnerSource&);
Outcome<ExactOwnerFactRead> read_original_social_supporting_fact(
 const OriginalSocialRelationContext&,const OriginalSocialEventSources&,
 const FactRecord& actual_header);
// This adapter borrows the caller's original contexts, including actual_now.
// Its lifetime is confined to that actual P10 capture/read call.
class OriginalSocialResultRead final:public ExactResultOwnerRead {
 const OriginalSocialRelationContext&relations_;
 const OriginalSocialEventSources&events_;
public:
 OriginalSocialResultRead(const OriginalSocialRelationContext&r,const OriginalSocialEventSources&e)
  :relations_(r),events_(e){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&h)const override{
  return read_original_social_supporting_fact(relations_,events_,h);
 }
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&s)const override{
  return read_original_social_supporting_source_moment(relations_,events_,s);
 }
};
}
