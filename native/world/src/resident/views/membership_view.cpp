#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
// 邀请条款与本人选择沿原场所和对白显示，所有按钮冻结同一主体、来源及版本。
#include "membership_view.hpp"
#include "dynamic_world_view.hpp"
#include "common_presentation.hpp"
#include "resident/domains/director/director_model.hpp"
#include "resident/domains/director/director_membership.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;using D=V::Dict;using A=V::Array;
// 原实际位置决定能否在眼前显示；远地已知记录不冒充当面接洽人。
bool here(const CommonStore& common,const StableId& actor,const StableId& place) {
    auto location=common.resolved_location(actor);if(!location.ok)return false;
    const auto* local=std::get_if<SettlementLocation>(&location.value);return local&&local->place_id==place;
}
// 公共条款不带钱包余额；工资和合同酬金的受款人明确不同。
StableId terms_text(const MembershipTerms& terms,const ObligationRate& rate) {
    return "承接组织交付的采购差事。每"+std::to_string(terms.wage_period_ticks)+"半日计任职工资"+std::to_string(rate.numerator)+"/"+std::to_string(rate.denominator)+"钱，按整数支付并保留余数；首期从接受后算起。合同酬金归组织，任职工资归本人。旧事先办，欠薪可请求结清并合法离开，旧欠款仍归原雇主。合法结束时，仅已计提有效服务期的正余数按本约新增终态规则一次补为1钱应付，记录原余数与补偿差额；没有有效账期或余数为零不补。";
}
// authority值来自当前真实任职，不接受界面保存的证明。
V authority_value(const ActingAuthority& value) {
    return boundary::obj({{"organization_id",V{value.organization_id}},{"office_id",V{value.office_id}},{"authority_epoch",V{value.authority_epoch}},{"actor_id",V{value.actor_id}}});
}
// 可点击与失败理由都来自同一正式预览，重绘不得重写原按钮参数。
V action_value(const StableId& text,const StableId& kind,V params,const Outcome<MembershipOfferQuote>& quote) {
    return boundary::obj({{"text",V{text}},{"command",V{kind}},{"params",std::move(params)},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});
}
// 终态显示必须使用本人确实已知的决定事实，旧邀请不能提供全知结果。
StableId invitation_text(const OrganizationState& state,const MembershipOffer& offer,const StableId& known_fact) {
    if(known_fact==offer.source_fact_id)return "已当面提出邀请，等本人作答。";
    if(const auto* decision=state.membership_decisions.find(known_fact)){
        if(decision->kind==MembershipDecisionKind::accept)return "本人已接受这份条款，真实任职已开始。";
        if(decision->kind==MembershipDecisionKind::decline)return "本人拒绝了这份邀请，原来的日子和责任照旧。";
        return decision->reason=="ExistingCommitment"?"本人先办已经接下的事，暂缓这份邀请。":"本人暂缓答复，期限仍按原约定。";
    }
    if(const auto* fact=state.facts.find(known_fact)){
        if(fact->kind=="membership_withdrawn")return "当前有权代表已撤回这份未决邀请。";
        if(fact->kind=="membership_expired")return "邀请期限已过，不能再用旧条款答复。";
    }return "这份邀请已有新的情况。";
}
// 已知邀请状态来自同一不可变事实；未获知终态时仍显示未决。
StableId known_invitation_status(const OrganizationState& state,const MembershipOffer& offer,const StableId& known_fact) {
    if(known_fact==offer.source_fact_id)return "pending";
    if(const auto* decision=state.membership_decisions.find(known_fact))return decision->kind==MembershipDecisionKind::accept?"accepted":decision->kind==MembershipDecisionKind::decline?"declined":"deferred";
    const auto* source=state.facts.find(known_fact);return source&&source->kind=="membership_withdrawn"?"withdrawn":"expired";
}
// 本人回应只翻译已经获知的组织事实，不借当前私账猜测离职。
StableId member_response_name(MemberResponse response) {
    switch(response){case MemberResponse::serving:return "serving";case MemberResponse::payment_requested:return "payment_requested";case MemberResponse::refusing_new_duties:return "refusing_new_duties";case MemberResponse::awaiting_release:return "awaiting_release";case MemberResponse::departed:return "departed";}return {};
}
// 对方名称由共同人物表现读取；不是从邀请ID拼出虚构姓名。
Outcome<StableId> name(const CommonPresentation& presentation,const StableId& id) { return presentation.display_name(id); }
}
Outcome<BoundaryValue> query_named_membership_entries(const WorldRead& world,const PreparedCatalog& catalog,const ActivityRead& activity,const MembershipReleaseRead& release,const StableId& observer,const StableId& place,const StableId& provider,const std::function<bool(const OrganizationContractCommand&)>& can_accept_organization,const EconomicSupplyPolicyRead* issued_supply) {
    const auto* common=dynamic_cast<const CommonStore*>(&world.common);
    if(!common||!catalog.named_membership||!catalog.organization||!catalog.local_economy||!catalog.common||!world.organization||!world.local_economy||!world.social.dynamic)return {false,{},common_failure("NamedMembershipUnavailable").error};
    if(!common->actor(observer)||!here(*common,observer,place))return {false,{},common_failure("PlaceNotObservable").error};
    if(!provider.empty()){
        bool matching=false;for(const auto& session:world.interaction.interactions)if(session.actor_id==observer&&session.context.provider_actor_id==provider&&session.context.place_id==place)matching=true;
        if(!matching||!here(*common,provider,place))return {false,{},common_failure("MembershipDialogueUnavailable").error};
    }
    const auto& state=*world.organization;const auto& org_catalog=*catalog.organization;const auto& economy_catalog=*catalog.local_economy;
    OrganizationAccess organization(*common,state,org_catalog,*world.local_economy);CommonPresentation presentation(common->state(),*catalog.common);A rows;
    // 先展示本人获知的邀请，入职后也保留明确答复而不是悄悄消失。
    for(auto it=state.membership_offers.rows().rbegin();it!=state.membership_offers.rows().rend();++it){
        const auto& offer=it->second;if(observer!=offer.issuer_actor_id&&observer!=offer.target_actor_id)continue;
        const auto other=observer==offer.target_actor_id?offer.issuer_actor_id:offer.target_actor_id;
        if((!provider.empty()&&provider!=other)||!here(*common,other,place)||!membership_fact_known(*common,world.social,observer,offer.source_fact_id))continue;
        const auto* terms=org_catalog.membership_terms.find(offer.terms_id);const auto* rate=terms?economy_catalog.rates.find(terms->wage_rate_profile_id):nullptr;if(!terms||!rate)return {false,{},common_failure("InvalidMembershipTerms",offer.id).error};
        auto person=name(presentation,other);if(!person.ok)return {false,{},person.error};
        StableId source=offer.source_fact_id;if(!offer.decision_fact_id.empty()&&membership_fact_known(*common,world.social,observer,offer.decision_fact_id))source=offer.decision_fact_id;
        A actions;if(offer.status==MembershipOfferStatus::pending&&observer==offer.target_actor_id){
            for(const auto& answer:std::vector<std::pair<MembershipDecisionKind,StableId>>{{MembershipDecisionKind::accept,"接受这份任职"},{MembershipDecisionKind::decline,"婉拒邀请"},{MembershipDecisionKind::defer,"先办旧事，暂缓答复"}}){
                OrganizationAnswerMembershipCommand command{observer,offer.id,offer.revision,answer.first,offer.source_fact_id};
                auto quote=preview_organization_answer_membership(*common,state,org_catalog,*world.local_economy,economy_catalog,world.social,world.interaction,activity,release,observer,command);
                if(!quote.ok&&quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};
                const auto value=answer.first==MembershipDecisionKind::accept?"accept":answer.first==MembershipDecisionKind::decline?"decline":"defer";
                actions.push_back(action_value(answer.second,"organization_answer_membership",boundary::obj({{"actor_id",V{observer}},{"offer_id",V{offer.id}},{"expected_offer_revision",V{offer.revision}},{"answer",V{value}},{"source_fact_id",V{offer.source_fact_id}}}),quote));
            }
        }else if(offer.status==MembershipOfferStatus::pending&&observer==offer.issuer_actor_id){
            const auto* office=state.offices.find(offer.office_id);
            if(office){OrganizationWithdrawMembershipCommand command{{offer.organization_id,office->id,office->epoch,observer},offer.id,offer.revision};auto quote=preview_organization_withdraw_membership(*common,state,org_catalog,economy_catalog,observer,command);actions.push_back(action_value("撤回尚未决定的邀请","organization_withdraw_membership",boundary::obj({{"authority",authority_value(command.authority)},{"offer_id",V{offer.id}},{"expected_offer_revision",V{offer.revision}}}),quote));}
        }
        rows.push_back(boundary::obj({{"kind",V{"membership_invitation"}},{"id",V{offer.id}},{"actor_id",V{other}},{"display_name",V{person.value}},{"organization_id",V{offer.organization_id}},{"terms_id",V{offer.terms_id}},{"source_fact_id",V{source}},{"status",V{known_invitation_status(state,offer,source)}},{"offer_revision",V{offer.revision}},{"expires_tick",V{offer.expires_tick}},{"text",V{invitation_text(state,offer,source)}},{"terms_text",V{terms_text(*terms,*rate)}},{"actions",V{std::move(actions)}}}));
    }
    // 原邀请双方只能显示已收到的任职结果；私人工资数额不跟随公共在岗状态扩散。
    for(const auto& row:state.members.rows()){
        const auto& member=row.second;if(!member.episode_source)continue;
        const auto* offer=state.membership_offers.find(member.episode_source->offer_id);if(!offer||(observer!=member.actor_id&&observer!=offer->issuer_actor_id))continue;
        if((!provider.empty()&&provider!=member.actor_id&&provider!=offer->issuer_actor_id)||!here(*common,member.actor_id,place))continue;
        if(member.last_fact_id.empty()||!membership_fact_known(*common,world.social,observer,member.last_fact_id))continue;
        const auto* source=state.facts.find(member.last_fact_id);if(!source)continue;auto person=name(presentation,member.actor_id);if(!person.ok)return {false,{},person.error};
        const auto response=member_response_name(source->response);
        const StableId text=response=="awaiting_release"?"本人已申请离开，已经承接的差事或受托物尚未清完，仍照旧履行。":response=="departed"?"已经合法结束这次任职；原雇主仍须清偿旧欠款，本人继续原来的生活。":response=="refusing_new_duties"?"本人暂不接新职责，旧差事仍继续。":response=="payment_requested"?"本人已请求结清工资。":"已按本人接受的条款任职，可依法委派真实采购差事。";
        A actions;
        if(observer==member.actor_id&&member.response!=MemberResponse::departed&&member.response!=MemberResponse::awaiting_release){
            OrganizationMemberCommand command{observer,member.organization_id,member.last_fact_id,MemberCommandKind::lawful_leave};
            auto preview=preview_named_membership_member_command(*common,state,org_catalog,*world.local_economy,release,command,common->tick());
            if(!preview.ok&&preview.error.kind!=ErrorKind::rule)return {false,{},preview.error};
            auto params=boundary::obj({{"actor_id",V{observer}},{"organization_id",V{member.organization_id}},{"source_fact_id",V{member.last_fact_id}},{"kind",V{"lawful_leave"}}});
            actions.push_back(boundary::obj({{"text",V{"申请合法离开，已有差事照办"}},{"command",V{"organization_member_response"}},{"params",std::move(params)},{"available",V{preview.ok}},{"code",V{preview.ok?StableId{}:preview.error.original.code}}}));
        }
        rows.push_back(boundary::obj({{"kind",V{"membership_episode"}},{"id",V{member.id}},{"actor_id",V{member.actor_id}},{"display_name",V{person.value}},{"source_fact_id",V{member.last_fact_id}},{"status",V{response}},{"offer_id",V{offer->id}},{"text",V{text}},{"terms_text",V{""}},{"actions",V{std::move(actions)}}}));
    }
    // 当前合法代表只看到当场且已认识的真人；每条动作经完整招揽预览，不产全城名单。
    for(const auto& office_row:state.offices.rows()){
        const auto& office=office_row.second;if(office.holder_actor_id!=std::optional<StableId>(observer)||office.status!=OfficeStatus::active)continue;
        for(const auto& term_row:org_catalog.membership_terms.rows()){
            const auto& terms=term_row.second;if(terms.organization_id!=office.organization_id||terms.site_id!=place)continue;const auto* rate=economy_catalog.rates.find(terms.wage_rate_profile_id);if(!rate)continue;
            for(const auto& target:common->actor_order()){
                if(target==observer||(!provider.empty()&&provider!=target)||!here(*common,target,place))continue;
                OrganizationOfferMembershipCommand command;command.authority={office.organization_id,office.id,office.epoch,observer};command.target_actor_id=target;command.terms_id=terms.id;command.context.place_id=place;command.context.provider_actor_id=target;
                auto quote=preview_organization_offer_membership(*common,state,org_catalog,economy_catalog,world.social,world.interaction,activity,observer,command);
                if(!quote.ok){if(quote.error.kind!=ErrorKind::rule)return {false,{},quote.error};continue;}
                auto person=name(presentation,target);if(!person.ok)return {false,{},person.error};
                auto params=boundary::obj({{"authority",authority_value(command.authority)},{"target_actor_id",V{target}},{"terms_id",V{terms.id}},{"context",boundary::obj({{"place_id",V{place}},{"provider_actor_id",V{target}},{"role_id",V{""}}})}});
                A actions{action_value("向"+person.value+"说明任职条款","organization_offer_membership",std::move(params),quote)};
                rows.push_back(boundary::obj({{"kind",V{"membership_contact"}},{"id",V{office.id+":"+terms.id+":"+target}},{"actor_id",V{target}},{"display_name",V{person.value}},{"organization_id",V{terms.organization_id}},{"terms_id",V{terms.id}},{"source_fact_id",V{""}},{"text",V{"可当面提出一份具名邀请，由对方本人决定。"}},{"terms_text",V{terms_text(terms,*rate)}},{"actions",V{std::move(actions)}}}));
            }
        }
    }
    // 派工仍借原有限Task投影和完整报价，执行者必须是实际接受后建立的成员。
    if(can_accept_organization){
        auto dynamic=query_dynamic_place(world,catalog,organization,observer,place,0,can_accept_organization,issued_supply);if(!dynamic.ok)return dynamic;
        const auto* values=dynamic.value.find("rows");if(values)for(const auto& row:boundary::array(*values,"membership/assignments")){
            const auto kind=boundary::get_string(row,"kind","membership");
            if(kind=="contract"){
                const auto target=boundary::get_string(row,"actor_id","membership");const auto* policy=catalog.director?catalog.director->subjects.find(target):nullptr;
                if(!policy||!policy->named_membership_policy_id||(!provider.empty()&&target!=provider)||!here(*common,target,place))continue;
                auto person=name(presentation,target);if(!person.ok)return {false,{},person.error};const auto status=boundary::get_string(row,"status","membership");
                D shown{{"kind",V{"membership_task"}},{"id",V{boundary::get_string(row,"task_id","membership")}},{"actor_id",V{target}},{"display_name",V{person.value}},{"source_fact_id",V{""}},{"status",V{status}},{"text",V{"本人已接下的原采购差事："+boundary::get_string(row,"title","membership")+"。差事与受托货物不会因邀请、拒绝或离开申请而消失。"}},{"terms_text",V{""}},{"actions",V{A{}}}};
                for(const auto& field:std::vector<StableId>{"task_id","contract_id","offer_id","deadline_tick","agreed_quantity","delivered_quantity"})if(const auto* value=row.find(field))shown.emplace_back(field,*value);
                rows.push_back(V{std::move(shown)});continue;
            }
            if(kind!="organization_opportunity")continue;
            const auto target=boundary::get_string(row,"executor_actor_id","membership"),org=boundary::get_string(row,"organization_id","membership");const auto* member=active_membership_episode(state,target,org);
            if(!member||!member->episode_source||(!provider.empty()&&target!=provider)||!here(*common,target,place))continue;
            const auto* offer=state.membership_offers.find(member->episode_source->offer_id);if(!offer||offer->issuer_actor_id!=observer||!membership_fact_known(*common,world.social,observer,offer->decision_fact_id))continue;
            auto person=name(presentation,target);if(!person.ok)return {false,{},person.error};const auto* action=row.find("action");if(!action)continue;
            D button=boundary::object(*action,"membership/action");button.emplace_back("text",V{"委派"+person.value+"办理这份采购"});button.emplace_back("available",V{true});button.emplace_back("code",V{""});
            rows.push_back(boundary::obj({{"kind",V{"membership_assignment"}},{"id",V{boundary::get_string(row,"id","membership")}},{"actor_id",V{target}},{"display_name",V{person.value}},{"source_fact_id",V{offer->decision_fact_id}},{"text",V{"对方已任合同执行者；这份差事将沿原货单实际采购、行旅与交付，酬金付给组织。"}},{"terms_text",V{""}},{"actions",V{A{V{std::move(button)}}}}}));
        }
    }
    // 私人欠薪只向本人显示已知来源；离开仍交给唯一成员响应命令复验。
    const auto known=known_subject_view(*common,world.social,organization,observer,&world.interaction);
    for(const auto& entry:state.members.rows())if(auto paid=known_membership_paid_source(state,world.social,known,entry.second)){
        const auto& member=entry.second;auto params=boundary::obj({{"actor_id",V{observer}},{"organization_id",V{member.organization_id}},{"source_fact_id",V{paid->fact_id}},{"kind",V{"wait_payment"}}});
        A actions{boundary::obj({{"text",V{"核对已收工钱，恢复接新差事"}},{"command",V{"organization_member_response"}},{"params",std::move(params)},{"available",V{true}},{"code",V{""}}})};
        rows.push_back(boundary::obj({{"kind",V{"membership_paid"}},{"id",V{member.id+":"+paid->fact_id}},{"actor_id",V{observer}},{"display_name",V{"本人任职"}},{"source_fact_id",V{paid->fact_id}},{"text",V{"已收到原雇主付款回执；可核实欠期是否全部结清后恢复接新差事。已经提出的离开申请仍按原意愿办理。"}},{"terms_text",V{""}},{"actions",V{std::move(actions)}}}));
    }
    for(const auto& arrear:known.arrears){
        if(arrear.outstanding<=0)continue;const auto* member=active_membership_episode(state,observer,arrear.organization_id);if(!member)continue;
        const auto* report=world.social.dynamic->reports.find(arrear.report_id);bool member_source=false;
        if(report&&report->observation.obligation)for(const auto& duty_id:member->duty_ids)if(const auto* duty=state.duties.find(duty_id))if(std::find(duty->obligation_ids.begin(),duty->obligation_ids.end(),report->observation.obligation->obligation_id)!=duty->obligation_ids.end())member_source=true;
        if(!member_source)continue;
        A actions;for(const auto& choice:std::vector<std::pair<StableId,StableId>>{{"request_payment","请结清任职工资"},{"decline_new_work","暂不接新差事"},{"lawful_leave","申请合法离开，旧责照办"}}){
            const auto params=boundary::obj({{"actor_id",V{observer}},{"organization_id",V{member->organization_id}},{"source_fact_id",V{arrear.fact_id}},{"kind",V{choice.first}}});
            actions.push_back(boundary::obj({{"text",V{choice.second}},{"command",V{"organization_member_response"}},{"params",params},{"available",V{true}},{"code",V{""}}}));
        }
        rows.push_back(boundary::obj({{"kind",V{"membership_arrears"}},{"id",V{arrear.arrear_id}},{"actor_id",V{observer}},{"display_name",V{"本人任职"}},{"source_fact_id",V{arrear.fact_id}},{"text",V{"据已收到的半日"+std::to_string(arrear.observation_tick)+"工资回执，仍欠"+std::to_string(arrear.outstanding)+"钱；申请离开不会免除原雇主旧债，也不会清掉受托货物。"}},{"terms_text",V{""}},{"actions",V{std::move(actions)}}}));
    }
    const bool truncated=rows.size()>64;if(truncated)rows.resize(64);
    return {true,boundary::obj({{"ok",V{true}},{"actor_id",V{observer}},{"place_id",V{place}},{"provider_actor_id",V{provider}},{"tick",V{common->tick()}},{"revision",V{common->revision()}},{"rows",V{std::move(rows)}},{"truncated",V{truncated}}}),{}};
}
}
