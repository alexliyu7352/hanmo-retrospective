#include "repair_service_command.hpp"
#include "repair_service_agreement_internal.hpp"
#include "resident/domains/craft/repair_acceptance.hpp"
#include "resident/domains/craft/repair_codec.hpp"
namespace hanmo::resident {using namespace repair_service_agreement_detail;
namespace {
void bound(const RepairServiceCommandContext&x){
 context(x.agreements);if(&x.commercial.common!=&x.agreements.common||&x.commercial.items!=&x.agreements.items||&x.commercial.policies!=&x.agreements.policies||x.commercial.rules!=&x.agreements.rules||x.commercial.repair!=&x.repair||x.repair.commercial_sources!=&x.commercial||&x.repair.conditions!=&x.agreements.conditions||&x.commercial.clock!=&x.agreements.clock||compare(take(x.stamps.read_current_moment()),context(x.agreements)))fail("RepairCommandOriginalContextMismatch");
 if(!x.commercial.original.bound_to(x.agreements.common,x.agreements.items)||!x.repair.original.bound_to(x.agreements.common,x.agreements.items))fail("RepairCommandOriginalCandidateMismatch");
}
void contact_now(const RepairServiceCommandContext&x,const StableId&actor,const StableId&other,const MomentOwnerSource&source){
 if(source.source_schema!=1)fail("RepairCurrentActualContactRequired",source.source_ref.id);const auto actual=take(read_current_face_to_face_contact_source(x.agreements.common,x.agreements.interactions,x.agreements.interaction_catalog,x.interaction_access,actor,other,context(x.agreements)));if(!same_source_ref(actual,source.source_ref))fail("RepairCurrentContactChanged",source.source_ref.id,ErrorKind::rule);
}
void prospective_values(const RepairServiceCommandContext&x,const StableId&actor,const OfferRepairServiceRequest&q){
 const auto&t=q.terms;const auto*p=take(read_repair_policy(x.agreements.policies,t.repair_policy_ref));
 if(t.customer.kind!=Owner::Kind::actor||t.provider.kind!=Owner::Kind::actor||t.customer.extra||t.provider.extra)fail("RepairRepresentedCommandAuthorityRequired",t.instance_id,ErrorKind::unsupported_capability);
 if(t.provider.id!=actor||actor==t.customer.id||!x.agreements.common.actor(actor)||!x.agreements.common.actor(t.customer.id))fail("RepairOfferActualProviderRequired",actor,ErrorKind::rule);
 const auto*f=take(read_repair_fee_policy(x.agreements.rules,t.fee_policy_source));const auto*c=take(read_repair_custody_policy(x.agreements.rules,t.custody_terms.original_rule_ref));
 if(f->policy_id!=p->fee_policy_ref||!same(t.fee_terms.original_rule_ref,t.fee_policy_source)||t.fee_terms.trigger!=f->trigger||t.fee_terms.amount_qian!=p->fee_qian||t.fee_terms.line_id.empty()||c->policy_id!=p->custody_policy_ref||c->delivery_before_acceptance!=t.custody_terms.delivery_before_acceptance||c->uncollected_fee_qian!=t.custody_terms.uncollected_fee_qian||c->uncollected_period_half_days!=t.custody_terms.uncollected_period_half_days)fail("RepairOfferLockedRuleMismatch",t.instance_id);
 if(c->uncollected_fee_qian)fail("RepairActualCustodyFeeOccurrenceWriterRequired",t.instance_id,ErrorKind::unsupported_capability);
 if(q.inspection.delivered_custody_source_ref)fail("RepairOfferOriginalDeliveryArrangementRequired",t.instance_id,ErrorKind::unsupported_capability);
 const auto&item=repair_detail::lot(x.agreements.common,q.inspection.original_item);if(item.item_instance_id!=std::optional<StableId>{t.instance_id}||q.inspection.original_item.quantity!=1||item.quantity!=1||!owner(item.owner,t.customer)||!([&](){const auto head=take(read_current_item_condition_head(x.agreements.conditions,t.instance_id));return repair_detail::same_head(head.value(),q.inspection.condition_head);}()))fail("StaleRepairOfferInspection",t.instance_id,ErrorKind::rule);
 contact_now(x,actor,t.customer.id,q.actual_contact_source);
}
RepairServiceQuotedTerms actual_quote(const RepairServiceOfferValues&v,const MomentOwnerSource&source){
 RepairServiceQuotedTerms t;t.instance_id=v.instance_id;t.repair_policy_ref=v.repair_policy_ref;t.worker_id=v.worker_id;t.customer=v.customer;t.provider=v.provider;t.payer_wallet_id=v.payer_wallet_id;t.payee_wallet_id=v.payee_wallet_id;t.fee_policy_source=v.fee_policy_source;t.fee_terms=v.fee_terms;
 static_cast<RepairServiceReturnValues&>(t.return_terms)=v.return_terms;t.return_terms.original_rule_ref=source;t.custody_terms=v.custody_terms;t.owner_authority_source=v.owner_authority_source;t.payer_authority_source=v.payer_authority_source;t.terms_revision=v.terms_revision;return t;
}
const RepairServiceOfferSource& prospective_assent(const RepairServiceCommandContext&x,const StableId&actor,const AcceptRepairServiceOfferRequest&q){
 const auto&o=offer(x.agreements,q.offer_ref);if(o.quoted_terms.customer.id!=actor)fail("RepairActualCustomerRequired",actor,ErrorKind::rule);
 for(const auto&r:x.agreements.items.repair_service_contracts.assents.rows())if(same(r.second.offer_ref,q.offer_ref))fail("RepairOfferAlreadyAccepted",o.id,ErrorKind::rule);
 contact_now(x,actor,o.provider_actor_id,q.actual_contact_source);inspection(x.agreements,o.quoted_terms,o.inspection,q.actual_contact_source,context(x.agreements),nullptr);
 if(o.quoted_terms.custody_terms.delivery_before_acceptance)fail("RepairActualEarlyDeliveryProducerRequired",o.id,ErrorKind::unsupported_capability);
 return o;
}
void pending_assent(const RepairServiceCommandContext&x,const RepairServiceOfferSource&o,const RepairServiceAssentSource&a){
 const auto current=context(x.agreements);if(compare(a.accepted_at,current)||a.customer_actor_id!=o.quoted_terms.customer.id||!same(a.offer_ref,{repair_service_offer_source(o.id),1}))fail("RepairInFlightAssentMismatch",a.id);
 validate_adopted_agreement(x.agreements,o,a,current);
 const auto cut=repair_service_assent_source(a.id);inspection(x.agreements,o.quoted_terms,o.inspection,a.actual_contact_source,a.accepted_at,&cut);
}
// This scoped reader exists only while the original two-owner acceptance runs.
// It recognizes one actually appended assent header. It cannot approve another
// proposal, survive publication, or treat the future Common seal as present.
class InFlightCommercial final:public RepairServiceOriginalRead {
 const RepairServiceCommandContext&x_;const RepairServiceOfferSource&o_;const RepairServiceAssentSource&a_;
 const RepairServiceOriginalRead&base()const{return x_.commercial.original;}
public:InFlightCommercial(const RepairServiceCommandContext&x,const RepairServiceOfferSource&o,const RepairServiceAssentSource&a):x_(x),o_(o),a_(a){}
 bool bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept override{return &c==&x_.agreements.common&&&i==&x_.agreements.items&&base().bound_to(c,i);}
 Status validate_original_repair_agreement_coverage()const override{try{
  pending_assent(x_,o_,a_);const auto&s=x_.agreements.items.repair_service_contracts;
  for(const auto&r:s.offers.rows()){if(r.first!=r.second.id)fail("RepairOfferKeyMismatch",r.first);offer(x_.agreements,{repair_service_offer_source(r.first),1});}
  for(const auto&r:s.assents.rows()){if(r.first!=r.second.id||same(r.second.offer_ref,a_.offer_ref))fail("RepairConflictingInFlightAssent",r.first);assent(x_.agreements,{repair_service_assent_source(r.first),1});}
  Integer targets=0;for(const auto&r:s.contracts.rows())if(same(r.second.terms.customer_acceptance_source,{repair_service_assent_source(a_.id),1})){++targets;if(r.first!=r.second.id||!matches_accepted_terms(o_,a_,r.second.terms)||compare(r.second.accepted_at,a_.accepted_at))fail("RepairInFlightContractMismatch",r.first);}else{const auto&prior=assent(x_.agreements,r.second.terms.customer_acceptance_source);const auto&offered=offer(x_.agreements,prior.offer_ref);if(!matches_accepted_terms(offered,prior,r.second.terms))fail("RepairExistingAgreementChanged",r.first);}
  if(targets>1)fail("RepairDuplicateInFlightContract",a_.id);
  for(const auto&r:x_.agreements.common.state().facts.rows())if(r.second.domain=="commerce"){
   if(r.second.kind=="repair_service_offered"&&!s.offers.find(r.first))fail("RepairOrphanOfferHeader",r.first);
   if(r.second.kind=="repair_service_assented"&&r.first!=a_.id&&!s.assents.find(r.first))fail("RepairOrphanAssentHeader",r.first);
  }
  for(const auto&r:x_.agreements.common.state().results.rows())if(r.second&&r.second->ok&&(r.second->kind=="offer_repair_service"||r.second->kind=="accept_repair_service_offer")){
   const auto*p=r.second->details?repair_service_command_receipt(*r.second->details):nullptr;if(!p)fail("RepairSealedCommandPayloadRequired",r.second->result_id);const MomentOwnerSource seal{{"common","command_result",r.second->result_id},1};
   if(r.second->kind=="offer_repair_service"){if(!same(offer(x_.agreements,p->actual_source).actual_command_source,seal))fail("RepairOfferCommandReverseMismatch",r.second->result_id);}else if(!same(assent(x_.agreements,p->actual_source).actual_command_source,seal))fail("RepairAssentCommandReverseMismatch",r.second->result_id);
  }return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
 Status validate_original_repair_service_rules(const RepairServiceContractTerms&t,const RepairPolicy&p)const override{return base().validate_original_repair_service_rules(t,p);}
 Status validate_original_repair_service_acceptance(const RepairServiceContractTerms&t,const WorldMoment&at,const RepairServiceContractSource*c)const override{try{
  if(!same(t.customer_acceptance_source,{repair_service_assent_source(a_.id),1}))return base().validate_original_repair_service_acceptance(t,at,c);
  pending_assent(x_,o_,a_);if(compare(at,a_.accepted_at)||!matches_accepted_terms(o_,a_,t))fail("RepairInFlightAcceptedTermsMismatch",a_.id);
  if(c&&(x_.agreements.items.repair_service_contracts.contracts.find(c->id)!=c||!matches_accepted_terms(o_,a_,c->terms)||compare(c->accepted_at,at)||header(x_.agreements.common,repair_service_contract_source(c->id))<=header(x_.agreements.common,repair_service_assent_source(a_.id))))fail("RepairInFlightActualContractRequired",a_.id);return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
 Outcome<RepairServiceFinancialParties>read_original_repair_service_parties(const RepairServiceContractSource&t)const override{return base().read_original_repair_service_parties(t);}
 Outcome<RepairServicePaymentPermission>read_original_repair_payment_permission(const RepairServiceContractSource&t,Integer n,const WorldMoment&m,RepairServicePaymentUse u,const MomentOwnerSource&r)const override{return base().read_original_repair_payment_permission(t,n,m,u,r);}
 Status validate_original_repair_payment(const RepairServiceContractSource&t,const MomentOwnerSource&r,RepairServicePaymentUse u,Integer n,const WorldMoment&m)const override{return base().validate_original_repair_payment(t,r,u,n,m);}
 Status validate_original_repair_custody(const RepairServiceContractSource&t,const RepairCustodyReceivedSource&r)const override{return base().validate_original_repair_custody(t,r);}
 Outcome<std::vector<MomentOwnerSource>>read_original_repair_terminal_continuity(const RepairServiceContractSource&t,const MomentOwnerSource&r)const override{return base().read_original_repair_terminal_continuity(t,r);}
 Status validate_original_repair_return(const RepairServiceContractSource&t,const RepairItemReturnedSource&r)const override{return base().validate_original_repair_return(t,r);}
};
class InFlightRepair final:public RepairOriginalRead {
 const RepairServiceCommandContext&x_;const RepairServiceOfferSource&o_;const RepairServiceAssentSource&a_;
 const RepairOriginalRead&base()const{return x_.repair.original;}
public:InFlightRepair(const RepairServiceCommandContext&x,const RepairServiceOfferSource&o,const RepairServiceAssentSource&a):x_(x),o_(o),a_(a){}
 bool bound_to(const CommonStore&c,const ItemInstancesState&i)const noexcept override{return &c==&x_.agreements.common&&&i==&x_.agreements.items&&base().bound_to(c,i);}
 Status validate_repair_acceptance(const RepairAcceptedTerms&t,const WorldMoment&at,const RepairAcceptedSource*actual)const override{try{
  const auto*c=x_.agreements.items.repair_service_contracts.contracts.find(t.fee_terms_ref.source_ref.id);
  if(!c||!same(c->terms.customer_acceptance_source,{repair_service_assent_source(a_.id),1}))return base().validate_repair_acceptance(t,at,actual);
  pending_assent(x_,o_,a_);const auto&v=o_.inspection;if(!matches_accepted_terms(o_,a_,c->terms)||!same(t.fee_terms_ref,{repair_service_contract_source(c->id),1})||!same(t.return_terms_ref,t.fee_terms_ref)||compare(at,a_.accepted_at)||t.instance_id!=o_.quoted_terms.instance_id||t.worker_id!=o_.quoted_terms.worker_id||t.policy_ref.policy_id!=o_.quoted_terms.repair_policy_ref.policy_id||t.policy_ref.version!=o_.quoted_terms.repair_policy_ref.version||!owner(t.legal_owner,o_.quoted_terms.customer)||t.terms_revision!=o_.quoted_terms.terms_revision||!same(t.actual_contact_ref,a_.actual_contact_source)||!same(t.owner_authority_ref,o_.quoted_terms.owner_authority_source)||t.original_item.inventory_id!=v.original_item.inventory_id||t.original_item.lot_id!=v.original_item.lot_id||t.original_item.quantity!=v.original_item.quantity||!repair_detail::same_head(t.condition_head,v.condition_head)||t.workstation.provider_domain!=v.workstation.provider_domain||t.workstation.facility_id!=v.workstation.facility_id||t.workstation.slot_kind!=v.workstation.slot_kind||t.delivered_custody_source_ref)fail("RepairInFlightS9TermsMismatch",a_.id);
  if(actual&&(x_.agreements.items.repair_jobs.acceptances.find(actual->id)!=actual||compare(actual->accepted_at,at)||!same_repair_accepted_terms(actual->terms,t)||header(x_.agreements.common,repair_accepted_source(actual->id))<=header(x_.agreements.common,t.fee_terms_ref.source_ref)))fail("RepairInFlightS9AcceptanceMismatch",a_.id);return common_success();
 }catch(Failure&e){return {false,{},std::move(e.error)};}}
 Status validate_repair_unstarted_interruption(const RepairAcceptedSource&a,const RepairUnstartedInterruptionSource&s)const override{return base().validate_repair_unstarted_interruption(a,s);}
 Status validate_repair_reacceptance_after_unstarted_terminal(const RepairAcceptedSource&a,const RepairUnstartedInterruptionSource&s,const RepairAcceptedTerms&t,const WorldMoment&m,const RepairAcceptedSource*n)const override{return base().validate_repair_reacceptance_after_unstarted_terminal(a,s,t,m,n);}
 Status validate_repair_terms(const RepairTerms&t,const RepairJob*j)const override{return base().validate_repair_terms(t,j);}
 Outcome<std::vector<FixedServiceInputRef>>read_repair_start_inputs(const RepairJob&j)const override{return base().read_repair_start_inputs(j);}
 Outcome<RepairActualServiceCoverage>read_repair_service_coverage(const RepairJob&j,const std::vector<FixedServiceInputRef>&v,const MomentSpan&s)const override{return base().read_repair_service_coverage(j,v,s);}
 Outcome<std::vector<FixedServiceInputRef>>read_repair_control_inputs(const RepairJob&j,RepairControlKind k,const MomentOwnerSource&s,const WorldMoment&m)const override{return base().read_repair_control_inputs(j,k,s,m);}
 Status validate_repair_control(const RepairJob&j,const RepairControlSource&s)const override{return base().validate_repair_control(j,s);}
 Status validate_repair_completion_fees(const RepairJob&j,const RepairCompletedSource&s)const override{return base().validate_repair_completion_fees(j,s);}
 Status validate_repair_interruption(const RepairJob&j,const RepairInterruptedSource&s)const override{return base().validate_repair_interruption(j,s);}
 Outcome<CraftFacilityEligibility>read_current_repair_facility_eligibility(const RepairJob&j)const override{return base().read_current_repair_facility_eligibility(j);}
 Outcome<CraftFacilityEligibility>read_repair_facility_eligibility_at_boundary(const RepairJob&j,const CraftFacilityBoundarySource&s)const override{return base().read_repair_facility_eligibility_at_boundary(j,s);}
 Status validate_repair_return(const RepairJob&j)const override{return base().validate_repair_return(j);}
 Status validate_repair_completed_workstation_clearance(const RepairJob&j,const RepairCompletedSource&s)const override{return base().validate_repair_completed_workstation_clearance(j,s);}
};
}
struct PreparedRepairServiceCommand::Impl:CheckpointAllocatedObject {
 const RepairServiceCommandContext*context=nullptr;const CommonStore*common=nullptr;const ItemInstancesState*items=nullptr;
 StableId actor;RepairServiceCommandRequest request;WorldMoment at;std::shared_ptr<const ReadStamp>stamp;
 Impl(const RepairServiceCommandContext&x,const StableId&a,const RepairServiceCommandRequest&r,const WorldMoment&m,std::shared_ptr<const ReadStamp>s):context(&x),common(&x.agreements.common),items(&x.agreements.items),actor(a),request(r),at(m),stamp(std::move(s)){}
};
struct PendingRepairServiceCommand::Impl:CheckpointAllocatedObject {
 const RepairServiceCommandContext*context=nullptr;const CommonStore*common=nullptr;const ItemInstancesState*items=nullptr;
 WorldId world_id{};Integer execution_epoch=0;
 std::variant<RepairServiceOfferSource,RepairServiceAssentSource>row;
 RepairServiceCommandReceipt receipt;
 explicit Impl(const RepairServiceCommandContext&x):context(&x),common(&x.agreements.common),items(&x.agreements.items),world_id(x.agreements.common.state().identity.world_id),execution_epoch(x.agreements.common.state().identity.execution_epoch){}
};
PreparedRepairServiceCommand::PreparedRepairServiceCommand()=default;
PreparedRepairServiceCommand::PreparedRepairServiceCommand(std::unique_ptr<Impl>p):impl_(std::move(p)){}
PreparedRepairServiceCommand::~PreparedRepairServiceCommand()=default;
PreparedRepairServiceCommand::PreparedRepairServiceCommand(PreparedRepairServiceCommand&&)noexcept=default;
PreparedRepairServiceCommand&PreparedRepairServiceCommand::operator=(PreparedRepairServiceCommand&&)noexcept=default;
PendingRepairServiceCommand::PendingRepairServiceCommand()=default;
PendingRepairServiceCommand::PendingRepairServiceCommand(std::unique_ptr<Impl>p):impl_(std::move(p)){}
PendingRepairServiceCommand::~PendingRepairServiceCommand()=default;
PendingRepairServiceCommand::PendingRepairServiceCommand(PendingRepairServiceCommand&&)noexcept=default;
PendingRepairServiceCommand&PendingRepairServiceCommand::operator=(PendingRepairServiceCommand&&)noexcept=default;
struct RepairServiceCommandAccess {
  // 预检报价或接受请求并捕获当前读戳；上下文或来源失效时拒绝。
  static PreparedRepairServiceCommand
  prepare(const RepairServiceCommandContext &x, const StableId &actor,
          const RepairServiceCommandRequest &r) {
    bound(x);
    if (!std::holds_alternative<OfferRepairServiceRequest>(r) &&
        !std::holds_alternative<AcceptRepairServiceOfferRequest>(r))
      fail("RepairPhysicalCommandContextRequired", {},
           ErrorKind::unsupported_capability);
    take(validate_repair_service_agreements(x.agreements));
    take(validate_repair_service_contracts(x.commercial));
    const StableId *inventory = nullptr;
    const StableId *instance = nullptr;
    std::vector<StableId> sources;
    if (const auto *q = std::get_if<OfferRepairServiceRequest>(&r)) {
      prospective_values(x, actor, *q);
      inventory = &q->inspection.original_item.inventory_id;
      instance = &q->terms.instance_id;
      sources.push_back(q->actual_contact_source.source_ref.id);
    } else {
      const auto &accepted_request =
          std::get<AcceptRepairServiceOfferRequest>(r);
      const auto &o = prospective_assent(x, actor, accepted_request);
      inventory = &o.inspection.original_item.inventory_id;
      instance = &o.quoted_terms.instance_id;
      sources = {accepted_request.offer_ref.source_ref.id,
                 accepted_request.actual_contact_source.source_ref.id};
    }
    auto stamp = take(
        x.stamps.capture({{ResourceKey::Kind::actor, actor},
                          {ResourceKey::Kind::inventory, *inventory}},
                         {{"craft", "item_instance", *instance}}, sources));
    if (!stamp || (stamp->mode() != ReadMode::committed_current &&
                   stamp->mode() != ReadMode::phase_current))
      fail("RepairCommandCurrentStampRequired", actor);
    take(x.stamp_authority.validate(
        *stamp, x.agreements.common.state().identity, stamp->mode(),
        x.agreements.common, x.versions));
    return PreparedRepairServiceCommand(
        std::make_unique<PreparedRepairServiceCommand::Impl>(
            x, actor, r, context(x.agreements), std::move(stamp)));
  }
  // 在同一候选中执行已预检请求；读戳或原始协议不符时拒绝。
  static PendingRepairServiceCommand
  apply(CommonStore &c, ItemInstancesState &i,
        const RepairServiceCommandContext &x,
        PreparedRepairServiceCommand &&prepared) {
    bound(x);
    auto p = std::move(prepared.impl_);
    if (!p || p->context != &x || p->common != &c || p->items != &i ||
        &c != &x.agreements.common || &i != &x.agreements.items || !p->stamp ||
        compare(p->at, context(x.agreements)))
      fail("RepairPreparedCandidateMismatch");
    take(x.stamp_authority.validate(*p->stamp, c.state().identity,
                                    p->stamp->mode(), c, x.versions));
    take(validate_repair_service_agreements(x.agreements));
    take(validate_repair_service_contracts(x.commercial));
    auto pending = std::make_unique<PendingRepairServiceCommand::Impl>(x);
    if (const auto *q = std::get_if<OfferRepairServiceRequest>(&p->request)) {
      prospective_values(x, p->actor, *q);
      ReadStampWriteBoundary write(x.stamp_authority);
      RepairServiceOfferSource o;
      o.id = take(c.next_id("repair_service_offer"));
      o.provider_actor_id = p->actor;
      o.offered_at = p->at;
      const MomentOwnerSource source{repair_service_offer_source(o.id), 1};
      append(c, source.source_ref);
      o.quoted_terms = actual_quote(q->terms, source);
      o.inspection = q->inspection;
      o.actual_contact_source = q->actual_contact_source;
      inspection(x.agreements, o.quoted_terms, o.inspection,
                 o.actual_contact_source, o.offered_at, &source.source_ref);
      pending->receipt = {"offer_repair_service", p->actor, source, p->at, {}};
      pending->row = std::move(o);
    } else {
      const auto &accepted_request =
          std::get<AcceptRepairServiceOfferRequest>(p->request);
      const auto &o = prospective_assent(x, p->actor, accepted_request);
      ReadStampWriteBoundary write(x.stamp_authority);
      RepairServiceAssentSource a;
      a.id = take(c.next_id("repair_service_assent"));
      a.customer_actor_id = p->actor;
      a.accepted_at = p->at;
      a.offer_ref = accepted_request.offer_ref;
      a.actual_contact_source = accepted_request.actual_contact_source;
      const MomentOwnerSource source{repair_service_assent_source(a.id), 1};
      append(c, source.source_ref);
      pending_assent(x, o, a);
      InFlightCommercial commercial_original(x, o, a);
      InFlightRepair repair_original(x, o, a);
      RepairReadContext repair{
          x.repair.common,          x.repair.common_catalog,
          x.repair.items,           x.repair.properties,
          x.repair.item_catalog,    x.repair.policies,
          x.repair.elapsed,         repair_original,
          x.repair.conditions,      x.repair.clock,
          x.repair.scale,           x.repair.arithmetic_budget,
          x.repair.occupancy,       x.repair.character_sources,
          x.repair.spatial_sources, nullptr};
      RepairServiceReadContext commercial{x.commercial.common,
                                          x.commercial.items,
                                          x.commercial.economy,
                                          x.commercial.policies,
                                          commercial_original,
                                          x.commercial.clock,
                                          &repair,
                                          x.commercial.rules};
      repair.commercial_sources = &commercial;
      const auto terms = accepted_terms(o, a);
      auto result = take(accept_original_repair_service_in_candidate(
          c, i, commercial, repair, terms, o.inspection));
      pending->receipt = {"accept_repair_service_offer", p->actor, source,
                          p->at, std::move(result)};
      pending->row = std::move(a);
    }
    return PendingRepairServiceCommand(std::move(pending));
  }
 static const RepairServiceCommandReceipt*receipt(const PendingRepairServiceCommand&p){if(!p.impl_)fail("RepairPendingCommandRequired");return &p.impl_->receipt;}
 static void bind(CommonStore&c,ItemInstancesState&i,const RepairServiceCommandContext&x,PendingRepairServiceCommand&&pending,const MomentOwnerSource&seal){
  bound(x);auto p=std::move(pending.impl_);if(!p||p->context!=&x||p->common!=&c||p->items!=&i||&c!=&x.agreements.common||&i!=&x.agreements.items||p->world_id!=c.state().identity.world_id||p->execution_epoch!=c.state().identity.execution_epoch||compare(p->receipt.occurred_at,context(x.agreements)))fail("RepairPendingCandidateMismatch");
  const auto&actual=command(x.agreements,p->receipt.actor_id,p->receipt.command_kind,p->receipt.actual_source,seal,p->receipt.occurred_at);
  if(actual.accepted_result.has_value()!=p->receipt.accepted_result.has_value()||(actual.accepted_result&&(!same(actual.accepted_result->commercial_contract,p->receipt.accepted_result->commercial_contract)||!same(actual.accepted_result->repair_acceptance,p->receipt.accepted_result->repair_acceptance))))fail("RepairActualSealResultMismatch",seal.source_ref.id);
  auto actual_route=route(x.agreements,p->receipt.actor_id,p->receipt.actual_source,seal,p->receipt.occurred_at);ReadStampWriteBoundary write(x.stamp_authority);
  if(auto*o=std::get_if<RepairServiceOfferSource>(&p->row)){if(!same(p->receipt.actual_source,{repair_service_offer_source(o->id),1}))fail("RepairPendingOfferMismatch",o->id);o->actual_command_source=seal;o->actor_route=std::move(actual_route);const auto id=o->id;if(!i.repair_service_contracts.offers.insert(id,std::move(*o)))fail("DuplicateRepairActualOffer",id);}
  else{auto&a=std::get<RepairServiceAssentSource>(p->row);if(!same(p->receipt.actual_source,{repair_service_assent_source(a.id),1}))fail("RepairPendingAssentMismatch",a.id);a.actual_command_source=seal;a.actor_route=std::move(actual_route);const auto id=a.id;if(!i.repair_service_contracts.assents.insert(id,std::move(a)))fail("DuplicateRepairActualAssent",id);}
  take(validate_repair_service_agreements(x.agreements));take(validate_repair_service_contracts(x.commercial));take(validate_repair_acceptances(x.repair));
 }
};
Outcome<PreparedRepairServiceCommand>prepare_repair_service_command(const RepairServiceCommandContext&x,const StableId&a,const RepairServiceCommandRequest&r){try{return {true,RepairServiceCommandAccess::prepare(x,a,r),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("RepairCommandMemoryExhausted",{},ErrorKind::internal).error};}}
Outcome<PendingRepairServiceCommand>apply_repair_service_command_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairServiceCommandContext&x,PreparedRepairServiceCommand&&p){try{return {true,RepairServiceCommandAccess::apply(c,i,x,std::move(p)),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return {false,{},common_failure("RepairCommandMemoryExhausted",{},ErrorKind::internal).error};}}
Outcome<const RepairServiceCommandReceipt*>read_pending_repair_service_command_receipt(const PendingRepairServiceCommand&p){try{return {true,RepairServiceCommandAccess::receipt(p),{}};}catch(Failure&e){return {false,nullptr,std::move(e.error)};}}
Status bind_repair_service_command_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairServiceCommandContext&x,PendingRepairServiceCommand&&p,const MomentOwnerSource&seal){try{RepairServiceCommandAccess::bind(c,i,x,std::move(p),seal);return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){return common_failure("RepairCommandMemoryExhausted",{},ErrorKind::internal);}}
}
