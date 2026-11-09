// 当面具名邀请、本人答复及撤回的唯一写入口；接受再交真实成员/工资owner原子完成。
#include "membership_offer.hpp"
#include "named_membership_rules.hpp"
#include "office_authority.hpp"
#include "grant_authority.hpp"
#include "resident/domains/social/social_model.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
template<class T> Outcome<T> failure(const StableId& code,const StableId& path={}) { return {false,{},common_failure(code,path).error}; }
bool includes(const std::vector<StableId>& ids,const StableId& id) { return std::find(ids.begin(),ids.end(),id)!=ids.end(); }
// 场所只读实际解析位置，随行或历史计划不能替代当前在场。
bool at(const CommonStore& common,const StableId& actor,const StableId& site) {
    auto location=common.resolved_location(actor);if(!location.ok)return false;
    const auto* local=std::get_if<SettlementLocation>(&location.value);return local&&local->place_id==site;
}
// 认识须有既存社会关系、获知的真实人物来源或正在进行的当面会话。
bool acquainted(const CommonStore& common,const SocialState& social,const InteractionState& interaction,const StableId& actor,const StableId& target) {
    for(const auto& row:social.relations.rows())if(row.second.from_actor==actor&&row.second.to_actor==target)return true;
    for(const auto& session:interaction.interactions)if(session.actor_id==actor&&session.context.provider_actor_id==target)return true;
    if(!social.dynamic)return false;const auto* knowledge=social.dynamic->known_reports.find("0:"+actor);if(!knowledge)return false;
    for(const auto& entry:knowledge->rows()){
        const auto* report=social.dynamic->reports.find(entry.first);if(!report||entry.second.learned_tick>common.tick())continue;
        for(const auto& participant:report->observation.participants)if(participant.subject.kind==SubjectKind::actor&&participant.subject.id==target)return true;
    }return false;
}
// 当前会话允许当事人当面商议；第三人的会话、战斗、受伤与当前行动均不能被邀请截断。
Status contact(const CommonStore& common,const InteractionState& interaction,const ActivityRead& activity,const StableId& issuer,const StableId& target,const StableId& site) {
    const auto* a=common.actor(issuer);const auto* b=common.actor(target);
    if(!a||!b||issuer==target||!a->alive||!b->alive||!a->available||!b->available)return common_failure("MembershipPersonUnavailable");
    const auto* full_a=common.state().actors.find(issuer);const auto* full_b=common.state().actors.find(target);
    if(!full_a||!full_b||full_a->injury>0||full_b->injury>0)return common_failure("MembershipPersonUnavailable");
    if(!at(common,issuer,site)||!at(common,target,site))return common_failure("MembershipPersonAbsent");
    if(!a->action_id.empty()||!b->action_id.empty()||activity.in_open_battle(issuer)||activity.in_open_battle(target))return common_failure("MembershipPersonBusy");
    StableId allowed_speaker;
    for(const auto& session:interaction.interactions){
        const auto& speaker=session.actor_id;const auto& provider=session.context.provider_actor_id;
        if(speaker!=issuer&&speaker!=target&&provider!=issuer&&provider!=target)continue;
        if(!((speaker==issuer&&provider==target)||(speaker==target&&provider==issuer))||session.context.place_id!=site)return common_failure("MembershipPersonBusy");
        if(!allowed_speaker.empty())return common_failure("MembershipPersonBusy");allowed_speaker=speaker;
    }
    if(activity.in_interaction(issuer,allowed_speaker)||activity.in_interaction(target,allowed_speaker))return common_failure("MembershipPersonBusy");
    return common_success();
}
// 全局事实头与组织payload只登记一次，邀请及决定本身不产生金钱。
Outcome<StableId> fact(CommonStore& common,OrganizationState& state,const MembershipOffer& offer,const StableId& kind,const StableId& code,const std::vector<StableId>& causes,const StableId& actual_actor,const OrganizationCatalog& catalog,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<StableId>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};
    auto id=common.next_id("organization");if(!id.ok)return id;
    OrganizationFact value;value.id=id.value;value.kind=kind;value.organization_id=offer.organization_id;value.actor_id=actual_actor.empty()?offer.target_actor_id:actual_actor;value.office_id=offer.office_id;value.activity_id=offer.id;value.tick=now.value.half_day_tick;if(catalog.exact_fact_clock_enabled)value.occurred_at=now.value;value.code=code;value.cause_fact_ids=causes;
    auto added=common.append_fact({id.value,kind,"organization"});if(!added.ok)return {false,{},added.error};
    if(!state.facts.insert(id.value,std::move(value)))return failure<StableId>("DuplicateMembershipFact",id.value);return id;
}
// 每次真实暂缓都须等到原复评点；重复点击不延长邀请期限或免费重抽。
Status decision_boundary(const OrganizationState& state,const MembershipOffer& offer,Integer tick) {
    if(offer.revision==std::numeric_limits<Integer>::max())return common_failure("MembershipRevisionOverflow");
    const auto* prior=state.membership_decisions.find(offer.decision_fact_id);
    if(prior&&(prior->tick==tick||(prior->kind==MembershipDecisionKind::defer&&prior->next_review_tick>tick)))return common_failure("MembershipReviewNotDue");
    return common_success();
}
// 决定序列属于本人对这一邀请的历史，不能借Director或UI传入数值改写。
Outcome<Integer> next_decision_sequence(const OrganizationState& state,const StableId& offer) {
    Integer sequence=0;for(const auto& row:state.membership_decisions.rows())if(row.second.offer_id==offer)sequence=std::max(sequence,row.second.decision_seq);
    if(sequence==std::numeric_limits<Integer>::max())return failure<Integer>("MembershipDecisionOverflow");return {true,sequence+1,{}};
}
StableId answer_name(MembershipDecisionKind kind) {
    switch(kind){case MembershipDecisionKind::accept:return "accept";case MembershipDecisionKind::decline:return "decline";case MembershipDecisionKind::defer:return "defer";}return {};
}
}
bool membership_fact_known(const CommonStore& common,const SocialState& social,const StableId& actor,const StableId& id) {
    if(!social.dynamic||!common.state().facts.find(id))return false;
    const auto* knowledge=social.dynamic->known_reports.find("0:"+actor);if(!knowledge)return false;
    for(const auto& entry:knowledge->rows()){
        const auto* report=social.dynamic->reports.find(entry.first);
        if(report&&report->fact_id==id&&report->observation_tick<=common.tick()&&entry.second.learned_tick<=common.tick()&&entry.second.receiver.kind==SubjectKind::actor&&entry.second.receiver.id==actor)return true;
    }return false;
}
Outcome<MembershipOfferQuote> check_membership_offer_authority(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyCatalog& economy,const ActingAuthority& acting,const MembershipTerms& terms,const StableId& required_grant,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferQuote>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};
    if(!catalog.named_membership_enabled)return failure<MembershipOfferQuote>("NamedMembershipUnavailable");
    const auto* rate=economy.rates.find(terms.wage_rate_profile_id);const auto* wallet=common.wallet(terms.payer_wallet_id);
    if(acting.organization_id!=terms.organization_id||terms.duty_kind!="contract_executor"||terms.terminal_fraction_policy_id!="terminal_fraction_compensation_v1"||!rate||rate->per_capita||rate->numerator<0||rate->denominator<=0||terms.wage_period_ticks<=0)return failure<MembershipOfferQuote>("InvalidMembershipTerms",terms.id);
    if(!wallet||wallet->owner.kind!=Owner::Kind::organization||wallet->owner.id!=terms.organization_id)return failure<MembershipOfferQuote>("MembershipPayerMismatch",terms.payer_wallet_id);
    const Integer first_due=rate->numerator/rate->denominator;
    const Integer period_limit=first_due+(rate->numerator%rate->denominator?1:0);
    const auto* office=state.offices.find(acting.office_id);if(!office)return failure<MembershipOfferQuote>("OrganizationAuthorityDenied");
    for(const auto& grant_id:office->grant_ids){
        if(!required_grant.empty()&&grant_id!=required_grant)continue;
        std::optional<GrantRuntimeRecord> runtime;const Grant* grant=catalog.grants.find(grant_id);if(catalog.authority_lifecycle_enabled){auto original=read_current_grant(common,state,catalog,grant_id);if(!original.ok){if(original.error.kind!=ErrorKind::rule)return {false,{},original.error};continue;}runtime=std::move(original.value);grant=&runtime->terms;}
        if(!grant||!includes(grant->allowed_command_kinds,"organization_offer_membership")||!includes(grant->allowed_command_kinds,"contract_executor")||!includes(grant->wallet_ids,terms.payer_wallet_id)||!grant->may_delegate||grant->continuity_policy_id!=terms.continuity_policy_id)continue;
        auto checked=check_authority_with_single_grant(common,state,catalog,acting,"organization_offer_membership",{ScopeRef::Kind::site,terms.site_id},period_limit,*grant,clock);
        if(!checked.ok){if(checked.error.kind!=ErrorKind::rule)return {false,{},checked.error};continue;}
        MembershipOfferQuote quote;quote.grant_id=grant_id;quote.terms_id=terms.id;quote.first_period_due=first_due;return {true,std::move(quote),{}};
    }return failure<MembershipOfferQuote>("MembershipAuthorityDenied");
}
Outcome<MembershipOfferQuote> preview_organization_offer_membership(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyCatalog& economy,const SocialState& social,const InteractionState& interaction,const ActivityRead& activity,const StableId& actor,const OrganizationOfferMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferQuote>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    if(!catalog.named_membership_enabled)return failure<MembershipOfferQuote>("NamedMembershipUnavailable");
    if(actor!=command.authority.actor_id||!common.actor(actor))return failure<MembershipOfferQuote>("MembershipCommandActorMismatch");
    const auto* terms=catalog.membership_terms.find(command.terms_id);if(!terms||!common.actor(command.target_actor_id))return failure<MembershipOfferQuote>("UnknownMembershipTargetOrTerms");
    if(command.context.input_error||command.context.encounter_id||(command.context.kind&&*command.context.kind!="narrative")||command.context.place_id!=terms->site_id||command.context.provider_actor_id!=command.target_actor_id||!command.context.role_id.empty())return failure<MembershipOfferQuote>("InvalidMembershipInteraction");
    for(const auto& row:state.membership_offers.rows())if(row.second.status==MembershipOfferStatus::pending&&row.second.organization_id==terms->organization_id&&row.second.target_actor_id==command.target_actor_id&&row.second.terms_id==terms->id&&actual_tick<row.second.expires_tick)return failure<MembershipOfferQuote>("MembershipOfferAlreadyPending");
    if(active_membership_episode(state,command.target_actor_id,terms->organization_id))return failure<MembershipOfferQuote>("MembershipAlreadyActive");
    auto quote=check_membership_offer_authority(common,state,catalog,economy,command.authority,*terms,{},clock);if(!quote.ok)return quote;
    auto visible=contact(common,interaction,activity,actor,command.target_actor_id,terms->site_id);if(!visible.ok)return {false,{},visible.error};
    if(!acquainted(common,social,interaction,actor,command.target_actor_id))return failure<MembershipOfferQuote>("MembershipPersonNotKnown");
    if(terms->offer_duration_ticks<1||actual_tick>std::numeric_limits<Integer>::max()-terms->offer_duration_ticks)return failure<MembershipOfferQuote>("MembershipTickOverflow");
    quote.value.offer_revision=1;quote.value.expires_tick=actual_tick+terms->offer_duration_ticks;return quote;
}
Outcome<MembershipOfferQuote> preview_organization_answer_membership(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyState&,const LocalEconomyCatalog& economy,const SocialState& social,const InteractionState& interaction,const ActivityRead& activity,const MembershipReleaseRead& release,const StableId& actor,const OrganizationAnswerMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferQuote>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    if(!catalog.named_membership_enabled)return failure<MembershipOfferQuote>("NamedMembershipUnavailable");
    if(actor!=command.actor_id||!common.actor(actor))return failure<MembershipOfferQuote>("MembershipCommandActorMismatch");
    const auto* offer=state.membership_offers.find(command.offer_id);
    if(!offer||offer->target_actor_id!=actor)return failure<MembershipOfferQuote>("MembershipOfferUnavailable");
    if(offer->revision!=command.expected_offer_revision)return failure<MembershipOfferQuote>("StaleMembershipOffer");
    if(offer->status!=MembershipOfferStatus::pending)return failure<MembershipOfferQuote>("MembershipOfferClosed");
    if(actual_tick<offer->created_tick||actual_tick>=offer->expires_tick)return failure<MembershipOfferQuote>("MembershipOfferExpired");
    const auto* terms=catalog.membership_terms.find(offer->terms_id);if(!terms)return failure<MembershipOfferQuote>("UnknownMembershipTerms");
    auto quote=check_membership_offer_authority(common,state,catalog,economy,{offer->organization_id,offer->office_id,offer->authority_epoch,offer->issuer_actor_id},*terms,offer->grant_id,clock);if(!quote.ok)return quote;
    auto visible=contact(common,interaction,activity,offer->issuer_actor_id,actor,terms->site_id);if(!visible.ok)return {false,{},visible.error};
    if(command.source_fact_id!=offer->source_fact_id||!membership_fact_known(common,social,actor,command.source_fact_id))return failure<MembershipOfferQuote>("MembershipInvitationNotKnown");
    if(answer_name(command.answer).empty())return failure<MembershipOfferQuote>("InvalidMembershipAnswer");
    auto boundary=decision_boundary(state,*offer,actual_tick);if(!boundary.ok)return {false,{},boundary.error};
    auto sequence=next_decision_sequence(state,offer->id);if(!sequence.ok)return {false,{},sequence.error};
    quote.value.offer_id=offer->id;quote.value.offer_revision=offer->revision;quote.value.expires_tick=offer->expires_tick;
    if(command.answer==MembershipDecisionKind::accept){
        auto joined=preview_named_membership_join(common,state,catalog,economy,release,offer->id,actual_tick,clock);if(!joined.ok)return {false,{},joined.error};quote.value.reason="AcceptedExactTerms";
    }else if(command.answer==MembershipDecisionKind::decline)quote.value.reason="DeclinedExactTerms";
    else {
        auto blockers=release.membership_release_view(actor);if(!blockers.ok)return {false,{},blockers.error};
        if(blockers.value.actor_id!=actor||blockers.value.tick!=actual_tick||blockers.value.revision!=common.revision())return failure<MembershipOfferQuote>("StaleMembershipReleaseView");
        quote.value.reason=blockers.value.blockers.empty()?"NeedsMoreTime":"ExistingCommitment";
        const Integer interval=1;
        if(interval<1||actual_tick>std::numeric_limits<Integer>::max()-interval)return failure<MembershipOfferQuote>("MembershipTickOverflow");quote.value.next_review_tick=actual_tick+interval;
    }return quote;
}
Outcome<MembershipOfferQuote> preview_organization_withdraw_membership(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyCatalog& economy,const StableId& actor,const OrganizationWithdrawMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferQuote>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    if(!catalog.named_membership_enabled)return failure<MembershipOfferQuote>("NamedMembershipUnavailable");
    if(actor!=command.authority.actor_id||!common.actor(actor))return failure<MembershipOfferQuote>("MembershipCommandActorMismatch");
    const auto* offer=state.membership_offers.find(command.offer_id);if(!offer||offer->organization_id!=command.authority.organization_id||offer->office_id!=command.authority.office_id)return failure<MembershipOfferQuote>("MembershipOfferUnavailable");
    if(offer->revision!=command.expected_offer_revision)return failure<MembershipOfferQuote>("StaleMembershipOffer");
    if(offer->status!=MembershipOfferStatus::pending)return failure<MembershipOfferQuote>("MembershipOfferClosed");
    if(actual_tick>=offer->expires_tick)return failure<MembershipOfferQuote>("MembershipOfferExpired");
    if(offer->revision==std::numeric_limits<Integer>::max())return failure<MembershipOfferQuote>("MembershipRevisionOverflow");
    const auto* terms=catalog.membership_terms.find(offer->terms_id);if(!terms)return failure<MembershipOfferQuote>("UnknownMembershipTerms");
    auto quote=check_membership_offer_authority(common,state,catalog,economy,command.authority,*terms,{},clock);if(quote.ok){quote.value.offer_id=offer->id;quote.value.offer_revision=offer->revision;quote.value.expires_tick=offer->expires_tick;}return quote;
}
Outcome<MembershipOfferEffects> apply_organization_offer_membership(CommonStore& common,OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyCatalog& economy,const SocialState& social,const InteractionState& interaction,const ActivityRead& activity,const StableId& actor,const OrganizationOfferMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferEffects>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    auto quote=preview_organization_offer_membership(common,state,catalog,economy,social,interaction,activity,actor,command,clock);if(!quote.ok)return {false,{},quote.error};
    auto id=common.next_id("membership_offer");if(!id.ok)return {false,{},id.error};MembershipOffer offer;
    offer.id=id.value;offer.organization_id=command.authority.organization_id;offer.issuer_actor_id=actor;offer.office_id=command.authority.office_id;offer.grant_id=quote.value.grant_id;offer.authority_epoch=command.authority.authority_epoch;offer.target_actor_id=command.target_actor_id;offer.terms_id=command.terms_id;offer.created_tick=actual_tick;offer.expires_tick=quote.value.expires_tick;
    auto source=fact(common,state,offer,"membership_offered","ExactTermsOffered",{},{},catalog,clock);if(!source.ok)return {false,{},source.error};offer.source_fact_id=source.value;
    if(!state.membership_offers.insert(offer.id,offer))return failure<MembershipOfferEffects>("DuplicateMembershipOffer");
    MembershipOfferEffects effects;effects.offer_id=offer.id;effects.fact_ids={source.value};effects.receipt_ids=effects.fact_ids;return {true,std::move(effects),{}};
}
Outcome<MembershipOfferEffects> apply_organization_answer_membership(CommonStore& common,OrganizationState& state,const OrganizationCatalog& catalog,LocalEconomyState& economy,const LocalEconomyCatalog& economy_catalog,const SocialState& social,const InteractionState& interaction,const ActivityRead& activity,const MembershipReleaseRead& release,const StableId& actor,const OrganizationAnswerMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferEffects>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    auto quote=preview_organization_answer_membership(common,state,catalog,economy,economy_catalog,social,interaction,activity,release,actor,command,clock);if(!quote.ok)return {false,{},quote.error};
    auto* offer=state.membership_offers.find(command.offer_id);auto sequence=next_decision_sequence(state,offer->id);if(!sequence.ok)return {false,{},sequence.error};
    auto source=fact(common,state,*offer,"membership_decided",answer_name(command.answer),{offer->source_fact_id},{},catalog,clock);if(!source.ok)return {false,{},source.error};
    MembershipDecision decision;decision.id=source.value;decision.offer_id=offer->id;decision.target_actor_id=actor;decision.offer_revision=offer->revision;decision.kind=command.answer;decision.reason=quote.value.reason;decision.source_fact_ids={offer->source_fact_id};decision.tick=actual_tick;decision.decision_seq=sequence.value;decision.next_review_tick=quote.value.next_review_tick;
    if(!state.membership_decisions.insert(decision.id,decision))return failure<MembershipOfferEffects>("DuplicateMembershipDecision");
    MembershipOfferEffects effects;effects.offer_id=offer->id;effects.decision_id=decision.id;effects.fact_ids={decision.id};effects.receipt_ids=effects.fact_ids;
    if(command.answer==MembershipDecisionKind::accept){
        auto joined=join_named_membership(common,state,catalog,economy,economy_catalog,release,offer->id,decision.id,actual_tick,clock);if(!joined.ok)return {false,{},joined.error};
        effects.member_episode_id=joined.value.member_episode_id;effects.fact_ids.insert(effects.fact_ids.end(),joined.value.fact_ids.begin(),joined.value.fact_ids.end());effects.receipt_ids.insert(effects.receipt_ids.end(),joined.value.receipt_ids.begin(),joined.value.receipt_ids.end());
    }else {offer=state.membership_offers.find(command.offer_id);offer->decision_fact_id=decision.id;++offer->revision;if(command.answer==MembershipDecisionKind::decline)offer->status=MembershipOfferStatus::declined;}
    return {true,std::move(effects),{}};
}
Outcome<MembershipOfferEffects> apply_organization_withdraw_membership(CommonStore& common,OrganizationState& state,const OrganizationCatalog& catalog,const LocalEconomyCatalog& economy,const StableId& actor,const OrganizationWithdrawMembershipCommand& command,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferEffects>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};
    auto quote=preview_organization_withdraw_membership(common,state,catalog,economy,actor,command,clock);if(!quote.ok)return {false,{},quote.error};auto* offer=state.membership_offers.find(command.offer_id);
    auto source=fact(common,state,*offer,"membership_withdrawn","withdrawn",{offer->source_fact_id},actor,catalog,clock);if(!source.ok)return {false,{},source.error};offer->status=MembershipOfferStatus::withdrawn;offer->decision_fact_id=source.value;++offer->revision;
    MembershipOfferEffects effects;effects.offer_id=offer->id;effects.fact_ids={source.value};effects.receipt_ids=effects.fact_ids;return {true,std::move(effects),{}};
}
Outcome<MembershipOfferEffects> expire_membership_offers(CommonStore& common,OrganizationState& state,const OrganizationCatalog& catalog,Integer tick,const OrganizationAuthorityClock* clock) {
    if(catalog.exact_fact_clock_enabled&&!clock)return failure<MembershipOfferEffects>("MembershipActualClockRequired");auto now=read_organization_authority_now(common,clock);if(!now.ok)return {false,{},now.error};const auto actual_tick=now.value.half_day_tick;
    if(!catalog.named_membership_enabled)return {true,{}, {}};if(tick!=actual_tick)return failure<MembershipOfferEffects>("MembershipTickMismatch");
    MembershipOfferEffects effects;for(auto& row:state.membership_offers.rows()){
        auto& offer=row.second;if(offer.status!=MembershipOfferStatus::pending||tick<offer.expires_tick)continue;
        if(offer.revision==std::numeric_limits<Integer>::max())return failure<MembershipOfferEffects>("MembershipRevisionOverflow");
        auto source=fact(common,state,offer,"membership_expired","expired",{offer.source_fact_id},{},catalog,clock);if(!source.ok)return {false,{},source.error};offer.status=MembershipOfferStatus::expired;offer.decision_fact_id=source.value;++offer.revision;effects.fact_ids.push_back(source.value);effects.receipt_ids.push_back(source.value);
    }return {true,std::move(effects),{}};
}
std::optional<DynamicFactView> membership_offer_dynamic_fact(const OrganizationState& state,const StableId& id) {
    const auto* source=state.facts.find(id);if(!source||(source->kind!="membership_offered"&&source->kind!="membership_decided"&&source->kind!="membership_withdrawn"&&source->kind!="membership_expired"))return {};
    const auto* offer=state.membership_offers.find(source->activity_id);if(!offer)return {};
    DynamicFactView out;out.id=id;out.kind=source->kind;out.rule_version="named_membership_v1";out.tick=source->tick;out.cause_fact_ids=source->cause_fact_ids;out.subject_id=offer->target_actor_id;out.outcome=source->code;
    out.participants={{{SubjectKind::organization,offer->organization_id},"principal"},{{SubjectKind::actor,offer->issuer_actor_id},"issuer"},{{SubjectKind::actor,offer->target_actor_id},"recipient"}};
    if(source->kind=="membership_withdrawn"&&source->actor_id!=offer->issuer_actor_id&&source->actor_id!=offer->target_actor_id)out.participants.push_back({{SubjectKind::actor,source->actor_id},"withdrawing_officer"});
    return out;
}
}
