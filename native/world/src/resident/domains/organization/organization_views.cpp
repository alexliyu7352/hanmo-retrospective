#include "institutional_obligation_source.hpp"
#include "office_authority.hpp"
// 组织只读适配器与有界成员视图；私有欠薪明细仅供中央授权后的观察者使用。
#include "organization_internal.hpp"
#include "organization_member_read.hpp"
#include "named_membership_rules.hpp"
namespace hanmo::resident {
namespace {
// 以同一组织事实顺序锁定响应所属episode；同tick离职再入职也不改变旧报告参与者。
const OrgMemberState* response_episode(const OrganizationState& state,const OrganizationFact& fact){const auto& order=state.facts.order();const auto observed=std::find(order.begin(),order.end(),fact.id);for(const auto& row:state.members.rows()){const auto& member=row.second;if(!member.episode_source||member.actor_id!=fact.actor_id||member.organization_id!=fact.organization_id)continue;const auto& source=*member.episode_source;const auto joined=std::find(order.begin(),order.end(),source.joined_fact_id);if(joined==order.end()||observed<joined)continue;if(!source.departure_fact_id.empty()){const auto departed=std::find(order.begin(),order.end(),source.departure_fact_id);if(departed==order.end()||observed>departed)continue;}return &member;}return nullptr;}
}
OrganizationAccess::OrganizationAccess(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    std::shared_ptr<const OrganizationAuthorityClock> clock)
    : common_(c), state_(s), catalog_(catalog), economy_(economy),
      authority_clock_(std::move(clock)) {}
bool OrganizationAccess::organization_exists(const StableId& id) const{return state_.organizations.find(id)!=nullptr;}
bool OrganizationAccess::organization_active(const StableId& id) const{const auto* org=state_.organizations.find(id);return org&&org->status==OrganizationStatus::active;}
// 缺位时仅登记的延续义务获准执行；解散墓碑不再授权新的支出。
bool OrganizationAccess::institutional_obligation_allowed(const StableId& issuer_id,const StableId& obligation_id,const StableId& principal,const StableId& source,Integer amount,Integer tick) const{
    if(economy_.membership_wage_bindings.find(issuer_id))return membership_wage_authorized(common_,state_,catalog_,economy_,issuer_id,obligation_id,principal,source,amount,tick);
    if(!institutional_issuer_obligation_accrual_allowed(common_,state_,catalog_,economy_,issuer_id,obligation_id,principal,source,amount,tick))return false;
    if(const auto* wallet=common_.wallet(source))return wallet->owner.kind==Owner::Kind::organization&&wallet->owner.id==principal;
    if(const auto* inventory=common_.inventory(source))return inventory->owner.kind==Owner::Kind::organization&&inventory->owner.id==principal;return false;
}
bool OrganizationAccess::actor_accepts_duty(const StableId& actor,const StableId& organization) const{if(!organization_active(organization)||!organization_detail::eligible_officer(common_,actor))return false;const auto* member=active_membership_episode(state_,actor,organization);return member&&(member->response==MemberResponse::serving||member->response==MemberResponse::payment_requested);}
std::optional<Integer> OrganizationAccess::authority_epoch(const StableId& id) const{const auto* office=state_.offices.find(id);if(!office)return {};return office->epoch;}
Outcome<AuthorityProof>
OrganizationAccess::authorize(const ActingAuthority &acting,
                              const StableId &kind, const ScopeRef &scope,
                              Integer amount) const {
  return check_authority(common_, state_, catalog_, acting, kind, scope, amount,
                         authority_clock_.get());
}
// 先选唯一有效episode；无在职者时才返回最新历史离职供旧义务停止未来计提。
std::optional<OrganizationSelectedMemberRead> read_current_organization_member(
    const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,
    const LocalEconomyState& economy,const StableId& actor,const StableId& organization){
    const auto* member=active_membership_episode(state,actor,organization);
    if(!member)for(const auto& row:state.members.rows())
        if(row.second.actor_id==actor&&row.second.organization_id==organization)member=&row.second;
    if(!member)return {};
    auto owed=organization_detail::debt(common,state,economy,*member);
    if(!owed.ok)return OrganizationSelectedMemberRead{member,{false,{},std::move(owed.error)}};
    OrganizationMemberView out;
    out.organization_id=organization;out.actor_id=actor;
    out.response=organization_detail::response_name(member->response);
    out.unpaid_amount=owed.value.amount;out.overdue_periods=owed.value.periods;
    out.accepts_new_duty=OrganizationAccess(common,state,catalog,economy).actor_accepts_duty(actor,organization);
    return OrganizationSelectedMemberRead{member,{true,std::move(out),{}}};
}
// 旧接口仍把真实无成员与欠薪读取失败都投影为空；受检调用者使用上面的原读取。
std::optional<OrganizationMemberView> OrganizationAccess::member_view(const StableId& actor,const StableId& organization) const{
    auto selected=read_current_organization_member(common_,state_,catalog_,economy_,actor,organization);
    if(!selected||!selected->view.ok)return {};
    return std::move(selected->view.value);
}
// 固定最多 64 条，游标越界拒绝；调用者负责观察者到组织的授权范围。
Outcome<std::vector<OrganizationMemberLedgerView>> organization_member_views(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog&,const LocalEconomyState& economy,const StableId& organization,Integer cursor){if(!s.organizations.find(organization))return {false,{},common_failure("UnknownOrganization").error};std::vector<const OrgMemberState*> members;for(const auto& row:s.members.rows())if(row.second.organization_id==organization)members.push_back(&row.second);if(cursor<0||cursor>Integer(members.size()))return {false,{},common_failure("InvalidOrganizationCursor").error};std::vector<OrganizationMemberLedgerView> out;for(Integer i=cursor;i<Integer(members.size())&&i<cursor+64;++i){const auto& member=*members[static_cast<std::size_t>(i)];auto owed=organization_detail::debt(c,s,economy,member);if(!owed.ok)return {false,{},owed.error};OrganizationMemberLedgerView view;view.actor_id=member.actor_id;view.organization_id=member.organization_id;view.last_fact_id=member.last_fact_id;view.response=member.response;view.accepts_new_duties=(member.response==MemberResponse::serving||member.response==MemberResponse::payment_requested)&&organization_detail::eligible_officer(c,member.actor_id);view.has_personal_commitment=organization_detail::personally_committed(c,s,nullptr,member.actor_id);view.outstanding_amount=owed.value.amount;out.push_back(view);}return {true,std::move(out),{}};}
// 消息投影只传事实所列参与者与结果，不带组织钱包真值。
std::optional<DynamicFactView> organization_dynamic_fact(const OrganizationState& s,const StableId& id){const auto* fact=s.facts.find(id);if(!fact)return {};DynamicFactView out;out.id=id;out.kind=fact->kind;out.rule_version="organization_v1";out.tick=fact->tick;out.cause_fact_ids=fact->cause_fact_ids;out.subject_id=fact->actor_id.empty()?fact->organization_id:fact->actor_id;if(fact->kind=="organization_member_response")for(const auto& duty:s.duties.rows())if(duty.second.actor_id==fact->actor_id&&duty.second.organization_id==fact->organization_id&&!duty.second.role_id.empty())out.publicly_visible=true;out.outcome=fact->kind=="organization_member_response"?organization_detail::response_name(fact->response):(fact->code.empty()?fact->kind:fact->code);out.participants.push_back({{SubjectKind::organization,fact->organization_id},"principal"});if(!fact->actor_id.empty())out.participants.push_back({{SubjectKind::actor,fact->actor_id},"member"});if(fact->kind=="organization_member_response")if(const auto* member=response_episode(s,*fact)){const auto* offer=s.membership_offers.find(member->episode_source->offer_id);if(!offer)return {};out.rule_version="named_membership_v1";out.publicly_visible=fact->response==MemberResponse::serving||fact->response==MemberResponse::departed;if(offer->issuer_actor_id!=fact->actor_id)out.participants.push_back({{SubjectKind::actor,offer->issuer_actor_id},"issuer"});}return out;}
} // loyalty 字段保持中性零值，不把私人信任偷换成组织忠诚。
