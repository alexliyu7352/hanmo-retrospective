#include "social_relation_retention.hpp"
namespace hanmo::resident {
Status visit_original_social_relation_history_pins(const OriginalSocialRelationContext&c,OriginalSocialRelationPinSink&sink){
 auto valid=validate_original_social_relations(c);if(!valid.ok)return valid;
 for(const auto&row:c.social.relation_results.rows()){
  const auto&r=row.second;const SourceRef source{"social","relation_applied",r.result_id};
  valid=sink.actual_pin({{"social","relation_record",r.relation_record_key,"original_applications"},source,SourcePinReason::dependent_state},1);if(!valid.ok)return valid;
  auto business=original_social_relation_business_source(r.original_cause);if(!business.ok)return {false,{},business.error};
  const auto&b=business.value;
  valid=sink.actual_pin({{"social","relation_applied",r.result_id,"original_cause"},{std::string(b.domain),std::string(b.kind),*b.id},SourcePinReason::dependent_state},b.schema);if(!valid.ok)return valid;
 }
 for(const auto&row:c.social.events.rows()){
  const auto&e=row.second;if(!e.original_application)return common_failure("OriginalSocialEventApplicationRequired",row.first,ErrorKind::internal);
  valid=sink.actual_pin({{"social","social_event",row.first,"actual_application"},{"social","social_event_applied",row.first},SourcePinReason::dependent_state},1);if(!valid.ok)return valid;
  const auto&period=e.original_application->original_period_opening;valid=sink.actual_pin({{"social","social_event_applied",row.first,"original_period_opening"},period.source_ref,SourcePinReason::dependent_state},period.source_schema);if(!valid.ok)return valid;
  if(e.kind=="news"&&c.social.events.find(e.fact_id)){valid=sink.actual_pin({{"social","social_event_applied",row.first,"original_news_event"},{"social","social_event_applied",e.fact_id},SourcePinReason::dependent_state},1);if(!valid.ok)return valid;}
 }
 return common_success();
}
}
