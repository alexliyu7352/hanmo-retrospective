#include "director_authority_wake2.hpp"
namespace hanmo::resident {namespace {
bool same(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool grant_kind(const StableId&kind){return kind=="organization_grant_issued"||kind=="organization_grant_amended"||kind=="organization_grant_revoked";}
Status current(const DirectorAuthorityWakeRead2&x){auto now=x.clock.read_organization_authority_now();if(!now.ok)return {false,{},now.error};auto equal=compare_world_moments(now.value,x.actual);if(!equal.ok)return {false,{},equal.error};return equal.value==0?common_success():common_failure("StaleDirectorAuthorityWakeRead");}
}
Status visit_director_authority_source_wakes2(const DirectorAuthorityWakeRead2&x,const SourceRef&source,Integer schema,const DirectorAuthorityWakeVisitor2&visit){
 if(!visit)return common_failure("DirectorAuthorityWakeVisitorRequired");auto valid=current(x);if(!valid.ok)return valid;
 if(source.domain!="organization"||source.id.empty())return common_failure("DirectorAuthorityWakeSourceRequired",source.id);
 auto emit=[&](const SubjectRef&subject,Integer revision,const WorldMoment&at)->Status{if(subject.id.empty()||revision<1)return common_failure("DirectorAuthorityWakePartyMissing",source.id);auto past=compare_world_moments(at,x.actual);if(!past.ok)return {false,{},past.error};if(past.value>0)return common_failure("FutureDirectorAuthorityWake",source.id);return visit({subject,SubjectWakeKind2::authority,source,schema,revision},at);};
 if(source.kind=="organization_office_changed"){
  auto original=read_office_authority_change_source(x.common,x.organization,x.catalog,source,schema,x.stamp,x.transitions,x.deaths,x.clock);if(!original.ok)return {false,{},original.error};const auto&fact=*original.value.fact;const auto&change=*original.value.change;
  valid=emit({SubjectKind::organization,fact.organization_id},change.after_epoch,change.effective_moment);if(!valid.ok)return valid;
  if(change.old_holder){valid=emit({SubjectKind::actor,*change.old_holder},change.after_epoch,change.effective_moment);if(!valid.ok)return valid;}
  if(change.new_holder&&change.new_holder!=change.old_holder){valid=emit({SubjectKind::actor,*change.new_holder},change.after_epoch,change.effective_moment);if(!valid.ok)return valid;}
  // K3 alone replays original Grant ancestry and continuity. The receiver set is
  // historical at this change, never today's office holder or current grant map.
  return visit_office_dependent_grant_targets_at_change(x.common,x.organization,x.catalog,source,schema,x.stamp,x.transitions,x.deaths,x.clock,[&](const OfficeTermRead&term){auto r=emit({SubjectKind::organization,term.organization_id},change.after_epoch,change.effective_moment);if(!r.ok)return r;return term.holder_actor_id?emit({SubjectKind::actor,*term.holder_actor_id},change.after_epoch,change.effective_moment):common_success();});
 }
 if(!grant_kind(source.kind))return common_failure("DirectorAuthorityWakeSourceRequired",source.id);
 auto original=read_grant_authority_change_source(x.common,x.organization,x.catalog,source,schema,x.clock);if(!original.ok)return {false,{},original.error};const auto&change=*original.value.change;
 return visit_descendant_grant_targets_at_change(x.common,x.organization,x.catalog,source,schema,x.clock,[&](const OfficeTermRead&term){auto r=emit({SubjectKind::organization,term.organization_id},change.after_revision,change.effective_moment);if(!r.ok)return r;return term.holder_actor_id?emit({SubjectKind::actor,*term.holder_actor_id},change.after_revision,change.effective_moment):common_success();});
}
Status visit_director_authority_wakes2(const DirectorAuthorityWakeRead2&x,const DirectorAuthorityWakeVisitor2&visit){
 if(!visit)return common_failure("DirectorAuthorityWakeVisitorRequired");auto valid=current(x);if(!valid.ok)return valid;
 // Original Common order preserves equal-M changes. No copied fact table or
 // independently persisted authorization graph is built for the Director.
 for(const auto&id:x.common.state().facts.order()){const auto*fact=x.organization.facts.find(id);if(!fact)continue;const bool office=fact->kind=="organization_office_changed";if(!office&&!grant_kind(fact->kind))continue;auto emitted=visit_director_authority_source_wakes2(x,{"organization",fact->kind,id},office?2:1,visit);if(!emitted.ok)return emitted;}
 return common_success();
}
Status validate_director_authority_wake2(const DirectorAuthorityWakeRead2&x,const SubjectWake2&wake){
 if(wake.kind!=SubjectWakeKind2::authority)return common_failure("DirectorAuthorityWakeKindMismatch",wake.actual_source.id);bool found=false;
 auto valid=visit_director_authority_source_wakes2(x,wake.actual_source,wake.source_schema,[&](const SubjectWake2&actual,const WorldMoment&){if(same(actual.subject,wake.subject)&&same(actual.actual_source,wake.actual_source)&&actual.source_revision==wake.source_revision)found=true;return common_success();});if(!valid.ok)return valid;
 return found?common_success():common_failure("DirectorAuthorityWakePartyMismatch",wake.actual_source.id);
}
}
