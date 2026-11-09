#include "repair_service_agreement_internal.hpp"
#include "resident/domains/actor_foundation/model.hpp"
#include "resident/domains/family/profile_control_source.hpp"
namespace hanmo::resident {using namespace repair_service_agreement_detail;
Outcome<const RepairServiceOfferSource*>read_repair_service_offer(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r){try{return {true,&offer(x,r),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Outcome<const RepairServiceAssentSource*>read_repair_service_assent(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r){try{return {true,&assent(x,r),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
bool NativeRepairServiceAgreementRead::bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept{return &c==&x_.common&&&i==&x_.items;}
Status NativeRepairServiceAgreementRead::validate_original_repair_service_rules(const RepairServiceContractTerms&t,const RepairPolicy&p)const{try{
 context(x_);const auto*original=take(read_repair_policy(x_.policies,t.repair_policy_ref));if(original!=&p)fail("RepairOriginalLockedPolicyRequired",t.instance_id);take(validate_repair_service_rule_terms(x_.rules,x_.policies,t));return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status NativeRepairServiceAgreementRead::validate_original_repair_service_acceptance(const RepairServiceContractTerms&t,const WorldMoment&at,const RepairServiceContractSource*actual)const{try{
 context(x_);const auto&a=assent(x_,t.customer_acceptance_source);const auto&o=offer(x_,a.offer_ref);
 if(!matches_accepted_terms(o,a,t))fail("RepairAgreedTermsMismatch",t.instance_id);
 if(actual)header(x_.common,repair_service_contract_source(actual->id));
 validate_adopted_agreement(x_,o,a,at,actual?x_.common.state().facts.find(actual->id):nullptr);
 if(actual){if(x_.items.repair_service_contracts.contracts.find(actual->id)!=actual||!same_repair_service_contract_terms(actual->terms,t)||compare(actual->accepted_at,at))fail("RepairAcceptedOriginalContractMismatch",actual->id);}
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
Outcome<WorldMoment>read_repair_service_agreement_moment(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r){try{if(r.source_ref.kind=="repair_service_offered")return {true,offer(x,r).offered_at,{}};if(r.source_ref.kind=="repair_service_assented")return {true,assent(x,r).accepted_at,{}};fail("UnsupportedRepairAgreementSource",r.source_ref.id,ErrorKind::unsupported_capability);}catch(Failure&e){return {false,{},std::move(e.error)};}}
Status validate_repair_service_agreements(const RepairServiceAgreementReadContext&x){try{
 context(x);const auto&s=x.items.repair_service_contracts;
 for(const auto&r:s.offers.rows()){
  if(r.first!=r.second.id)fail("RepairOfferKeyMismatch",r.first);const auto&o=offer(x,{repair_service_offer_source(r.first),1});Integer accepted=0;
  for(const auto&a:s.assents.rows())if(same(a.second.offer_ref,{repair_service_offer_source(o.id),1})){assent(x,{repair_service_assent_source(a.first),1});++accepted;}if(accepted>1)fail("RepairOfferAcceptedMoreThanOnce",o.id);
  for(const auto&q:s.offers.rows())if(q.first!=o.id&&same(q.second.actual_command_source,o.actual_command_source))fail("RepairOfferCommandReused",o.id);
 }
 for(const auto&r:s.assents.rows()){
  if(r.first!=r.second.id)fail("RepairAssentKeyMismatch",r.first);const auto&a=assent(x,{repair_service_assent_source(r.first),1});Integer contracts=0;
  for(const auto&c:s.contracts.rows())if(same(c.second.terms.customer_acceptance_source,{repair_service_assent_source(a.id),1}))++contracts;
  if(contracts!=1)fail("RepairAssentContractCoverageMismatch",a.id);
  for(const auto&q:s.assents.rows())if(q.first!=a.id&&same(q.second.actual_command_source,a.actual_command_source))fail("RepairAssentCommandReused",a.id);
 }
 for(const auto&r:s.contracts.rows()){
  const auto&a=assent(x,r.second.terms.customer_acceptance_source);const auto&o=offer(x,a.offer_ref);if(r.first!=r.second.id||compare(r.second.accepted_at,a.accepted_at)||!matches_accepted_terms(o,a,r.second.terms))fail("RepairAgreementContractReverseMismatch",r.first);
 }
 for(const auto&r:x.common.state().facts.rows())if(r.second.domain=="commerce"){
  if(r.second.kind=="repair_service_offered"&&!s.offers.find(r.first))fail("RepairOrphanOfferHeader",r.first);
  if(r.second.kind=="repair_service_assented"&&!s.assents.find(r.first))fail("RepairOrphanAssentHeader",r.first);
 }
 // Real retained commands are also roots. Deleting both payload and header
 // cannot conceal a sealed successful original business command.
 for(const auto&r:x.common.state().results.rows())if(r.second&&r.second->ok&&(r.second->kind=="offer_repair_service"||r.second->kind=="accept_repair_service_offer")){
  const auto*p=r.second->details?repair_service_command_receipt(*r.second->details):nullptr;if(!p)fail("RepairSealedCommandPayloadRequired",r.second->result_id);
  const MomentOwnerSource seal{{"common","command_result",r.second->result_id},1};
  if(r.second->kind=="offer_repair_service"){const auto&o=offer(x,p->actual_source);if(!same(o.actual_command_source,seal))fail("RepairOfferCommandReverseMismatch",o.id);}
  else{const auto&a=assent(x,p->actual_source);if(!same(a.actual_command_source,seal))fail("RepairAssentCommandReverseMismatch",a.id);}
 }
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
namespace repair_service_agreement_detail {
Status original_route_pins(const RepairServiceAgreementReadContext&x,const LifeActorRoute&route,const StableId&actor,RepairServiceAgreementPinSink&sink){
 if(const auto*player=std::get_if<LifePlayerActorRoute>(&route))if(const auto*source=std::get_if<SourceRef>(&player->original_control_source_ref)){
  const auto original=take(read_profile_controller_actual_source(x.controls,x.controls.foundation_catalog->identity,*source));
  if(!original.actor_id||*original.actor_id!=actor||!original.original_header||original.source_schema<=0)fail("RepairOriginalControlPinMismatch",actor);
  return sink.source({*source,original.source_schema});
 }
 // Authored initial control and Director policy are their original typed
 // catalog/adoption roots; they are not invented Common control facts.
 return sink.original_actor_route(route,actor);
}
}
Status visit_repair_service_agreement_pins(const RepairServiceAgreementReadContext&x,const MomentOwnerSource&r,RepairServiceAgreementPinSink&sink){try{
 if(r.source_ref.kind=="repair_service_offered"){
  const auto&o=offer(x,r);const auto&t=o.quoted_terms;take(sink.source(o.actual_contact_source));take(sink.source(o.actual_command_source));take(sink.item_condition(o.inspection.condition_head));
  for(const auto*p:{&t.fee_policy_source,&t.fee_terms.original_rule_ref,&t.return_terms.receiver_authority,&t.custody_terms.authority_ref,&t.custody_terms.original_rule_ref,&t.owner_authority_source,&t.payer_authority_source})take(sink.source(*p));
  take(original_route_pins(x,o.actor_route,o.provider_actor_id,sink));
  // Instance-specific return terms are this very offer; no self-pin.
  
 }else if(r.source_ref.kind=="repair_service_assented"){
  const auto&a=assent(x,r);take(sink.source(a.offer_ref));take(sink.source(a.actual_contact_source));take(sink.source(a.actual_command_source));take(original_route_pins(x,a.actor_route,a.customer_actor_id,sink));
  
 }else fail("UnsupportedRepairAgreementSource",r.source_ref.id,ErrorKind::unsupported_capability);
 return common_success();
}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
