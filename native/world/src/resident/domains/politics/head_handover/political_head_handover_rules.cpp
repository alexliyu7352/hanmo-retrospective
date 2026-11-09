// 原首任的明确让职和候选本人接受分别真实提交；接受时重报价后一次更新原Office和epoch。
#include "political_head_handover_detail.hpp"
namespace hanmo::resident {namespace {
// 报价逐字段绑定本次原命令，不能把拒绝换为接受或替换原候选/任职版本。
bool same(const PoliticalHeadHandoverCommand& a,const PoliticalHeadHandoverCommand& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& left){using T=std::decay_t<decltype(left)>;const auto& right=std::get<T>(b);if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>)return std::tie(left.offer_fact_id,left.accept,left.expected_office_epoch)==std::tie(right.offer_fact_id,right.accept,right.expected_office_epoch);else{const auto& x=left.authority;const auto& y=right.authority;if(std::tie(x.organization_id,x.office_id,x.actor_id,x.authority_epoch)!=std::tie(y.organization_id,y.office_id,y.actor_id,y.authority_epoch))return false;if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>)return std::tie(left.policy_id,left.candidate_actor_id,left.expected_office_epoch)==std::tie(right.policy_id,right.candidate_actor_id,right.expected_office_epoch);else return left.offer_fact_id==right.offer_fact_id;}},a);}
// 即使本人过去签过，今日接受时也须原首任仍持同一有效专项权源，不能借死亡后的空职执行。
Status issuer(const PoliticalHeadHandoverContext& x,const PoliticalHeadHandoverPolicy& p,const PoliticalSuccessionPolicy& original,const ActingAuthority& a,const StableId& kind){using namespace head_handover_detail;if(a.organization_id!=original.organization_id||a.office_id!=original.leader_office_id)return common_failure("PoliticalHeadHandoverOriginalLeaderRequired");auto v=original_office(x,original,a.actor_id,a.authority_epoch,x.common.tick(),{});if(!v.ok)return v;v=member_now(x,original,a.actor_id);if(!v.ok)return v;if(x.state.version==2){auto proof=read_current_head_handover_authority2(x,p,original,a,kind);return {proof.ok,{},proof.error};}auto proof=check_authority(x.common,x.organizations,x.organization_catalog,a,kind,{ScopeRef::Kind::site,original.seat_place_id},0);if(!proof.ok)return {false,{},proof.error};return grant(x,p,original,p.handover_grant_id,kind,x.common.tick());}
}
Outcome<PoliticalHeadHandoverQuote> preview_political_head_handover(const PoliticalHeadHandoverContext& x,const StableId& actor,const PoliticalHeadHandoverCommand& command){
 using namespace head_handover_detail;if(x.state.version==1&&x.organization_catalog.authority_lifecycle_enabled)return {false,{},common_failure("HeadHandoverTime2ProfileRequired").error};auto fail=[](const Status& v){return Outcome<PoliticalHeadHandoverQuote>{false,{},v.error};};if(actor!=x.common.state().metadata.player_actor_id)return fail(common_failure("PoliticalHeadHandoverPlayerRequired"));auto v=validate_political_head_handover_sources(x);if(!v.ok)return fail(v);auto actual_now=political_head_handover_now(x);if(!actual_now.ok)return {false,{},actual_now.error};const auto now=actual_now.value;
 auto checked=std::visit([&](const auto& c)->Status{using T=std::decay_t<decltype(c)>;const PoliticalHeadHandoverOffer* offer=nullptr;const PoliticalHeadHandoverPolicy* p=nullptr;
  if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){p=x.catalog.policies.find(c.policy_id);if(!p||c.authority.actor_id!=actor)return common_failure("PoliticalHeadHandoverOriginalPolicyRequired");if(x.state.offers.size()>=4096)return common_failure("PoliticalHeadHandoverHistoryCapacity");}
  else{offer=x.state.offers.find(c.offer_fact_id);if(!offer||(std::is_same_v<T,PoliticalAnswerHeadHandover>?offer->candidate_actor_id:offer->issuer_actor_id)!=actor)return common_failure("PoliticalHeadHandoverNotKnown");p=x.catalog.policies.find(offer->policy_id);if(!p)return common_failure("PoliticalHeadHandoverOriginalPolicyRequired");if(closed(x.state,offer->id))return common_failure("PoliticalHeadHandoverAlreadyClosed");if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){if(x.state.answers.size()>=4096)return common_failure("PoliticalHeadHandoverHistoryCapacity");}else if(x.state.withdrawals.size()>=4096)return common_failure("PoliticalHeadHandoverHistoryCapacity");}
  const auto* original=x.succession_catalog.policies.find(p->succession_policy_id);if(!original)return common_failure("PoliticalHeadHandoverOriginalPolicyRequired");auto valid=contact(x,*original,actor);if(!valid.ok)return valid;valid=seat_source(x,*original,now.half_day_tick,{});if(!valid.ok)return valid;
  if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){
   if(c.candidate_actor_id==actor)return common_failure("PoliticalHeadHandoverDistinctCandidateRequired");if(c.expected_office_epoch!=c.authority.authority_epoch)return common_failure("PoliticalHeadHandoverOfficeVersionChanged");valid=issuer(x,*p,*original,c.authority,political_head_handover_offer_command);if(!valid.ok)return valid;valid=contact(x,*original,c.candidate_actor_id);if(!valid.ok)return valid;
   for(const auto& id:{actor,c.candidate_actor_id}){valid=x.state.version==2?validate_head_handover_person2(x,id,{}):growth_independent_office_age(x.common,id);if(!valid.ok)return valid;valid=member_now(x,*original,id);if(!valid.ok)return valid;}
   if(now.half_day_tick>std::numeric_limits<Integer>::max()-p->response_ticks)return common_failure("PoliticalHeadHandoverDeadlineOverflow");for(const auto& row:x.state.offers.rows())if(row.second.policy_id==p->id&&!closed(x.state,row.first)){auto open=political_head_handover_response_open(x,row.second,now);if(!open.ok)return {false,{},open.error};if(open.value)return common_failure("PoliticalHeadHandoverOfferAlreadyPending");}
  }else if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){
   auto open=political_head_handover_response_open(x,*offer,now);if(!open.ok)return {false,{},open.error};if(!open.value)return common_failure("PoliticalHeadHandoverOfferExpired");if(c.expected_office_epoch!=offer->office_epoch)return common_failure("PoliticalHeadHandoverOfficeVersionChanged");
   if(c.accept){valid=contact(x,*original,offer->issuer_actor_id);if(!valid.ok)return valid;valid=issuer(x,*p,*original,{offer->organization_id,offer->office_id,offer->office_epoch,offer->issuer_actor_id},political_head_handover_offer_command);if(!valid.ok)return valid;if(offer->office_epoch==std::numeric_limits<Integer>::max())return common_failure("AuthorityEpochOverflow");valid=x.state.version==2?validate_head_handover_person2(x,actor,{}):growth_independent_office_age(x.common,actor);if(!valid.ok)return valid;valid=member_now(x,*original,actor);if(!valid.ok)return valid;}
  }else{if(c.authority.actor_id!=actor||c.authority.authority_epoch!=offer->office_epoch)return common_failure("PoliticalHeadHandoverOriginalLeaderRequired");valid=issuer(x,*p,*original,c.authority,political_head_handover_withdraw_command);if(!valid.ok)return valid;}
  return common_success();
 },command);return checked.ok?Outcome<PoliticalHeadHandoverQuote>{true,{x.common.state().identity,actor,command},{}}:fail(checked);
}
// 两领域副本在全部头/载荷写成后统一发布；Common晚失败由调用者的同候选保存点回滚。
Outcome<StableId> apply_political_head_handover_in_candidate(CommonStore& common,OrganizationState& organizations,PoliticalHeadHandoverState& state,const PoliticalHeadHandoverContext& x,const StableId& actor,const PoliticalHeadHandoverCommand& command,const PoliticalHeadHandoverQuote& quote,OrganizationRuntime* exact_runtime){
 if(state.version==2){if(!exact_runtime)return {false,{},common_failure("HeadHandoverTime2RuntimeRequired").error};return apply_political_head_handover2_in_candidate(common,organizations,*exact_runtime,state,x,actor,command,quote);}
 const auto& a=common.state().identity;const auto& b=quote.identity;if(&common!=&x.common||&organizations!=&x.organizations||&state!=&x.state||quote.actor_id!=actor||!same(command,quote.command)||std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)!=std::tie(b.world_id,b.execution_epoch,b.revision,b.tick))return {false,{},common_failure("StalePoliticalHeadHandoverQuote").error};auto fresh=preview_political_head_handover(x,actor,command);if(!fresh.ok)return {false,{},fresh.error};auto next=state;auto next_org=organizations;
 auto result=std::visit([&](const auto& c)->Outcome<StableId>{using T=std::decay_t<decltype(c)>;StableId prefix,kind;if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){prefix="political_head_handover_offer";kind="political_head_handover_offered";}else if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){prefix=c.accept?"political_head_handover_acceptance":"political_head_handover_decline";kind=c.accept?"political_head_handover_accepted":"political_head_handover_declined";}else{prefix="political_head_handover_withdrawal";kind="political_head_handover_withdrawn";}auto id=common.next_id(prefix);if(!id.ok)return id;auto header=common.append_fact({id.value,kind,political_head_handover_domain});if(!header.ok)return {false,{},header.error};
  if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){const auto& p=*x.catalog.policies.find(c.policy_id);const auto& old=*x.succession_catalog.policies.find(p.succession_policy_id);PoliticalHeadHandoverOffer row;row.id=id.value;row.policy_id=p.id;row.organization_id=old.organization_id;row.office_id=old.leader_office_id;row.issuer_actor_id=actor;row.candidate_actor_id=c.candidate_actor_id;row.grant_id=p.handover_grant_id;row.office_epoch=c.authority.authority_epoch;row.issued_tick=common.tick();row.response_until=common.tick()+p.response_ticks;if(!next.offers.insert(row.id,row))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};}
  else if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){const auto& old=*state.offers.find(c.offer_fact_id);PoliticalHeadHandoverAnswer row;row.id=id.value;row.offer_fact_id=old.id;row.actor_id=actor;row.tick=common.tick();row.expected_office_epoch=c.expected_office_epoch;row.accepted=c.accept;
   if(c.accept){auto change=common.next_id("organization");if(!change.ok)return change;header=common.append_fact({change.value,"organization_office_changed","organization"});if(!header.ok)return {false,{},header.error};auto* office=next_org.offices.find(old.office_id);if(!office||office->holder_actor_id!=std::optional<StableId>{old.issuer_actor_id}||office->epoch!=old.office_epoch||office->status!=OfficeStatus::active||office->deputy_actor_id)return {false,{},common_failure("PoliticalHeadHandoverOfficeVersionChanged").error};office->holder_actor_id=actor;++office->epoch;row.office_change_fact_id=change.value;row.installed_office_epoch=office->epoch;OrganizationFact f;f.id=change.value;f.kind="organization_office_changed";f.organization_id=old.organization_id;f.office_id=old.office_id;f.actor_id=actor;f.tick=common.tick();f.cause_fact_ids={id.value};if(!next_org.facts.insert(f.id,f))return {false,{},common_failure("DuplicatePoliticalHeadHandoverOfficeChange").error};}
   if(!next.answers.insert(row.id,row))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};
  }else{const auto& old=*state.offers.find(c.offer_fact_id);PoliticalHeadHandoverWithdrawal row;row.id=id.value;row.offer_fact_id=old.id;row.actor_id=actor;row.grant_id=old.grant_id;row.office_epoch=c.authority.authority_epoch;row.tick=common.tick();if(!next.withdrawals.insert(row.id,row))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};}return id;
 },command);if(!result.ok)return result;organizations=std::move(next_org);state=std::move(next);return result;
}
// /2 keeps the original offer/consent command, but issues its exact original authority and
// consumes the one Organization office writer. The caller's unpublished savepoint owns
// Common, Organization, OrganizationRuntime and this source together on every failure.
Outcome<StableId> apply_political_head_handover2_in_candidate(CommonStore& common,OrganizationState& org,
 OrganizationRuntime& runtime,PoliticalHeadHandoverState& state,const PoliticalHeadHandoverContext& x,
 const StableId& actor,const PoliticalHeadHandoverCommand& command,const PoliticalHeadHandoverQuote& quote){
 const auto& a=common.state().identity;const auto& b=quote.identity;
 if(&common!=&x.common||&org!=&x.organizations||&state!=&x.state||state.version!=2||!x.exact||
    &runtime!=&x.exact->runtime||quote.actor_id!=actor||!same(command,quote.command)||
    std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)!=std::tie(b.world_id,b.execution_epoch,b.revision,b.tick))
  return {false,{},common_failure("StalePoliticalHeadHandoverQuote").error};
 auto fresh=preview_political_head_handover(x,actor,command);if(!fresh.ok)return {false,{},fresh.error};
 auto now=political_head_handover_now(x);if(!now.ok)return {false,{},now.error};
 // Player result/history projects this same actual command moment, never a later phase.
 if(now.value.half_day_tick!=common.tick())return {false,{},common_failure("HeadHandoverPlayerMomentMismatch").error};
 auto result=std::visit([&](const auto& c)->Outcome<StableId>{using T=std::decay_t<decltype(c)>;
  const PoliticalHeadHandoverOffer* old=nullptr;const PoliticalHeadHandoverPolicy* policy=nullptr;
  StableId prefix,kind,authority_command;ActingAuthority signer;bool uses_authority=true;
  if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){policy=x.catalog.policies.find(c.policy_id);signer=c.authority;
   prefix="political_head_handover_offer";kind="political_head_handover_offered";authority_command=political_head_handover_offer_command;
  }else{old=state.offers.find(c.offer_fact_id);policy=old?x.catalog.policies.find(old->policy_id):nullptr;
   if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){uses_authority=c.accept;
    prefix=c.accept?"political_head_handover_acceptance":"political_head_handover_decline";
    kind=c.accept?"political_head_handover_accepted":"political_head_handover_declined";
    if(old)signer={old->organization_id,old->office_id,old->office_epoch,old->issuer_actor_id};
    authority_command=political_head_handover_offer_command;
   }else{prefix="political_head_handover_withdrawal";kind="political_head_handover_withdrawn";signer=c.authority;authority_command=political_head_handover_withdraw_command;}
  }
  const auto* base=policy?x.succession_catalog.policies.find(policy->succession_policy_id):nullptr;
  if(!policy||!base)return {false,{},common_failure("PoliticalHeadHandoverOriginalPolicyRequired").error};
  std::optional<SingleGrantAuthorityRead> proof;
  if(uses_authority){auto read=read_current_head_handover_authority2(x,*policy,*base,signer,authority_command);if(!read.ok)return {false,{},read.error};proof=std::move(read.value);}
  auto id=common.next_id(prefix);if(!id.ok)return id;SourceRef ref{political_head_handover_domain,kind,id.value};
  auto appended=common.append_fact({id.value,kind,political_head_handover_domain});if(!appended.ok)return {false,{},appended.error};
  PoliticalHeadHandoverExactIssue exact{now.value,{}};if(uses_authority)exact.authority_basis=AuthorityBasisRef{ref,1};
  if constexpr(std::is_same_v<T,PoliticalOfferHeadHandover>){PoliticalHeadHandoverOffer row;
   row.id=id.value;row.policy_id=policy->id;row.organization_id=base->organization_id;row.office_id=base->leader_office_id;
   row.issuer_actor_id=actor;row.candidate_actor_id=c.candidate_actor_id;row.grant_id=policy->handover_grant_id;
   row.office_epoch=signer.authority_epoch;row.issued_tick=now.value.half_day_tick;row.response_until=row.issued_tick+policy->response_ticks;row.exact_issue=exact;
   if(!state.offers.insert(id.value,std::move(row)))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};
  }else if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){PoliticalHeadHandoverAnswer row;
   row.id=id.value;row.offer_fact_id=old->id;row.actor_id=actor;row.tick=now.value.half_day_tick;
   row.expected_office_epoch=c.expected_office_epoch;row.accepted=c.accept;row.exact_issue=exact;
   if(!state.answers.insert(id.value,std::move(row)))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};
  }else{PoliticalHeadHandoverWithdrawal row;row.id=id.value;row.offer_fact_id=old->id;row.actor_id=actor;
   row.grant_id=old->grant_id;row.office_epoch=signer.authority_epoch;row.tick=now.value.half_day_tick;row.exact_issue=exact;
   if(!state.withdrawals.insert(id.value,std::move(row)))return {false,{},common_failure("DuplicatePoliticalHeadHandoverSource").error};}
  if(uses_authority){NativeHeadHandoverAuthorityRead original(common,state,x.exact->clock);
   SingleGrantAuthorityRequest request{signer,authority_command,{ScopeRef::Kind::site,base->seat_place_id},0,{},false,{}};
   auto registered=register_authority_basis_for_fact(common,runtime.authority_bases,org,x.organization_catalog,
    x.organization_catalog.authority_catalog_identity,request,*proof,ref,original);
   if(!registered.ok)return {false,{},registered.error};
  }
  if constexpr(std::is_same_v<T,PoliticalAnswerHeadHandover>){if(c.accept){
   auto change=apply_head_handover_office2(common,org,runtime,state,x,id.value);if(!change.ok)return {false,{},change.error};
   if(!change.value.changed||!change.value.fact_id)return {false,{},common_failure("HeadHandoverActualOfficeResultRequired").error};
   auto* row=state.answers.find(id.value);row->office_change_fact_id=*change.value.fact_id;row->installed_office_epoch=row->expected_office_epoch+1;
  }}
  return id;
 },command);
 if(!result.ok)return result;auto valid=validate_political_head_handover_sources(x);return valid.ok?result:Outcome<StableId>{false,{},valid.error};
}

}
