#include "visit_authored_command.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<DirectorAcceptedCommandView>read_visit_authored_command(const VisitAuthoredCommandRead&x,const MomentOwnerSource&source,const StableId&actor,const StableId&kind,Integer tick){
 auto fail=[&](const char*code,ErrorKind k=ErrorKind::rule)->Outcome<DirectorAcceptedCommandView>{return {false,{},common_failure(code,source.source_ref.id,k).error};};
 if(!x.director.subject2||x.director.subject2->command_binding_version!=1||!x.catalog.subjects2)return fail("VisitAuthoredDirectorCommandSourcesRequired",ErrorKind::unsupported_capability);
 const auto&ref=source.source_ref;const auto*h=x.common.state().facts.find(ref.id);
 if(source.source_schema!=1||!h||h->id!=ref.id||h->domain!=ref.domain||h->kind!=ref.kind)return fail("VisitAuthoredOriginalHeaderRequired");
 const DirectorAdoptionResult2*found=nullptr;
 for(const auto&row:x.director.subject2->adoption_results.rows())if(row.second.accepted_command){const auto&effect=row.second.accepted_command->actual_effect;if(effect.source_ref.domain==ref.domain&&effect.source_ref.kind==ref.kind&&effect.source_ref.id==ref.id){if(found||effect.source_schema!=source.source_schema)return fail("VisitAuthoredCommandSourceAliased");found=&row.second;}}
 if(!found||!found->accepted_command)return fail("VisitAuthoredActualAdoptionRequired");
 const auto&cmd=*found->accepted_command;if(cmd.actor_id!=actor||cmd.command_kind!=kind||cmd.accepted_at.half_day_tick!=tick)return fail("VisitAuthoredCommandSourceMismatch");
 auto actual=read_director_accepted_command(x.common,x.director,x.catalog,x.identity,{actor,found->registration_epoch,found->decision_seq,kind,cmd.request_seq,cmd.result_id});if(!actual.ok)return actual;
 auto now=x.clock.read_current_moment();if(!now.ok)return {false,{},now.error};auto order=compare_world_moments(cmd.accepted_at,now.value);if(!order.ok)return {false,{},order.error};if(order.value>0)return fail("VisitAuthoredCommandFromFuture");
 const auto*r=actual.value.actual_result;const auto*effect=r&&r->details?std::get_if<InteractionEffect>(&r->details->data):nullptr;
 if(!effect||effect->settlement_result!=ref.id||!effect->encounter_id.empty()||effect->party_id||effect->person_id||effect->news_ids)return fail("VisitAuthoredActualResultEffectRequired");
 return actual;
}
}
