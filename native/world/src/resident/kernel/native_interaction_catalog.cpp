#include "resident/kernel/catalog_section_observation.hpp"
#include "native_interaction_catalog.hpp"
#include "world_bundle.hpp"
#include "native_catalog_decode.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/interaction/interaction_codec.hpp"
#include "resident/domains/interaction/interaction_projection.hpp"
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include "resident/domains/interaction/interaction_exact_phase.hpp"
#include "resident/domains/interaction/visit_messages/visit_message.hpp"
#include "resident/domains/interaction/visit_replies/visit_reply.hpp"
#include "resident/domains/interaction/visit_messages/visit_message_receipt_profile.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_life.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{std::move(v.error)};}
[[noreturn]]void fail(const char*c,const StableId&p={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(c,p,kind).error};}
// 仅有限原模块的显式字符串版本；重复字段和其它版本均拒绝。
Integer version(const CatalogIdentity&id,const char*key,Integer maximum){Integer out=0;for(const auto&v:id.module_versions)if(v.first==key){if(out)fail("DuplicateInteractionModule",key);if(v.second=="1")out=1;else if(v.second=="2"&&maximum>=2)out=2;else if(v.second=="3"&&maximum>=3)out=3;else if(v.second=="4"&&maximum>=4)out=4;else if(v.second=="5"&&maximum>=5)out=5;else if(v.second=="6"&&maximum>=6)out=6;else if(v.second=="7"&&maximum>=7)out=7;else if(v.second=="8"&&maximum>=8)out=8;else if(v.second=="9"&&maximum>=9)out=9;else if(v.second=="10"&&maximum>=10)out=10;else if(v.second=="11"&&maximum>=11)out=11;else if(v.second=="12"&&maximum>=12)out=12;else if(v.second=="13"&&maximum>=13)out=13;else fail("UnsupportedInteractionModuleVersion",key,ErrorKind::unsupported_capability);}return out;}
bool has(const CatalogIdentity&id,const char*key,const char*value){for(const auto&v:id.module_versions)if(v.first==key&&v.second==value)return true;return false;}
}
Outcome<Integer> decode_native_interaction_response_selection(const BoundaryValue& scenario){try{
 const BoundaryValue* selected=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="interaction_response_sources"){
  if(selected)boundary::fail("DuplicateInteractionResponseSelection",row.first);selected=&row.second;
 }
 if(!selected)return {true,0,{}};
 boundary::fields(*selected,{"profile"},{},"interaction_response_sources");
 if(boundary::object(*selected,"interaction_response_sources").size()!=1)
  boundary::fail("InvalidInteractionResponseSelection","interaction_response_sources");
 const auto& profile=boundary::field(*selected,"profile","interaction_response_sources");
 const auto* text=profile.as<StableId>();if(!text)boundary::fail("InvalidInteractionResponseSelection","interaction_response_sources/profile");
 Integer physical=0;
 if(*text=="original_interaction_response_v5")physical=5;
 else if(*text=="original_interaction_response_v6")physical=6;
 else if(*text=="original_interaction_response_v7")physical=7;
 else if(*text=="original_interaction_response_v8")physical=8;
 else if(*text=="original_interaction_response_v9")physical=9;
 else if(*text=="original_interaction_response_v10")physical=10;
 else if(*text=="original_interaction_response_v11")physical=11;
 else if(*text=="original_interaction_response_v12")physical=12;
 else if(*text=="original_interaction_response_v13")physical=13;
 else boundary::unsupported("UnsupportedInteractionResponseSelection","interaction_response_sources/profile");
 return {true,physical,{}};
}catch(const boundary::DecodeFailure& e){return {false,0,e.error};}}
Status append_native_interaction_response_module(const BoundaryValue& scenario,CatalogIdentity& identity){
 auto selected=decode_native_interaction_response_selection(scenario);if(!selected.ok)return {false,{},std::move(selected.error)};
 for(const auto& row:identity.module_versions)if(row.first=="interaction")return common_failure("DuplicateInteractionModule",row.first);
 if(selected.value){const char* value=selected.value==5?"5":selected.value==6?"6":selected.value==7?"7":selected.value==8?"8":selected.value==9?"9":selected.value==10?"10":selected.value==11?"11":selected.value==12?"12":"13";append_native_catalog_module(identity,"interaction",value);}
 return common_success();
}
Outcome<Integer> selected_native_interaction_response_version(const CatalogIdentity&id){try{
 const auto selected=version(id,"interaction",13);return {true,interaction_response_version(selected)?selected:0,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<Integer> native_interaction_physical_version(const PreparedCatalog& c){try{
 const auto selected=take(selected_native_interaction_response_version(c.identity));
 if((c.interaction?c.interaction->response_sources_version:0)!=selected)fail("InteractionResponseCatalogSelectionMismatch");
 const Integer base=c.regional_life?(c.interaction&&!c.interaction->regional_outcome_topics.empty()?4:3):2;
 if(selected&&take(interaction_response_base_version(selected))!=base)fail("InteractionOriginalBaseVersionMismatch");
 return {true,selected?selected:base,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<InteractionState> decode_native_interaction_seed(const BoundaryValue& value,const PreparedCatalog& catalog){
 auto physical=native_interaction_physical_version(catalog);if(!physical.ok)return {false,{},std::move(physical.error)};
 ScopedCheckpointJsonDecode original_decode(CheckpointWorldJsonSection::interaction);
 return interaction_response_version(physical.value)?decode_interaction_selected_seed(value,physical.value):decode_interaction_seed(value);
}
Status visit_native_interaction_checkpoint(CodecVisitor& c,InteractionState& state,const PreparedCatalog& catalog){
 auto physical=native_interaction_physical_version(catalog);if(!physical.ok)return {false,{},std::move(physical.error)};
 return interaction_response_version(physical.value)?visit_interaction_selected_checkpoint(c,state,physical.value):visit_interaction_checkpoint(c,state,catalog.regional_life);
}
Status emit_native_interaction_state_fields(ProjectionWriter& out,const InteractionState& state,const PreparedCatalog& catalog){
 auto physical=native_interaction_physical_version(catalog);if(!physical.ok)return {false,{},std::move(physical.error)};
 return interaction_response_version(physical.value)?emit_interaction_selected_state_fields(out,state,physical.value):emit_interaction_state_fields(out,state);
}
Status validate_native_interaction_response_layout(const PreparedCatalog&c,const InteractionState&i){try{
 const auto selected=take(selected_native_interaction_response_version(c.identity));
 take(native_interaction_physical_version(c));
 if((c.interaction?c.interaction->response_sources_version:0)!=selected||
    (i.response_sources?i.response_sources->physical_version:0)!=selected)
  fail("InteractionResponseSelectedRootMismatch");
 if(selected&&(!c.exact_time||!exact_complete_selected(*c.exact_time)||!c.family_life||!c.family_life->enabled||c.family_life->selected_version!=4||!c.actor_social_occupancy_selected||version(c.identity,"actor_social_occupancy",3)!=3||!i.social_occupancy||i.social_occupancy->version!=3))
  fail("InteractionResponseOriginalLifeAndOpeningRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<NativeInteractionSectionVersions> selected_native_interaction_versions(const CatalogIdentity&id){try{
 NativeInteractionSectionVersions s{version(id,"visit_messages",3),version(id,"personal_appointments",2),version(id,"contact_inquiries",2),version(id,"visit_appointment_invitations",2),version(id,"visit_appointment_meetings",2),take(visit_message_receipt_selected_version(id.module_versions)),version(id,"visit_message_replies",4)};
 if(s.receipts==2&&s.messages<2)fail("VisitMessageReceipt2LifeSourcesRequired",{},ErrorKind::unsupported_capability);
 if(s.receipts==2&&(!has(id,"director","6")||!has(id,"director_subjects","2")))fail("VisitMessageDirectorPhysical6Required",{},ErrorKind::unsupported_capability);
 if(s.replies&&(!s.messages||(s.replies<=2?s.messages!=1:s.messages<2)))fail("VisitReplyOriginalLifeVersionRequired",{},ErrorKind::unsupported_capability);
 if(s.invitations!=s.meetings)fail("VisitAppointmentSelectedVersionMismatch");
 if(s.invitations&&(s.personal!=s.invitations||(s.invitations==1?s.messages!=1:(s.messages!=2&&s.messages!=3))))fail("VisitAppointmentOriginalProfileRequired");
 if(s.inquiry&&s.personal!=s.inquiry&&!has(id,"task_message_handoffs","1")&&!has(id,"task_message_handoffs","2")&&!has(id,"task_message_handoffs","3")&&!has(id,"task_message_handoffs","4"))fail("ContactInquiryOriginalProfileRequired");
 if(s.exact_sources()&&(!has(id,"world_timeline","2")||!has(id,"actor_start_context","3")||!has(id,"family_life","4")))fail("InteractionCompleteLife4SourcesRequired",{},ErrorKind::unsupported_capability);
 if(s.exact_actions()&&((s.personal&&s.personal!=2)||(s.inquiry&&s.inquiry!=2)||(s.meetings&&s.meetings!=2)))fail("InteractionSingleTime2ProviderRequired",{},ErrorKind::unsupported_capability);
 return {true,s,{}};
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_native_interaction_catalog(const PreparedCatalog&c){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 140, "PreparedCatalog", "validate_native_interaction_catalog", &c);
try{
 const auto s=take(selected_native_interaction_versions(c.identity));
 const auto response=take(selected_native_interaction_response_version(c.identity));
 take(native_interaction_physical_version(c));
 if((c.interaction?c.interaction->response_sources_version:0)!=response)fail("InteractionResponseCatalogSelectionMismatch");
 // Real selected-tail consumption and the TeachStudy death owner exist. The
 // dispatcher may also await other original ActionStarted results; their actual
 // death-terminal/source binding is not yet completely supplied. Do not admit
 // that gap merely from a Life4/occupancy3 marker or today's Action state.
 if(response)fail("InteractionWaitingActionDeathSourceCoverageRequired",{},ErrorKind::unsupported_capability);
 if(!c.interaction){if(s.messages||s.personal||s.inquiry||s.invitations||s.meetings||s.receipts||s.replies)fail("InteractionOriginalCatalogRequired",{},ErrorKind::unsupported_capability);return common_success();}
 if(bool(s.receipts)!=c.visit_message_receipts_enabled)fail("VisitMessageReceiptProfileMismatch");
 // 固定 S3 的附属段各有原生命/认领来源；旧回复 1/2 不能因主段 2/3 自动升级。
 if(s.messages>=2&&(c.visit_message_replies||c.visit_message_receipts_enabled||(c.director&&c.director->visit_reception)))fail("VisitMessageCompleteAdjunctOwnersRequired",{},ErrorKind::unsupported_capability);
 if((c.interaction->visit_messages?c.interaction->visit_messages->version:0)!=s.messages||(c.personal_appointments?c.personal_appointments->version:0)!=s.personal||(c.contact_inquiries?c.contact_inquiries->version:0)!=s.inquiry||(c.visit_appointment_invitations?c.visit_appointment_invitations->version:0)!=s.invitations||(c.visit_message_replies?c.visit_message_replies->version:0)!=s.replies)fail("InteractionCatalogModuleMismatch");
 if(s.exact_sources()&&(!c.exact_time||!exact_complete_selected(*c.exact_time)||!c.family_life||!c.family_life->enabled||c.family_life->selected_version!=4||!c.actor_foundation))fail("InteractionCompleteOriginalCatalogRequired",{},ErrorKind::unsupported_capability);
 if(c.exact_time&&exact_complete_selected(*c.exact_time)){
  const ExactPhaseSelection* selected=nullptr;for(const auto&p:c.exact_time->complete->phase_providers)if(p.provider==ExactProviderId::interaction){if(selected)fail("DuplicateInteractionPhaseSelection");selected=&p;}
  if(s.exact_actions()){if(!selected||selected->profile_id!="interaction_time2"||selected->version!=2)fail("InteractionExactPhaseSelectionRequired",{},ErrorKind::unsupported_capability);}
  else if(selected&&selected->profile_id=="interaction_time2")fail("InteractionExactOriginalSelectionRequired",{},ErrorKind::unsupported_capability);
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_native_interaction_layout(const PreparedCatalog&c,const InteractionState&i){try{
 take(validate_native_interaction_catalog(c));const auto s=take(selected_native_interaction_versions(c.identity));
 take(validate_visit_message_receipt_layout(c.identity.module_versions,c.visit_message_receipts_enabled,c.interaction&&bool(c.interaction->visit_messages),i.visit_message_receipts?&*i.visit_message_receipts:nullptr));
 if((i.visit_messages?i.visit_messages->version:0)!=s.messages||(i.personal_appointments?i.personal_appointments->version:0)!=s.personal||(i.contact_inquiries?i.contact_inquiries->version:0)!=s.inquiry||(i.visit_appointment_invitations?i.visit_appointment_invitations->version:0)!=s.invitations||(i.visit_appointment_meetings?i.visit_appointment_meetings->version:0)!=s.meetings||(i.visit_message_replies?i.visit_message_replies->version:0)!=s.replies)fail("InteractionStateSelectedVersionMismatch");
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status initialize_native_interaction_roots(const PreparedCatalog&c,InteractionState&i){try{
 take(validate_native_interaction_catalog(c));
 if(i.visit_messages||i.personal_appointments||i.contact_inquiries||i.visit_appointment_invitations||i.visit_appointment_meetings||i.visit_message_receipts||i.visit_message_replies||i.response_sources)fail("InteractionFreshStateRequired");
 if(!c.interaction)return validate_native_interaction_layout(c,i);
 if(c.interaction->response_sources_version)i.response_sources=take(make_initial_interaction_response_sources(c.interaction->response_sources_version));
 if(c.interaction->visit_messages)i.visit_messages=take(make_initial_visit_message_state(*c.interaction->visit_messages,*c.interaction));
 if(c.visit_message_replies){if(!c.interaction->visit_messages)fail("VisitReplyOriginalProfileRequired");i.visit_message_replies=take(make_initial_visit_reply_state(*c.visit_message_replies,*c.interaction->visit_messages,*c.interaction));}
 if(c.visit_message_receipts_enabled){if(!c.interaction->visit_messages)fail("VisitMessageReceiptOriginalRequired");i.visit_message_receipts=take(make_initial_visit_message_receipt_state(c.identity,*c.interaction->visit_messages));}
 if(c.personal_appointments)i.personal_appointments=take(make_initial_personal_appointment_state(*c.personal_appointments,*c.interaction));
 if(c.contact_inquiries)i.contact_inquiries=take(make_initial_contact_inquiry_state(*c.contact_inquiries,*c.interaction));
 if(c.visit_appointment_invitations){if(!c.interaction->visit_messages||!c.personal_appointments)fail("VisitAppointmentOriginalCatalogRequired");i.visit_appointment_invitations=take(make_initial_visit_appointment_state(*c.visit_appointment_invitations,*c.interaction->visit_messages,*c.personal_appointments,*c.interaction,c.visit_message_replies.get()));i.visit_appointment_meetings=take(make_initial_visit_appointment_meeting_state(*c.visit_appointment_invitations));}
 return validate_native_interaction_layout(c,i);
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
