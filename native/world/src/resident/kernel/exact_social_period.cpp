// 原S5初始化实际producer与原Social渠道writer闭合；不运行Director复评、整旧finish或新S5采集。
#include "exact_social_period.hpp"
#include "world_bundle.hpp"
#include "exact_time_world.hpp"
#include <set>
#include <limits>
namespace hanmo::resident {
namespace {
Status reject(const char* code,const StableId& path={}){return common_failure(code,path,ErrorKind::unsupported_capability);}
// 唯一作者订阅输入只投影原collect实际读取的subjects/report_source_keys；没有模板或世界状态。
DirectorCatalog report_routes(const ExactTimeProfile& profile){DirectorCatalog routes;for(const auto& subscription:profile.supply_report_subscriptions){DirectorPolicy policy;policy.report_source_keys=subscription.report_source_keys;routes.subjects.insert(subscription.actor_id,std::move(policy));}return routes;}
Integer initial_tick(const WorldBundle& w){return w.catalog->actor_foundation->actors.rows().front().second.scenario_origin_ref.initial_tick;}
}
Status validate_exact_social_source_scope(const WorldBundle& w){
 if(!w.catalog->exact_time||!exact_social_selected(*w.catalog->exact_time))return reject("ExactSocialProfileRequired");if(!w.catalog->actor_foundation||w.catalog->actor_foundation->actors.empty()||!w.catalog->local_economy||!w.catalog->organization||!w.social_period_sources||!w.social.dynamic||w.social.dynamic->version!=4||!w.social.dynamic->time2)return common_failure("ExactSocialSourcePairRequired");const auto initial=initial_tick(w);const auto& ec=*w.catalog->local_economy;const auto& oc=*w.catalog->organization;
 if(ec.supply_sources.empty()||oc.organizations.empty()||w.organization.organizations.size()!=oc.organizations.size())return reject("ExactSocialInitialSourcesRequired");for(const auto& row:oc.organizations.rows()){const auto& definition=row.second;const auto* actual=w.organization.organizations.find(row.first);if(!actual||definition.status!=OrganizationStatus::active||definition.administrative_parent||definition.branch_parent||!definition.office_ids.empty()||!definition.policy_profile_id.empty()||!definition.succession_policy_id.empty()||actual->id!=definition.id||actual->kind!=definition.kind||actual->status!=definition.status||actual->administrative_parent!=definition.administrative_parent||actual->branch_parent!=definition.branch_parent||actual->headquarters_site!=definition.headquarters_site||actual->policy_profile_id!=definition.policy_profile_id||actual->office_ids!=definition.office_ids||actual->succession_policy_id!=definition.succession_policy_id)return reject("ExactSocialOrganizationNotStatic",row.first);}
 // 本首链显式只选公开初始S5源；未订阅公开源仍按原collector形成组织自知Report，不能开放私密支线。
 for(const auto& row:ec.supply_sources.rows())if(!row.second.public_report)return reject("ExactSupplyPublicSourceRequired",row.first);
 if(w.local_economy.supply_registrations.size()!=ec.supply_sources.size()||w.local_economy.supply_observations.size()!=ec.supply_sources.size())return common_failure("ExactInitialSupplySourceCoverage");std::set<StableId> registered,observed;for(const auto& row:w.local_economy.supply_registrations.rows()){const auto& r=row.second;const auto* definition=ec.supply_sources.find(r.source_key);const auto* header=w.common.state().facts.find(row.first);if(!definition||r.id!=row.first||!header||header->domain!="local_economy"||header->kind!="local_supply_source"||r.tick!=initial||r.organization_id!=definition->organization_id||r.site_id!=definition->site_id||r.inventory_id!=definition->inventory_id||r.item_id!=definition->item_id||!registered.insert(r.source_key).second)return common_failure("InvalidExactInitialSupplyRegistration",row.first);}
 for(const auto& row:w.local_economy.supply_observations.rows()){const auto& r=row.second;const auto& o=r.observation;const auto* definition=ec.supply_sources.find(o.source_key);const auto* header=w.common.state().facts.find(row.first);const auto* inventory=w.common.inventory(o.inventory_id);if(!definition||r.id!=row.first||o.source_fact_id!=row.first||!header||header->domain!="local_economy"||header->kind!="local_supply_observation"||r.tick!=initial||o.organization_id!=definition->organization_id||o.site_id!=definition->site_id||o.inventory_id!=definition->inventory_id||o.item_id!=definition->item_id||o.target_amount!=definition->target_amount||r.publicly_visible!=definition->public_report||!inventory||inventory->revision!=o.source_revision||!observed.insert(o.source_key).second)return common_failure("InvalidExactInitialSupplyObservation",row.first);}
 auto current=observe_local_supply(w.common,w.local_economy,ec);if(!current.ok)return {false,{},current.error};if(current.value.size()!=ec.supply_sources.size())return common_failure("ExactSupplySourceUnavailable");
 std::set<StableId> actors;for(const auto& subscription:w.catalog->exact_time->supply_report_subscriptions){if(!actors.insert(subscription.actor_id).second||!w.catalog->actor_foundation->actors.find(subscription.actor_id)||!w.common.actor(subscription.actor_id)||subscription.report_source_keys.empty()||subscription.report_source_keys.size()>64)return reject("InvalidExactSupplySubscriber",subscription.actor_id);std::set<StableId> keys;for(const auto& key:subscription.report_source_keys){const auto* source=ec.supply_sources.find(key);if(!source||!source->public_report||!keys.insert(key).second)return reject("InvalidExactSupplySubscription",key);}}
 return common_success();
}
// Complete新局消费已经存在的原事实，只产原Report与真参与者自知，不造订阅或观察M。
static Status bootstrap_complete_original_reports(WorldBundle& w){
 if(!w.catalog||!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time))return common_success();
 if(!w.catalog->actor_foundation||w.catalog->actor_foundation->actors.empty()||
    !w.world_timeline||!w.social.dynamic||!w.social.dynamic->time2)
   return common_failure("CompleteInitialReportSourcesRequired");
 const auto& state=*w.social.dynamic;
 if(w.common.revision()!=0||w.common.tick()!=initial_tick(w)||
    state.last_collected_tick!=-1||!state.reports.empty()||
    !state.pending_messages.empty()||!state.known_reports.empty()||
    !state.time2->source_time_bindings.empty()||
    state.time2->collection_cursor.last_scanned_fact_id)
   return common_failure("CompleteReportsAlreadyBootstrapped");
 if(!w.catalog->exact_time->supply_report_subscriptions.empty())
   return common_failure("CompleteInitialSubscriptionsNotSelected");
 auto now=w.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 std::unique_ptr<PhaseCurrentReadScope> phase;
 if(!w.has_active_read_phase()){
   auto entered=w.enter_phase_read_context("complete_original_reports","social",{now.value,now.value},now.value);
   if(!entered.ok)return {false,{},std::move(entered.error)};
   phase=std::move(entered.value);
 }
 {
   auto source=capture_exact_result_time_read(w);if(!source.ok)return {false,{},std::move(source.error)};
   auto collected=collect_exact_dynamic_reports(w.common,w.social,*source.value,source.value->stamp(),now.value,w.catalog->identity);
   if(!collected.ok)return collected;
 }
 w.note_write_boundary();
 return common_success();
}
Status bootstrap_exact_social_in_candidate(WorldBundle& w){
 if(w.catalog&&w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time))return bootstrap_complete_original_reports(w);
 if(!w.catalog->exact_time||!exact_social_selected(*w.catalog->exact_time))return common_success();auto scope=validate_exact_rest_world_scope(w);if(!scope.ok)return scope;if(w.common.revision()!=0||w.common.tick()!=initial_tick(w)||!w.social.dynamic||!w.social.dynamic->time2||w.social.dynamic->last_collected_tick!=-1||!w.social.dynamic->reports.empty()||!w.social.dynamic->pending_messages.empty()||!w.social.dynamic->known_reports.empty()||!w.social.dynamic->time2->source_time_bindings.empty()||!w.social_period_sources->openings.empty())return common_failure("ExactSocialAlreadyBootstrapped");auto source=capture_exact_result_time_read(w);if(!source.ok)return {false,{},source.error};auto routes=report_routes(*w.catalog->exact_time);return collect_exact_supply_reports(w.common,w.social,*source.value,source.value->stamp(),source.value->source_moment(),w.catalog->identity,&routes);
}
Status collect_exact_social_in_candidate(WorldBundle& w){
 if(!w.catalog->exact_time||!exact_social_selected(*w.catalog->exact_time))return common_success();auto scope=validate_exact_social_source_scope(w);if(!scope.ok)return scope;auto source=capture_exact_result_time_read(w);if(!source.ok)return {false,{},source.error};auto before=w.social.dynamic->reports.size(),messages=w.social.dynamic->pending_messages.size();auto result=collect_exact_supply_reports(w.common,w.social,*source.value,source.value->stamp(),source.value->source_moment(),w.catalog->identity,nullptr);if(!result.ok)return result;if(w.social.dynamic->reports.size()!=before||w.social.dynamic->pending_messages.size()!=messages)return common_failure("ExactSocialBootstrapWriterClosed");return common_success();
}
Status validate_exact_social_world(const WorldBundle& w){
 auto scope=validate_exact_social_source_scope(w);if(!scope.ok)return scope;auto source=capture_exact_result_time_read(w);if(!source.ok)return {false,{},source.error};auto valid=validate_social_time2(w.common,w.social,*source.value,source.value->stamp(),w.catalog->identity);if(!valid.ok)return valid;const auto& d=*w.social.dynamic;const auto initial=initial_tick(w);if(initial==std::numeric_limits<Integer>::max())return common_failure("ExactSupplyInitialTickOverflow");
 // 每个真实S5 observation恰一Report；每条作者订阅恰一原pending message，不靠删整行逃覆盖。
 if(d.reports.size()!=w.local_economy.supply_observations.size())return common_failure("ExactSupplyReportCoverage");std::set<StableId> messages;std::set<StableId> known_keys;for(const auto& row:w.local_economy.supply_observations.rows()){const auto report_id="report:"+row.first;const auto* report=d.reports.find(report_id);if(!report||report->fact_id!=row.first)return common_failure("MissingExactSupplyReport",row.first);for(const auto& participant:report->observation.participants)known_keys.insert(social_time_key(SocialKnowledgeKey{participant.subject,report_id}));for(const auto& subscription:w.catalog->exact_time->supply_report_subscriptions){if(!row.second.publicly_visible||std::find(subscription.report_source_keys.begin(),subscription.report_source_keys.end(),row.second.observation.source_key)==subscription.report_source_keys.end())continue;const PendingMessage* message=nullptr;for(const auto& candidate:d.pending_messages.rows())if(candidate.second.report_id==report_id&&same_social_subject(candidate.second.receiver,{SubjectKind::actor,subscription.actor_id})){if(message)return common_failure("DuplicateExactSupplyMessage",report_id);message=&candidate.second;}if(!message||message->channel!="official_supply_report"||message->created_tick!=initial||message->due_tick!=initial+1||message->local_place_id||message->address_binding||!same_social_subject(message->sender,report->origin_subject)||!messages.insert(message->id).second)return common_failure("MissingExactSupplyMessage",report_id);const auto* created_binding=d.time2->source_time_bindings.find(social_time_key(MessageCreatedKey{message->id}));const auto* created=created_binding?std::get_if<SocialExactMoment>(&created_binding->time_source):nullptr;if(!created)return common_failure("MissingBootstrapMessageMoment",message->id);auto initial_created=compare_world_moments(created->moment,WorldMoment{initial,{}});if(!initial_created.ok)return {false,{},initial_created.error};if(initial_created.value)return common_failure("InvalidBootstrapMessageMoment",message->id);if(message->status==MessageStatus::delivered)known_keys.insert(social_time_key(SocialKnowledgeKey{message->receiver,report_id}));}}
 if(messages.size()!=d.pending_messages.size())return common_failure("UnexpectedExactSupplyMessage");std::size_t known_count=0;for(const auto& receiver:d.known_reports.rows())for(const auto& row:receiver.second.rows()){const auto key=social_time_key(SocialKnowledgeKey{row.second.receiver,row.first});if(!known_keys.count(key))return common_failure("UnselectedExactKnowledgeWriter",row.first);++known_count;if(!row.second.source_message_id){const auto* binding=d.time2->source_time_bindings.find(key);const auto* at=binding?std::get_if<SocialExactMoment>(&binding->time_source):nullptr;if(!at||at->moment.half_day_tick!=initial)return common_failure("InvalidBootstrapKnowledgeMoment",row.first);auto zero=compare_fractions(at->moment.sub_half_day,CanonicalFraction{});if(!zero.ok)return {false,{},zero.error};if(zero.value!=0)return common_failure("InvalidBootstrapKnowledgeMoment",row.first);}}if(known_count!=known_keys.size())return common_failure("IncompleteExactSupplyKnowledge");
 // 实际内部knowledge consumer用同一typed reader；不从公开Report或当地可见性制造已知。
 auto reader=capture_exact_social_report_read(w.common,w.social,source.value,w.catalog->identity);if(!reader.ok)return {false,{},reader.error};for(const auto& row:d.pending_messages.rows()){auto receipt=reader.value->knowledge_receipt_v2(row.second.receiver,row.second.report_id,source.value->stamp());if(!receipt.ok)return {false,{},receipt.error};if((row.second.status==MessageStatus::delivered)!=receipt.value.has_value())return common_failure("ExactKnowledgeReceiptCoverage",row.first);}
 return validate_social_due_periods(w.common,w.social,*w.social_period_sources,*w.world_timeline,w.catalog->identity,initial);
}
}
