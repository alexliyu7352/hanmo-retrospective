#include "political_treaty_exact_phase.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
template<class F>Status status(F&& fn){try{fn();return common_success();}catch(const Failure& f){return {false,{},f.error};}}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
// 每次callback持有本次World的lease；写完退出前失效，后验另从fresh factory取读。
struct WriterBoundary {WorldBundle& world;~WriterBoundary(){world.note_write_boundary();}};
std::shared_ptr<const PoliticalExactReadLease> read(PoliticalExactWorldFactory factory,const WorldBundle& world,const WorldMoment& at){
 auto owner=take(bind_original_political_exact_read(world,at,factory));
 if(!owner)fail("PoliticalExactReadLeaseRequired");const auto* b=owner->bindings().treaty_exact;
 if(!b||&b->terms.revisions.common!=&world.common||&b->exits.common!=&world.common||&b->exits.treaties!=&b->terms.terminations)
  fail("PoliticalTreatyExactReadBindingRequired");
 const auto& x=b->terms;const auto& s=x.state;const auto& r=x.revisions;
 // 在预测/到期前也核同一真实八根，不能以稳定时全验代替本次lease身份。
 if(s.schema_version!=1||&r.offers!=&s.offers||&r.counteroffers!=&s.counteroffers||
    &x.mandates.revisions!=&r||&x.mandates.mandates!=&s.mandates||
    &x.recognition.revisions!=&r||&x.recognition.presentations!=&s.presentations||&x.recognition.replies!=&s.replies||
    &x.signatures.revisions!=&r||&x.signatures.signatures!=&s.signatures||
    &x.activations.signatures!=&x.signatures||&x.activations.activations!=&s.activations||
    &x.terminations.activations!=&x.activations||&x.terminations.terminations!=&s.terminations||
    &x.releases.treaties!=&x.terminations||&x.releases.replies!=&s.replies)
  fail("PoliticalTreatyExactCandidateMismatch");
 if(compare(at,take(world.actual_phase_moment()))||compare(at,take(b->terms.revisions.clock.read_organization_authority_now()))||compare(at,take(b->exits.clock.read_organization_authority_now())))
  fail("PoliticalTreatyExactClockMismatch");return owner;
}
std::shared_ptr<PoliticalExactWriteLease> write(PoliticalExactWorldFactory factory,WorldBundle& world,const WorldMoment& at){
 auto owner=take(bind_original_political_exact_write(world,at,factory));
 if(!owner)fail("PoliticalExactWriteLeaseRequired");auto* b=owner->bindings().treaty_exact;
 if(!b||&b->sources.activations.signatures.revisions.common!=&world.common||&b->sources.activations.activations!=&b->terms.activations||
    &b->sources.terminations!=&b->terms.terminations||&b->exit_sources.common!=&world.common||&b->exit_sources.state!=&b->exits||&b->exit_sources.treaties!=&b->sources)
  fail("PoliticalTreatyExactWriteBindingRequired");
 const auto& signatures=b->sources.activations.signatures;const auto& revisions=signatures.revisions;
 if(b->terms.schema_version!=1||&revisions.offers!=&b->terms.offers||&revisions.counteroffers!=&b->terms.counteroffers||
    &signatures.signatures!=&b->terms.signatures)
  fail("PoliticalTreatyExactCandidateMismatch");
 if(compare(at,take(world.actual_phase_moment()))||compare(at,take(b->sources.activations.signatures.revisions.clock.read_organization_authority_now()))||compare(at,take(b->exit_sources.clock.read_organization_authority_now())))
  fail("PoliticalTreatyExactClockMismatch");return owner;
}
ExactEventPrediction event(const TreatyTermsDue& due){return {due.at,ExactStage::political_due_close,due.original_source.source_ref,due.original_source.source_schema,due.terms_revision};}
SafeExitDeadlineKind local_kind(ExactDerivedDeadlineKind kind){
 if(kind==ExactDerivedDeadlineKind::safe_exit_termination_window)return SafeExitDeadlineKind::install_termination_window;
 if(kind==ExactDerivedDeadlineKind::safe_exit_window_expiry)return SafeExitDeadlineKind::expire_exit_window;
 fail("PoliticalSafeExitDeadlineKindUnsupported");
}
ExactDerivedDeadlinePrediction deadline(const SafeExitEntryContext& ctx,const SafeExitDeadline& due){
 const auto* binding=ctx.state.bindings.find(due.binding_source.id);if(!binding)fail("SafeExitBindingSourceMissing",due.binding_source.id);
 const auto kind=due.kind==SafeExitDeadlineKind::install_termination_window?ExactDerivedDeadlineKind::safe_exit_termination_window:ExactDerivedDeadlineKind::safe_exit_window_expiry;
 // binding是此原有限scope的身份；at由真实termination/crossing读，不取binding原发生时点。
 return {{due.at,ExactStage::political_due_close,due.binding_source,1,binding->terms_revision},kind};
}
bool same(const ExactEventPrediction& a,const ExactEventPrediction& b){return a.stage==b.stage&&a.source_schema==b.source_schema&&a.dependency_revision==b.dependency_revision&&same_source_ref(a.actual_source_basis,b.actual_source_basis)&&compare(a.at,b.at)==0;}
bool in_window(const WorldMoment& at,const WorldMoment& from,const WorldMoment& until){if(compare(at,from)<0)fail("PoliticalOriginalDeadlineMissed");return compare(at,until)<=0;}
}
ExactPhaseRegistration make_political_treaty_exact_phase_registration(PoliticalExactWorldFactory factory){
 auto r=make_political_exact_phase_registration(factory);r.version=2;r.schedule_kind=ExactScheduleKind::exact_due;
 r.effect_writer_keys.push_back("politics/original_treaty_activation_expiry");r.effect_writer_keys.push_back("politics/original_safe_exit_deadline_projection");
 r.validate_author_selection=[factory](const WorldBundle& world){return status([&]{
  take(validate_original_political_exact_author_selection(world,factory));auto owner=read(factory,world,take(world.actual_phase_moment()));
  const auto& b=*owner->bindings().treaty_exact;if(b.terms.state.schema_version!=1)fail("PoliticalTreatyTermsSchemaRequired");
 });};
 r.validate_world_sources=[factory](const WorldBundle& world,const WorldMoment& at){return status([&]{
  take(validate_original_political_exact_world_sources(world,at,factory));auto owner=read(factory,world,at);const auto& b=*owner->bindings().treaty_exact;
  take(validate_political_treaty_terms_state(b.terms));take(validate_safe_exit_entries(b.exits));
 });};
 r.predict=[factory](const WorldBundle& world,const WorldMoment& from,const WorldMoment& until)->Outcome<std::vector<ExactEventPrediction>>{try{
  auto owner=read(factory,world,from);const auto& b=*owner->bindings().treaty_exact;std::vector<ExactEventPrediction> out;
  for(const auto& due:take(read_treaty_terms_due(b.terms.terminations)))if(in_window(due.at,from,until))out.push_back(event(due));
  return {true,std::move(out),{}};
 }catch(const Failure& f){return {false,{},f.error};}};
 r.apply_due=[factory](WorldBundle& world,const ExactEventPrediction& prediction,ExactPhaseWriteContext&)->Outcome<ExactEventApplied>{try{
  auto owner=write(factory,world,prediction.at);auto& b=*owner->bindings().treaty_exact;std::optional<TreatyTermsDue> actual;
  for(const auto& due:take(read_treaty_terms_due(b.sources)))if(same(event(due),prediction)){if(actual)fail("AmbiguousTreatyOriginalDue");actual=due;}
  if(!actual)fail("TreatyOriginalDueChanged",prediction.actual_source_basis.id);WriterBoundary boundary{world};
  auto source=take(apply_treaty_terms_due_in_candidate(world.common,b.terms.activations,b.terms.terminations,b.sources,b.activation,b.termination,*actual));
  return {true,{std::move(source),1},{}};
 }catch(const Failure& f){return {false,{},f.error};}};
 r.validate_applied_source=[factory](const WorldBundle& world,const ExactEventApplied& applied){return status([&]{
  const auto now=take(world.actual_phase_moment());auto owner=read(factory,world,now);const auto& b=*owner->bindings().treaty_exact;WorldMoment at;
  if(applied.source_schema!=1||applied.applied_result.domain!="politics")fail("TreatyActualResultSourceRequired",applied.applied_result.id);
  if(applied.applied_result.kind=="treaty_terms_activated")at=take(read_verified_treaty_activation(b.terms.activations,applied.applied_result,1)).source->activated_at;
  else if(applied.applied_result.kind=="treaty_terms_terminated")at=take(read_verified_treaty_termination(b.terms.terminations,applied.applied_result,1)).source->terminated_at;
  else fail("TreatyActualResultSourceRequired",applied.applied_result.id);
  if(compare(at,now))fail("TreatyActualResultMomentMismatch",applied.applied_result.id);
 });};
 r.derived_deadline_kinds={ExactDerivedDeadlineKind::safe_exit_termination_window,ExactDerivedDeadlineKind::safe_exit_window_expiry};
 r.predict_derived_deadlines=[factory](const WorldBundle& world,const WorldMoment& from,const WorldMoment& until)->Outcome<std::vector<ExactDerivedDeadlinePrediction>>{try{
  auto owner=read(factory,world,from);const auto& b=*owner->bindings().treaty_exact;std::vector<ExactDerivedDeadlinePrediction> out;
  for(const auto& due:take(read_safe_exit_deadlines(b.exits)))if(in_window(due.at,from,until))out.push_back(deadline(b.exits,due));
  return {true,std::move(out),{}};
 }catch(const Failure& f){return {false,{},f.error};}};
 r.apply_derived_deadline=[factory](WorldBundle& world,const ExactDerivedDeadlinePrediction& prediction,ExactPhaseWriteContext&){return status([&]{
  auto owner=write(factory,world,prediction.original.at);auto& b=*owner->bindings().treaty_exact;std::optional<SafeExitDeadline> actual;
  for(const auto& due:take(read_safe_exit_deadlines(b.exit_sources))){auto current=deadline(b.exit_sources,due);if(current.kind==prediction.kind&&same(current.original,prediction.original)){if(actual)fail("AmbiguousSafeExitOriginalDeadline");actual=due;}}
  if(!actual)fail("SafeExitOriginalDeadlineChanged",prediction.original.actual_source_basis.id);WriterBoundary boundary{world};
  take(apply_safe_exit_deadline_in_candidate(world.common,b.exits,b.exit_sources,*actual));
 });};
 r.validate_derived_deadline=[factory](const WorldBundle& world,const ExactDerivedDeadlinePrediction& prediction){return status([&]{
  auto owner=read(factory,world,prediction.original.at);const auto& b=*owner->bindings().treaty_exact;const auto& event=prediction.original;
  const auto* binding=b.exits.state.bindings.find(event.actual_source_basis.id);
  if(event.stage!=ExactStage::political_due_close||event.source_schema!=1||!binding||event.dependency_revision!=binding->terms_revision)
   fail("SafeExitOriginalDeadlineBindingMismatch",event.actual_source_basis.id);
  take(validate_safe_exit_deadline_applied(b.exits,event.actual_source_basis,local_kind(prediction.kind),event.at));
 });};
 return r;
}
}
