#include "voluntary_path_views.hpp"
#include "resident/views/common_presentation.hpp"
#include <set>
namespace hanmo::resident {namespace {
using V=BoundaryValue;
V identity(const CommonStore& c){using namespace boundary;StableId world;const char* digits="0123456789abcdef";for(auto b:c.state().identity.world_id){world+=digits[b>>4];world+=digits[b&15];}const auto& i=c.state().identity;return obj({{"world_id",V{world}},{"execution_epoch",V{i.execution_epoch}},{"revision",V{i.revision}},{"tick",V{i.tick}}});}
V person(const VoluntaryPathContext& x,const StableId& actor){using namespace boundary;auto name=CommonPresentation(x.people.common.state(),x.people.catalog).display_name(actor);return obj({{"actor_id",V{actor}},{"name",V{name.ok?name.value:actor}}});}
V selection(const SuccessionPersonalTerms& terms){using namespace boundary;V::Array goods;for(const auto& lot:terms.lots)goods.push_back(obj({{"item_id",V{lot.item_id}},{"quantity",V{lot.quantity}}}));return obj({{"money",V{terms.money}},{"goods",V{std::move(goods)}}});}
}
Outcome<BoundaryValue> query_voluntary_path_page(const VoluntaryPathContext& x,const VoluntaryPathState& state,const InteractionState& interactions,const InteractionAccess& access,Integer page){
 using namespace boundary;if(page<0||page>4095)return {false,{},common_failure("InvalidVoluntaryPathPage").error};const auto& c=x.people.common;auto valid=validate_voluntary_path_history(x,state);if(!valid.ok)return {false,{},valid.error};valid=validate_voluntary_path_result_bindings(c,x.people.family,state);if(!valid.ok)return {false,{},valid.error};const auto& actor=c.state().metadata.player_actor_id;const auto* current=c.actor(actor);if(!current)return {false,{},common_failure("UnknownActor",actor).error};V::Array rows;std::set<std::pair<StableId,StableId>> seen;
 auto operation=[&](const VoluntaryPathCommand& command){auto quote=preview_voluntary_path_command(x,state,actor,command);V::Dict params;if(!command.plan_id.empty())params.emplace_back("plan_id",V{command.plan_id});else{params.emplace_back("request_id",V{command.request_id});params.emplace_back("successor_actor_id",V{command.successor_actor_id});if(!command.relation_fact_id.empty())params.emplace_back("relation_fact_id",V{command.relation_fact_id});}return obj({{"command",V{command.kind}},{"params",V{std::move(params)}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});};
 auto original=[&](const FamilyCommand& command){auto quote=preview_original_voluntary_path_command(x,state,interactions,access,actor,command);V::Dict params;if(command.plan_id)params.emplace_back("plan_id",V{*command.plan_id});else{params.emplace_back("request_id",V{*command.request_id});params.emplace_back("heir_actor_id",V{*command.heir_actor_id});}return obj({{"command",V{command.kind}},{"params",V{std::move(params)}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});};
 if(current->alive)for(const auto& session:interactions.interactions){if(session.actor_id!=actor||session.phase!="say"||session.context.provider_actor_id.empty())continue;const auto& candidate=session.context.provider_actor_id;
  for(const auto& row:x.catalog.definitions.rows()){
   // 仅沿当前实际发言双方自己的已成立关系取原ID；不扫描陌生人物或未认可族谱。
   std::vector<StableId> sources;const auto kind=row.second.relation_kind;
   if(kind==VoluntaryRelationKind::biological_child)sources.push_back({});
   else if(kind==VoluntaryRelationKind::adoption)for(const auto& fact:x.people.family.facts.rows()){
    if(const auto* a=std::get_if<AdoptionFact>(&fact.second);a&&a->success&&a->guardian_actor_id==actor&&a->child_actor_id==candidate)sources.push_back(fact.first);
    if(const auto* a=std::get_if<AdultAdoptionFact>(&fact.second);a&&a->agreement.adopter_actor_id==actor&&a->agreement.adoptee_actor_id==candidate)sources.push_back(fact.first);
   }else if(kind==VoluntaryRelationKind::accepted_disciple&&x.people.apprenticeships){for(const auto& fact:x.people.apprenticeships->replies.rows())if(fact.second.confirmation){const auto& a=*fact.second.confirmation;if(a.teacher_actor_id==actor&&a.student_actor_id==candidate&&a.skill_id==row.second.disciple_skill_id)sources.push_back(fact.first);}}
   else if(kind==VoluntaryRelationKind::recognized_collateral&&x.people.collateral){for(const auto& fact:x.people.collateral->recognitions.rows()){auto a=read_collateral_recognition(c,*x.people.collateral,fact.first);if(a.ok&&((a.value.first_actor_id==actor&&a.value.second_actor_id==candidate)||(a.value.second_actor_id==actor&&a.value.first_actor_id==candidate)))sources.push_back(fact.first);}}
   for(const auto& source:sources){VoluntaryPathCommand command{"family_voluntary_path_accept",row.first,candidate,source,{}};auto quote=preview_voluntary_path_command(x,state,actor,command);if(!quote.ok||!seen.emplace(row.first,candidate).second)continue;rows.push_back(obj({{"kind",V{StableId{"offer"}}},{"request_id",V{row.first}},{"successor",person(x,candidate)},{"personal",selection(quote.value.consent->agreement.personal)},{"actions",V{V::Array{operation(command)}}}}));}
  }
  for(const auto& row:x.original_requests.definitions.rows()){FamilyCommand command;command.kind="family_agree_succession";command.request_id=row.first;command.heir_actor_id=candidate;auto quote=preview_original_voluntary_path_command(x,state,interactions,access,actor,command);if(!quote.ok||!seen.emplace(row.first,candidate).second)continue;rows.push_back(obj({{"kind",V{StableId{"offer"}}},{"request_id",V{row.first}},{"successor",person(x,candidate)},{"personal",selection(quote.value.succession_consent->agreement.personal)},{"actions",V{V::Array{original(command)}}}}));}
 }
 for(const auto& row:x.people.family.facts.rows())if(const auto* plan=std::get_if<SuccessionConsentFact>(&row.second)){
  const auto& a=plan->agreement;const auto& first=a.personal.predecessor_actor_id;const auto& second=a.personal.successor_actor_id;if(first!=actor&&second!=actor)continue;const bool modern=state.consents.find(row.first)!=nullptr;V::Array actions;
  if(!plan->conclusion&&first==actor&&current->alive){
   if(modern){actions.push_back(operation({"family_voluntary_path_continue",{},{},{},row.first}));actions.push_back(operation({"family_voluntary_path_cancel",{},{},{},row.first}));VoluntaryPathCommand withdrawal{"family_voluntary_path_withdraw",{},{},{},row.first};if(preview_voluntary_path_command(x,state,actor,withdrawal).ok)actions.push_back(operation(withdrawal));}
   else for(const auto* kind:{"family_continue_as","family_cancel_succession"}){FamilyCommand command;command.kind=kind;command.plan_id=row.first;actions.push_back(original(command));}
  }
  rows.push_back(obj({{"kind",V{StableId{"plan"}}},{"plan_id",V{row.first}},{"request_id",V{a.request_id}},{"predecessor",person(x,first)},{"successor",person(x,second)},{"accepted_tick",V{a.accepted_tick}},{"status",V{plan->conclusion?plan->conclusion->kind:StableId{"accepted"}}},{"personal",selection(a.personal)},{"actions",V{std::move(actions)}}}));
 }
 const Integer count=rows.size();V::Array page_rows;for(Integer i=page*16;i<std::min<Integer>(count,page*16+16);++i)page_rows.push_back(std::move(rows[static_cast<std::size_t>(i)]));
 return {true,obj({{"actor_id",V{actor}},{"source_identity",identity(c)},{"page",V{page}},{"total",V{count}},{"rows",V{std::move(page_rows)}},{"living_voluntary",V{true}},{"public_authority_transferred",V{false}},{"knowledge_transferred",V{false}},{"scope",V{StableId{"player_control_and_selected_personal_assets"}}}}),{}};
}
}
