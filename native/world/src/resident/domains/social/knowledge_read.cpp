#include "knowledge_read.hpp"
#include "resident/domains/director/director_subject2.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T>Outcome<T> fail(const char* code,ErrorKind kind=ErrorKind::internal){return {false,{},common_failure(code,{},kind).error};}
StableId part(const StableId& s){return std::to_string(s.size())+":"+s;}
StableId subject(const SubjectRef& s){return std::to_string(static_cast<Integer>(s.kind))+":"+part(s.id);}
StableId query_key(const KnowledgeQuery& q){return std::visit([](const auto& v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,ExactKnownReport>)return "r"+part(v.report_id);else if constexpr(std::is_same_v<T,ExactKnownOffer>)return "o"+std::to_string(static_cast<Integer>(v.offer.kind))+part(v.offer.id);else if constexpr(std::is_same_v<T,KnownContact>)return "c"+part(v.subject_actor_id);else if constexpr(std::is_same_v<T,ExactLegacyMemory>)return "m"+part(v.fact_id);else return "l"+std::to_string(static_cast<Integer>(v.category))+(v.known_subject_filter?subject(*v.known_subject_filter):"-");},q);}
// 沿用世界身份的小写十六进制编码；保留身份与查询边界，避免游标跨世界复用。
StableId cursor_prefix(const WorldIdentity &i, const KnowledgePageRequest &q) {
  StableId world_id;
  world_id.reserve(i.world_id.size() * 2);
  constexpr char digits[] = "0123456789abcdef";
  for (const auto byte : i.world_id) {
    world_id.push_back(digits[byte >> 4]);
    world_id.push_back(digits[byte & 15]);
  }
  return "KQ1:" + part(world_id) + ":" + std::to_string(i.execution_epoch) +
         ":" + std::to_string(i.revision) + ":" + subject(q.context.observer) +
         ":" + std::to_string(static_cast<Integer>(q.context.scope)) + ":" +
         part(query_key(q.query)) + ":" + std::to_string(q.limit) + ":" +
         (q.context.acting_authority
              ? part(q.context.acting_authority->office_id) + ":" +
                    std::to_string(q.context.acting_authority->authority_epoch)
              : "-") +
         ":";
}
KnowledgeEntry unknown(const KnowledgeQuery& q){KnowledgeEntry e;e.lookup_key=q;return e;}
Status authorize(const KnowledgeReadAccess& a,const KnowledgeQueryContext& q){
 const auto& id=a.common.state().identity;if(id.world_id!=q.expected_identity.world_id||id.execution_epoch!=q.expected_identity.execution_epoch)return common_failure("StaleKnowledgeWorld",{},ErrorKind::rule);
 if(!q.access_actor_id||*q.access_actor_id!=a.caller.actor_id||a.caller.actor_id.empty())return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);
 if(a.caller.kind==KnowledgeCaller::Kind::controlled_actor){if(a.caller.actor_id!=a.common.state().metadata.player_actor_id)return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);}
 else{if(!a.caller.director)return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);bool active=director_subject_active(*a.caller.director,a.caller.actor_id);if(a.caller.director->subject2){const auto*r=a.caller.director->subject2->registrations.find(director_subject_key2({SubjectKind::actor,a.caller.actor_id}));active=r&&r->lifecycle==DirectorLifecycle::active;}if(!active)return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);}
 if(q.scope==KnowledgeScope::institutional_known){if(q.observer.kind!=SubjectKind::organization||!q.acting_authority||q.acting_authority->actor_id!=a.caller.actor_id||q.acting_authority->organization_id!=q.observer.id)return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);auto proof=a.organizations.authorize(*q.acting_authority,"read_institutional_knowledge",{ScopeRef::Kind::activity,"knowledge"},0);if(!proof.ok)return {false,{},proof.error};if(proof.value.candidate_revision!=a.common.revision())return common_failure("StaleKnowledgeAuthority",{},ErrorKind::rule);}
 else if(q.observer.kind==SubjectKind::cohort)return common_failure("CohortKnowledgeAdapterRequired",{},ErrorKind::unsupported_capability);
 else if(q.observer.kind!=SubjectKind::actor||q.observer.id!=a.caller.actor_id)return common_failure("KnowledgeAccessDenied",{},ErrorKind::rule);
 return common_success();
}
Outcome<KnowledgeEntry> report_entry(const KnowledgeReadAccess& a,const KnowledgePageRequest& q,const StableId& id){
 auto e=unknown(ExactKnownReport{id});if(!a.social.dynamic)return {true,std::move(e),{}};const auto& d=*a.social.dynamic;
 // Search only the observer's permitted index first; do not reveal whether an unknown report exists.
 const auto* k=find_social_knowledge(d,{q.context.observer,id});if(!k)return {true,std::move(e),{}};const auto* r=d.reports.find(id);if(!r)return fail<KnowledgeEntry>("KnowledgeReportSourceMissing");
 if(q.context.scope==KnowledgeScope::local_disclosed){if(k->source_message_id){const auto* m=d.pending_messages.find(*k->source_message_id);if(!m)return fail<KnowledgeEntry>("KnowledgeMessageSourceMissing");if(!m->local_place_id)return {true,std::move(e),{}};}else return {true,std::move(e),{}};}
 e.state=r->valid_until_tick&&a.common.tick()>*r->valid_until_tick?KnowledgeState::remembered:KnowledgeState::known;e.observed_tick=r->observation_tick;e.learned_tick=k->learned_tick;e.valid_until_tick=r->valid_until_tick;e.reliability=r->reliability_kind;e.source=KnowledgeSourceRef{KnowledgeSourceRef::Kind::known_report,id,k->source_subject};
 if(d.time2){if(!a.exact_results)return fail<KnowledgeEntry>("ExactKnowledgeReaderRequired",ErrorKind::unsupported_capability);const auto* time=d.time2->source_time_bindings.find(social_time_key(SocialKnowledgeKey{k->receiver,k->report_id}));if(!time)return fail<KnowledgeEntry>("KnowledgeTimeSourceMissing");auto at=read_social_bound_moment(*time,*a.exact_results,a.exact_results->stamp());if(!at.ok)return {false,{},at.error};if(at.value.half_day_tick!=k->learned_tick)return fail<KnowledgeEntry>("KnowledgeTimeProjectionMismatch");}
 if(const auto*known=d.known_reports.find(social_subject_key(q.context.observer)))for(const auto&row:known->rows()){if(row.first==id)continue;const auto*newer=d.reports.find(row.first);if(!newer)return fail<KnowledgeEntry>("KnowledgeReportSourceMissing");if(r->observation.supply&&newer->observation.supply&&r->observation.supply->source_key==newer->observation.supply->source_key&&newer->observation.supply->source_revision>r->observation.supply->source_revision)e.state=KnowledgeState::remembered;if(r->observation.obligation&&newer->observation.obligation&&r->observation.obligation->arrear_id==newer->observation.obligation->arrear_id&&newer->observation_tick>r->observation_tick)e.state=KnowledgeState::remembered;}
 if(r->observation.supply){const auto& v=*r->observation.supply;e.payload=KnownSupply{id,r->fact_id,v.organization_id,v.site_id,v.inventory_id,v.item_id,v.source_key,v.physical_amount,v.target_amount,v.source_revision,r->observation_tick,k->learned_tick,r->valid_until_tick};return {true,std::move(e),{}};}
 if(r->observation.obligation){const auto& v=*r->observation.obligation;e.payload=KnownArrear{id,r->fact_id,v.arrear_id,v.principal_id,v.beneficiary_id,v.outstanding_amount,v.due_tick,r->observation_tick,k->learned_tick};return {true,std::move(e),{}};}
 if(r->payload_kind=="intelligence_probe_location"||r->payload_kind=="cultural_content"){const auto category=r->payload_kind=="intelligence_probe_location"?KnowledgeCategory::intelligence_probe:KnowledgeCategory::cultural_content;if(!a.professional||!a.professional->supports(category))return fail<KnowledgeEntry>("ProfessionalKnowledgeReaderRequired",ErrorKind::unsupported_capability);auto value=a.professional->disclosed_payload(*r,*k,category);if(!value.ok)return {false,{},value.error};if(!value.value)return fail<KnowledgeEntry>("ProfessionalDisclosureSourceMissing");e.payload=std::move(*value.value);return {true,std::move(e),{}};}
 KnownActionOutcome outcome;outcome.report_id=id;outcome.fact_id=r->fact_id;
 if(r->payload_kind=="patrol_work_result"||r->payload_kind=="supervised_work_result"){
  if(!d.time2||!a.exact_results)return fail<KnowledgeEntry>("ExactWorkKnowledgeReaderRequired",ErrorKind::unsupported_capability);auto actual=a.exact_results->fact_v2_borrowed(r->fact_id,a.exact_results->stamp());if(!actual.ok)return {false,{},actual.error};if(!actual.value||actual.value->kind!=r->payload_kind||actual.value->outcome!=r->observation.outcome)return fail<KnowledgeEntry>("WorkKnowledgeProjectionMismatch");
  outcome.original_work_result=r->payload_kind=="patrol_work_result"?WorkResultSource{PatrolResult{r->fact_id}}:WorkResultSource{SupervisedResult{r->fact_id}};outcome.action_kind=r->payload_kind;outcome.completed_tick=r->observation_tick;outcome.success=r->observation.outcome=="work_completed";outcome.code=r->observation.outcome;for(const auto&person:r->observation.participants)outcome.disclosed_participants.push_back(person.subject);e.payload=std::move(outcome);return {true,std::move(e),{}};
 }
 if(d.time2){auto v=a.exact_results->action_result_v2_borrowed(r->fact_id,a.exact_results->stamp());if(!v.ok)return {false,{},v.error};if(!v.value)return fail<KnowledgeEntry>("UnsupportedKnowledgePayload",ErrorKind::unsupported_capability);outcome.action_id=v.value->action_id;outcome.action_kind=v.value->action_kind;outcome.completed_tick=v.value->completed_at.half_day_tick;outcome.success=v.value->success;outcome.code=v.value->code;}
 else{if(!a.legacy_results)return fail<KnowledgeEntry>("KnownActionReaderRequired",ErrorKind::unsupported_capability);auto v=a.legacy_results->action_result(r->fact_id);if(!v)return fail<KnowledgeEntry>("UnsupportedKnowledgePayload",ErrorKind::unsupported_capability);outcome.action_id=v->action_id;outcome.action_kind=v->action_kind;outcome.completed_tick=v->completed_tick;outcome.success=v->success;outcome.code=v->code;}
 for(const auto& p:r->observation.participants)outcome.disclosed_participants.push_back(p.subject);e.payload=std::move(outcome);return {true,std::move(e),{}};
}
Outcome<KnowledgeEntry> offer_entry(const KnowledgeReadAccess&a,const KnowledgePageRequest&q,const OfferRef&ref){auto e=unknown(ExactKnownOffer{ref});if(q.context.observer.kind!=SubjectKind::actor)return {true,std::move(e),{}};if(ref.kind==OfferRef::Kind::runtime){if(!a.runtime_offers)return fail<KnowledgeEntry>("RuntimeOfferKnowledgeAdapterRequired",ErrorKind::unsupported_capability);const RuntimeOfferDiscovery*found=nullptr;for(const auto&row:a.runtime_offers->discoveries.rows()){const auto&d=row.second;if(d.observer.id!=q.context.observer.id||d.offer.id!=ref.id)continue;if(q.context.scope==KnowledgeScope::local_disclosed&&!d.local_place_id)continue;if(!found||d.learned_at.half_day_tick>found->learned_at.half_day_tick||(d.learned_at.half_day_tick==found->learned_at.half_day_tick&&d.id<found->id))found=&d;}if(!found)return {true,std::move(e),{}};KnowledgeSource source{found->original_disclosure_ref.kind,found->original_disclosure_ref.id,{}};e.state=KnowledgeState::known;e.payload=KnownOfferDiscovery{ref,found->learned_at.half_day_tick,source,found->disclosed_terms_revision};e.source=KnowledgeSourceRef{KnowledgeSourceRef::Kind::offer_discovery,found->id,{}};e.learned_tick=found->learned_at.half_day_tick;return {true,std::move(e),{}};}for(const auto&person:a.interaction.knowledge)if(person.actor_id==q.context.observer.id)for(const auto&k:person.offers)if(k.offer_id==ref.id){if(q.context.scope==KnowledgeScope::local_disclosed&&k.source.kind!="dialogue"&&k.source.kind!="board")return {true,std::move(e),{}};e.state=KnowledgeState::known;e.payload=KnownOfferDiscovery{ref,k.discovered_tick,k.source,{}};e.source=KnowledgeSourceRef{KnowledgeSourceRef::Kind::offer_discovery,k.source.id,{}};e.learned_tick=k.discovered_tick;return {true,std::move(e),{}};}return {true,std::move(e),{}};}
Outcome<KnowledgeEntry> contact_entry(const KnowledgeReadAccess& a,const KnowledgePageRequest& q,const StableId& id){auto e=unknown(KnownContact{id});if(q.context.observer.kind!=SubjectKind::actor)return {true,std::move(e),{}};if(!a.contacts)return fail<KnowledgeEntry>("ContactKnowledgeReaderRequired",ErrorKind::unsupported_capability);auto value=read_known_contact_location(*a.contacts,q.context.observer.id,id);if(!value.ok)return {false,{},value.error};if(!value.value)return {true,std::move(e),{}};e.state=KnowledgeState::remembered;e.payload=*value.value;e.source=KnowledgeSourceRef{KnowledgeSourceRef::Kind::contact_receipt,value.value->source_fact_id,{}};e.observed_tick=value.value->observed_tick;e.learned_tick=value.value->learned_tick;return {true,std::move(e),{}};}
// 只投影观察者已有记忆；原始来源缺失或类型不受支持时拒绝。
Outcome<KnowledgeEntry> memory_entry(const KnowledgeReadAccess &a,
                                     const KnowledgePageRequest &q,
                                     const StableId &id) {
  auto e = unknown(ExactLegacyMemory{id});
  if (q.context.observer.kind != SubjectKind::actor)
    return {true, std::move(e), {}};
  const auto *memories = a.social.memories.find(q.context.observer.id);
  const auto *m = memories ? memories->find(id) : nullptr;
  if (!m)
    return {true, std::move(e), {}};
  KnownLegacySocialMemory value;
  value.fact_id = id;
  if (const auto *original = a.social.events.find(id)) {
    if (original->kind != "aid" && original->kind != "theft" &&
        original->kind != "refusal")
      return fail<KnowledgeEntry>("UnsupportedLegacyKnowledgeSource",
                                  ErrorKind::unsupported_capability);
    value.event_kind = original->kind;
    value.disclosed_actor_ids = {original->actor_id, original->target_actor_id};
    if (!original->place_id.empty())
      value.place_id = original->place_id;
    if (!original->item_id.empty())
      value.item_id = original->item_id;
    if (value.item_id)
      value.quantity = original->quantity;
    if (!original->reason.empty())
      value.reason = original->reason;
    e.observed_tick = original->tick;
  } else {
    if (!a.legacy_facts)
      return fail<KnowledgeEntry>("LegacyKnowledgeReaderRequired",
                                  ErrorKind::unsupported_capability);
    auto legacy_source = a.legacy_facts->social_fact(id);
    if (!legacy_source)
      return fail<KnowledgeEntry>("LegacyKnowledgeSourceMissing");
    if (legacy_source->kind != "personal_battle" &&
        legacy_source->kind != "training")
      return fail<KnowledgeEntry>("UnsupportedLegacyKnowledgeSource",
                                  ErrorKind::unsupported_capability);
    value.event_kind = legacy_source->kind;
    value.disclosed_actor_ids = legacy_source->participants;
    e.observed_tick = legacy_source->tick;
  }
  e.state = KnowledgeState::known;
  e.payload = std::move(value);
  e.source =
      KnowledgeSourceRef{KnowledgeSourceRef::Kind::legacy_memory, id,
                         SubjectRef{SubjectKind::actor, m->source_actor_id}};
  e.learned_tick = m->learned_tick;
  return {true, std::move(e), {}};
}
bool matches(const KnowledgeEntry& e,KnowledgeCategory category,const std::optional<SubjectRef>& filter,const ProfessionalKnowledgeRead*professional){if(!e.payload||e.payload->index()!=static_cast<std::size_t>(category))return false;if(!filter)return true;return std::visit([&](const auto& p){using T=std::decay_t<decltype(p)>;if constexpr(std::is_same_v<T,KnownSupply>)return filter->kind==SubjectKind::organization&&filter->id==p.organization_id;else if constexpr(std::is_same_v<T,KnownArrear>)return (filter->kind==SubjectKind::organization&&filter->id==p.organization_id)||(filter->kind==SubjectKind::actor&&filter->id==p.member_id);else if constexpr(std::is_same_v<T,KnownActionOutcome>){for(const auto& s:p.disclosed_participants)if(same_social_subject(s,*filter))return true;return false;}else if constexpr(std::is_same_v<T,KnownContactLocation>)return filter->kind==SubjectKind::actor&&filter->id==p.subject_actor_id;else if constexpr(std::is_same_v<T,KnownLegacySocialMemory>)return filter->kind==SubjectKind::actor&&std::find(p.disclosed_actor_ids.begin(),p.disclosed_actor_ids.end(),filter->id)!=p.disclosed_actor_ids.end();else if constexpr(std::is_same_v<T,std::shared_ptr<const KnownIntelligenceProbe>>||std::is_same_v<T,std::shared_ptr<const KnownCulturalContent>>)return professional&&professional->disclosed_subject(*e.payload,*filter);else return false;},*e.payload);}
}
Outcome<KnowledgePage> KnowledgeRead::query(const KnowledgePageRequest& q)const{
 auto permit=authorize(access_,q.context);if(!permit.ok)return {false,{},permit.error};if(q.limit<1||q.limit>50)return fail<KnowledgePage>("InvalidKnowledgePageLimit",ErrorKind::rule);
 const auto& a=access_;KnowledgePage page{a.common.state().identity,q.context.observer,q.context.scope,{},{}};const auto* list=std::get_if<ListKnown>(&q.query);
 if(!list){if(q.cursor)return fail<KnowledgePage>("UnexpectedKnowledgeCursor",ErrorKind::rule);auto item=std::visit([&](const auto& v)->Outcome<KnowledgeEntry>{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,ExactKnownReport>)return report_entry(a,q,v.report_id);else if constexpr(std::is_same_v<T,ExactKnownOffer>)return offer_entry(a,q,v.offer);else if constexpr(std::is_same_v<T,KnownContact>)return contact_entry(a,q,v.subject_actor_id);else if constexpr(std::is_same_v<T,ExactLegacyMemory>)return memory_entry(a,q,v.fact_id);else return fail<KnowledgeEntry>("InvalidKnowledgeQuery");},q.query);if(!item.ok)return {false,{},item.error};page.entries.push_back(std::move(item.value));return {true,std::move(page),{}};}
 if(static_cast<Integer>(list->category)<0||static_cast<Integer>(list->category)>7)return fail<KnowledgePage>("UnknownKnowledgeCategory",ErrorKind::unsupported_capability);if((list->category==KnowledgeCategory::intelligence_probe||list->category==KnowledgeCategory::cultural_content)&&(!a.professional||!a.professional->supports(list->category)))return fail<KnowledgePage>("ProfessionalKnowledgeReaderRequired",ErrorKind::unsupported_capability);
 struct Key{Integer learned=0;Integer category=0;StableId source;};auto before=[](const Key&a,const Key&b){if(a.learned!=b.learned)return a.learned>b.learned;if(a.category!=b.category)return a.category<b.category;return a.source<b.source;};auto key_of=[](const KnowledgeEntry&e){return Key{*e.learned_tick,static_cast<Integer>(e.payload->index()),e.source->source_id};};const auto prefix=cursor_prefix(page.source_identity,q);std::optional<Key>after;
 if(q.cursor){if(q.cursor->compare(0,prefix.size(),prefix)!=0)return fail<KnowledgePage>("StaleKnowledgeCursor",ErrorKind::rule);const char*p=q.cursor->data()+prefix.size();const char*end=q.cursor->data()+q.cursor->size();Key key;auto number=[&](Integer&value){auto parsed=std::from_chars(p,end,value);if(parsed.ec!=std::errc{}||parsed.ptr==end||*parsed.ptr!=':')return false;p=parsed.ptr+1;return true;};Integer bytes=0;if(!number(key.learned)||!number(key.category)||!number(bytes)||key.learned<0||key.category<0||key.category>7||bytes<0||static_cast<std::uint64_t>(bytes)!=static_cast<std::uint64_t>(end-p))return fail<KnowledgePage>("InvalidKnowledgeCursor",ErrorKind::rule);key.source.assign(p,end);after=std::move(key);}
 // Retain only the requested visible window plus one look-ahead, ordered by the fixed KQ1 key.
 // The scan is confined to this observer's real indexes; no world-wide hidden existence list is consulted.
 std::vector<KnowledgeEntry>window;auto consider=[&](Outcome<KnowledgeEntry>item)->Status{if(!item.ok)return {false,{},item.error};if(!matches(item.value,list->category,list->known_subject_filter,a.professional))return common_success();if(!item.value.learned_tick||!item.value.source)return common_failure("KnownEntryProvenanceMissing");auto key=key_of(item.value);if(after&&!before(*after,key))return common_success();auto where=std::lower_bound(window.begin(),window.end(),key,[&](const auto&e,const Key&k){return before(key_of(e),k);});if(where!=window.end()){auto existing=key_of(*where);if(!before(key,existing)&&!before(existing,key))return common_success();}window.insert(where,std::move(item.value));if(window.size()>q.limit+1)window.pop_back();return common_success();};
 if(list->category==KnowledgeCategory::offer_discovery){for(const auto&person:a.interaction.knowledge)if(person.actor_id==q.context.observer.id)for(const auto&offer:person.offers){auto valid=consider(offer_entry(a,q,{OfferRef::Kind::authored,offer.offer_id}));if(!valid.ok)return {false,{},valid.error};}if(a.runtime_offers)for(const auto&row:a.runtime_offers->discoveries.rows())if(row.second.observer.kind==q.context.observer.kind&&row.second.observer.id==q.context.observer.id){auto valid=consider(offer_entry(a,q,row.second.offer));if(!valid.ok)return {false,{},valid.error};}}
 else if(list->category==KnowledgeCategory::legacy_social){const auto*memories=a.social.memories.find(q.context.observer.id);if(memories)for(const auto&row:memories->rows()){auto valid=consider(memory_entry(a,q,row.first));if(!valid.ok)return {false,{},valid.error};}}
 else if(list->category==KnowledgeCategory::contact_location){if(!a.contacts)return fail<KnowledgePage>("ContactKnowledgeReaderRequired",ErrorKind::unsupported_capability);Integer cursor=0;while(true){auto contacts=read_known_contact_locations(*a.contacts,q.context.observer.id,cursor,50);if(!contacts.ok)return {false,{},contacts.error};for(const auto&row:contacts.value.rows){auto valid=consider(contact_entry(a,q,row.subject_actor_id));if(!valid.ok)return {false,{},valid.error};}if(!contacts.value.next_cursor)break;if(*contacts.value.next_cursor<=cursor)return fail<KnowledgePage>("ContactCursorDidNotAdvance");cursor=*contacts.value.next_cursor;}}
 else if(a.social.dynamic){const auto*reports=a.social.dynamic->known_reports.find(social_subject_key(q.context.observer));if(reports)for(const auto&row:reports->rows()){const auto*r=a.social.dynamic->reports.find(row.first);if(!r)return fail<KnowledgePage>("KnowledgeReportSourceMissing");bool relevant=false;switch(list->category){case KnowledgeCategory::supply:relevant=r->observation.supply.has_value();break;case KnowledgeCategory::arrear:relevant=r->observation.obligation.has_value();break;case KnowledgeCategory::intelligence_probe:relevant=r->payload_kind=="intelligence_probe_location";break;case KnowledgeCategory::cultural_content:relevant=r->payload_kind=="cultural_content";break;case KnowledgeCategory::action_outcome:if(a.social.dynamic->time2){if(!a.exact_results)return fail<KnowledgePage>("ExactKnowledgeReaderRequired",ErrorKind::unsupported_capability);auto actual=a.exact_results->action_result_v2_borrowed(r->fact_id,a.exact_results->stamp());if(!actual.ok)return {false,{},actual.error};relevant=actual.value!=nullptr||r->payload_kind=="patrol_work_result"||r->payload_kind=="supervised_work_result";}else{if(!a.legacy_results)return fail<KnowledgePage>("KnownActionReaderRequired",ErrorKind::unsupported_capability);relevant=a.legacy_results->action_result(r->fact_id).has_value();}break;default:break;}if(!relevant)continue;auto valid=consider(report_entry(a,q,row.first));if(!valid.ok)return {false,{},valid.error};}}
 const bool more=window.size()>q.limit;if(more)window.pop_back();page.entries=std::move(window);if(more&&!page.entries.empty()){const auto last=key_of(page.entries.back());page.next_cursor=prefix+std::to_string(last.learned)+":"+std::to_string(last.category)+":"+part(last.source);}return {true,std::move(page),{}};
}
}
