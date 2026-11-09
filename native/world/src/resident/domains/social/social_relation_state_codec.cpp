#include "social_relation_state_codec.hpp"
#include "social_event_original_source.hpp"
namespace hanmo::resident {
Status validate_social_relation_state_selection(const SocialState&s,Integer version){
 if(version!=1&&version!=2)return common_failure("UnsupportedOriginalSocialVersion",{},ErrorKind::unsupported_capability);
 if(s.original_relation_version!=version||(version==1&&!s.relation_results.empty()))return common_failure("OriginalSocialSelectedStateMismatch",{},ErrorKind::internal);
 for(const auto&row:s.events.rows()){const auto&app=row.second.original_application;if(version==1?bool(app):!app)return common_failure("OriginalSocialEventSelectedStateMismatch",row.first,ErrorKind::internal);if(app){auto valid=validate_social_event_original_application_shape(*app);if(!valid.ok)return valid;}}
 for(const auto&row:s.relation_results.rows()){if(row.first!=row.second.result_id)return common_failure("OriginalRelationResultKeyMismatch",row.first,ErrorKind::internal);auto valid=validate_social_relation_result_shape(row.second);if(!valid.ok)return valid;}
 return common_success();
}
}
