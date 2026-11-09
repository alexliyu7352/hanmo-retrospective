// 就任只安装真实被接受的候选人到原空职；不续签旧军令、私产、公库义务或玩家控制。
#include "political_succession_accession.hpp"
#include "political_succession_nomination_detail.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
namespace hanmo::resident {
Outcome<PoliticalSuccessionAccessionQuote> preview_political_succession_accession(const PoliticalSuccessionAccessionContext& ctx,const StableId& actor,const PoliticalAccedeSuccession& command){
 const auto& x=ctx.sources.nominations;auto fail=[](const Status& error){return Outcome<PoliticalSuccessionAccessionQuote>{false,{},error.error};};
 if(actor!=x.common.state().metadata.player_actor_id)return fail(common_failure("PoliticalSuccessionPlayerRequired"));
 const auto* accepted=x.state.answers.find(command.acceptance_fact_id);if(!accepted||!accepted->accepted||accepted->actor_id!=actor)return fail(common_failure("PoliticalSuccessionNotKnown"));
 auto valid=validate_political_succession_accessions(ctx);if(!valid.ok)return fail(valid);
 for(const auto& row:ctx.state.accessions.rows())if(row.second.acceptance_fact_id==accepted->id)return fail(common_failure("PoliticalSuccessionAlreadyAcceded"));
 auto source=read_political_succession_accession_basis(ctx.sources,command.acceptance_fact_id,command.death_fact_id,command.death_notice_binding_fact_id);if(!source.ok)return {false,{},source.error};const auto& basis=source.value;
 const auto* policy=x.catalog.policies.find(basis.policy_id);if(!policy||basis.successor_actor_id!=actor||basis.vacant_office_epoch!=command.expected_office_epoch||basis.vacant_office_epoch==std::numeric_limits<Integer>::max())return fail(common_failure("PoliticalSuccessionOfficeVersionChanged"));
 valid=succession_detail::contact(x,*policy,actor);if(!valid.ok)return fail(valid);valid=growth_independent_office_age(x.common,actor);if(!valid.ok)return fail(valid);valid=x.seat(*policy,x.common.tick(),{});if(!valid.ok)return fail(valid);
 // 唯一首职空缺可能使组织limited；本人原有效成员资格仍从原episode读取，不临时伪改组织来通过authorize。
 const auto* org=x.organizations.organizations.find(policy->organization_id);const auto* member=active_membership_episode(x.organizations,actor,policy->organization_id);
 if(!org||(org->status!=OrganizationStatus::active&&org->status!=OrganizationStatus::limited)||!member||(member->response!=MemberResponse::serving&&member->response!=MemberResponse::payment_requested))return fail(common_failure("PoliticalSuccessionMemberUnavailable"));
 return {true,{x.common.state().identity,actor,command,std::move(source.value)},{}};
}
// 先重报价；两新账只在原Common头均写成后发布，资产和其他岗位完全不动。
Outcome<StableId> apply_political_succession_accession_in_candidate(CommonStore& common,OrganizationState& organizations,PoliticalSuccessionAccessionState& state,const PoliticalSuccessionAccessionContext& ctx,const StableId& actor,const PoliticalAccedeSuccession& command,const PoliticalSuccessionAccessionQuote& quote){
 const auto& x=ctx.sources.nominations;const auto& a=common.state().identity;const auto& b=quote.identity;const auto& original=quote.command;
 if(&common!=&x.common||&organizations!=&x.organizations||&state!=&ctx.state||x.accessions!=&state||quote.actor_id!=actor||std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)!=std::tie(b.world_id,b.execution_epoch,b.revision,b.tick)||std::tie(command.acceptance_fact_id,command.death_fact_id,command.death_notice_binding_fact_id,command.expected_office_epoch)!=std::tie(original.acceptance_fact_id,original.death_fact_id,original.death_notice_binding_fact_id,original.expected_office_epoch))return {false,{},common_failure("StalePoliticalSuccessionAccessionQuote").error};
 auto fresh=preview_political_succession_accession(ctx,actor,command);if(!fresh.ok)return {false,{},fresh.error};const auto& basis=fresh.value.basis;auto next=state;auto offices=organizations;
 auto id=common.next_id("political_succession_accession");if(!id.ok)return id;auto head=common.append_fact({id.value,"political_succession_acceded",political_succession_domain});if(!head.ok)return {false,{},head.error};
 auto change=common.next_id("organization");if(!change.ok)return change;head=common.append_fact({change.value,"organization_office_changed","organization"});if(!head.ok)return {false,{},head.error};
 auto* office=offices.offices.find(basis.office_id);auto* org=offices.organizations.find(basis.organization_id);if(!office||!org||office->holder_actor_id||office->status!=OfficeStatus::vacant||office->epoch!=basis.vacant_office_epoch)return {false,{},common_failure("PoliticalSuccessionOfficeVersionChanged").error};
 office->holder_actor_id=actor;office->status=OfficeStatus::active;++office->epoch;org->status=OrganizationStatus::active;
 OrganizationFact changed;changed.id=change.value;changed.kind="organization_office_changed";changed.organization_id=basis.organization_id;changed.office_id=basis.office_id;changed.actor_id=actor;changed.tick=common.tick();changed.cause_fact_ids={id.value};if(!offices.facts.insert(change.value,changed))return {false,{},common_failure("DuplicatePoliticalSuccessionOfficeChange").error};
 PoliticalSuccessionAccession record;record.id=id.value;record.acceptance_fact_id=basis.acceptance_fact_id;record.actor_id=actor;record.death_fact_id=basis.death_fact_id;record.notice_binding_fact_id=basis.notice_binding_fact_id;record.office_change_fact_id=change.value;record.tick=common.tick();record.installed_office_epoch=office->epoch;if(!next.accessions.insert(id.value,record))return {false,{},common_failure("DuplicatePoliticalSuccessionAccession").error};
 organizations=std::move(offices);state=std::move(next);return id;
}
}
