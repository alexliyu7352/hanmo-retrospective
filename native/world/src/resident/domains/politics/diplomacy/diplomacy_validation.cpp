// 冷验证沿真实历史公职与原条款来源重建提议/亲递，不要求今日持有人仍是昨日代表。
#include "diplomacy_offers.hpp"
#include "diplomacy_delegation.hpp"
#include "diplomacy_reports.hpp"
#include "resident/domains/tasks/envoy_task_contract.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/interaction/face_to_face_contact.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
// 原目录Grant是历史范围，今天的在场或钱包状态不改写过去签发资格。
bool historic_grant(const OrganizationCatalog& catalog,const StableId& id,const ActingAuthority& authority,const StableId& kind,const StableId& place,Integer tick){const auto* grant=catalog.grants.find(id);const auto* office=catalog.offices.find(authority.office_id);if(!grant||!office||grant->office_id!=authority.office_id||std::find(office->grant_ids.begin(),office->grant_ids.end(),id)==office->grant_ids.end()||grant->amount_limit<0||tick<grant->valid_from||(grant->valid_until&&tick>*grant->valid_until)||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),kind)==grant->allowed_command_kinds.end())return false;return std::any_of(grant->scopes.begin(),grant->scopes.end(),[&](const auto& s){return s.kind==ScopeRef::Kind::site&&s.id==place;});}
// Common只存唯一头，政治载荷必须与同一个实际ID及kind对应。
bool header(const CommonStore& c,const StableId& id,const StableId& kind){const auto* h=c.state().facts.find(id);return h&&h->id==id&&h->kind==kind&&h->domain=="politics";}
}
// 当前第一片只有提议和真实亲递；未实现的回应状态不能通过伪造enum提前启用。
Status validate_diplomacy_state(const CommonStore& c,const OrganizationState& organizations,const OrganizationCatalog& oc,const DiplomacyCatalog& catalog,const DiplomacyState& state,const TaskEnvoyRead* envoy_tasks){
    if(state.version!=catalog.version||(state.version!=1&&state.version!=2)||(state.version==2)!=state.v2.has_value()||(catalog.version==2)!=catalog.v2.has_value())return common_failure("DiplomacyVersionMismatch");
    if(!catalog.enabled)return state.offers.empty()&&state.facts.empty()&&state.presentations.empty()&&state.replies.empty()&&state.missions.empty()?common_success():common_failure("UnexpectedDiplomacyState");
    if(state.v2) {
        const DiplomaticHistoryRead history{c,organizations,oc,catalog,state};
        auto known=validate_diplomatic_mission_knowledge(history,*catalog.v2,*state.v2);if(!known.ok)return known;
        auto reports=validate_diplomatic_report_state(history,*state.v2);if(!reports.ok)return reports;
        for(const auto& row:state.v2->reports.rows()) {
            if(!envoy_tasks)return common_failure("MissingEnvoyTaskServices",row.first);
            auto task=envoy_tasks->envoy_task_source(row.second.source.task_id);if(!task.ok)return {false,{},task.error};
            const auto& final=task.value.completion;
            if(!final||final->result!=EnvoyTaskResult::completed||!final->diplomatic_report_fact_id||*final->diplomatic_report_fact_id!=row.first||
                !final->report_action_id||*final->report_action_id!=row.second.source.report_action_id||final->completed_tick!=row.second.source.completed_tick)
                return common_failure("DiplomaticReportTaskNotFinal",row.first);
        }
    }
    std::set<StableId> covered;
    for(const auto& row:state.offers.rows()){
        const auto& o=row.second;const auto* p=catalog.mission_policies.find(o.policy_id);const auto* terms=catalog.passage_terms.find(o.terms_id);const auto* grant=oc.grants.find(o.issuer_grant_id);const auto sequence=organization_instance_sequence(o.id,"diplomatic_offer");const auto* f=state.facts.find(o.id);
        if(row.first!=o.id||!p||!terms||!grant||!sequence||o.terms_revision!=1||(state.version==1&&o.envoy_actor_id!=o.issuer.actor_id)||o.issuer.organization_id!=p->issuer_organization_id||terms->beneficiary_organization_id!=p->issuer_organization_id||terms->grantor_organization_id!=p->target_organization_id||std::find(p->passage_terms_ids.begin(),p->passage_terms_ids.end(),o.terms_id)==p->passage_terms_ids.end()||o.issued_tick<0||o.issued_tick>c.tick()||p->offer_valid_ticks<=0||o.issued_tick>INT64_MAX-p->offer_valid_ticks||!historic_grant(oc,o.issuer_grant_id,o.issuer,"diplomatic_propose_passage",p->issuer_site_id,o.issued_tick)||!header(c,o.id,"diplomatic_offer_proposed")||!f||f->id!=o.id||f->offer_id!=o.id||f->kind!="diplomatic_offer_proposed"||f->actor_id!=o.issuer.actor_id||f->terms_id!=o.terms_id||f->tick!=o.issued_tick||f->terms_revision!=1||f->offer_revision!=1||!f->cause_fact_ids.empty()||f->witness_actor_ids!=(o.envoy_actor_id==o.issuer.actor_id?std::vector<StableId>{o.issuer.actor_id}:std::vector<StableId>{o.issuer.actor_id,o.envoy_actor_id}))return common_failure("InvalidDiplomaticOfferHistory",row.first);
        auto signer=validate_historical_office_signer(c,organizations,oc,o.issuer,o.issued_tick,*sequence);if(!signer.ok)return signer;
        Integer deadline=o.issued_tick+p->offer_valid_ticks;if(grant->valid_until&&*grant->valid_until<INT64_MAX)deadline=std::min(deadline,*grant->valid_until+1);if(o.expires_tick!=deadline)return common_failure("DiplomaticDeadlineHistoryMismatch",o.id);covered.insert(o.id);if(o.envoy_actor_id!=o.issuer.actor_id&&o.mission_authority_fact_id.empty())return common_failure("MissingDiplomaticMission",o.id);if(!o.mission_authority_fact_id.empty()){const auto* mission=state.missions.find(o.mission_authority_fact_id);if(!mission||mission->proposal_id!=o.id)return common_failure("MissingDiplomaticMission",o.id);}
        if(o.status==DiplomaticOfferStatus::awaiting_delivery){if(o.revision!=1||o.last_fact_id!=o.id||!o.presentation_fact_id.empty())return common_failure("DiplomaticOfferHistoryMismatch",o.id);continue;}
        if(o.status!=DiplomaticOfferStatus::presented&&o.status!=DiplomaticOfferStatus::accepted_pending_ratification&&o.status!=DiplomaticOfferStatus::refused)return common_failure("UnsupportedDiplomaticOfferHistory",o.id);
        if(o.status==DiplomaticOfferStatus::presented&&(o.revision!=2||o.last_fact_id!=o.presentation_fact_id))return common_failure("DiplomaticOfferHistoryMismatch",o.id);
        const auto* delivery=state.presentations.find(o.presentation_fact_id);const auto* event=state.facts.find(o.presentation_fact_id);if(!delivery||!event)return common_failure("MissingDiplomaticPresentation",o.id);
        const auto& d=*delivery;const auto number=organization_instance_sequence(d.id,"diplomatic_presentation");
        std::vector<StableId> presentation_causes{o.id};
        if(d.task_binding){
            if(!envoy_tasks)return common_failure("MissingEnvoyTaskServices",d.id);const auto& b=*d.task_binding;auto source=envoy_tasks->envoy_task_source(b.task_id);if(!source.ok)return {false,{},source.error};const auto& a=source.value.acceptance;const auto& done=source.value.delivery;
            if(b.mission_authority_fact_id!=o.mission_authority_fact_id||b.mission_authority_fact_id!=a.mission_authority_fact_id||b.task_id!=a.task_id||b.contract_id!=a.contract_id||b.acceptance_fact_id!=a.fact_id||a.proposal_id!=o.id||a.proposal_fact_id!=o.id||a.terms_revision!=d.terms_revision||a.envoy_actor_id!=d.envoy_actor_id||!done||done->diplomatic_presentation_fact_id!=d.id||done->action_id!=b.action_id||done->task_id!=b.task_id||done->contract_id!=b.contract_id||done->acceptance_fact_id!=b.acceptance_fact_id||done->completed_tick!=d.tick||!d.session_id.empty())return common_failure("InvalidDiplomaticTaskPresentation",d.id);
            presentation_causes={o.id,b.mission_authority_fact_id,b.acceptance_fact_id};
        }
        if(!number||*number<=*sequence||!header(c,d.id,"diplomatic_offer_presented")||d.id!=o.presentation_fact_id||d.offer_id!=o.id||d.proposal_fact_id!=o.id||d.terms_id!=o.terms_id||d.terms_revision!=1||d.offer_revision!=2||d.envoy_actor_id!=o.envoy_actor_id||d.representative.organization_id!=p->target_organization_id||d.representative.office_id!=p->target_office_id||d.representative.actor_id==d.envoy_actor_id||d.tick<o.issued_tick||d.tick>=o.expires_tick||d.tick>c.tick()||d.place_id!=p->target_site_id||(!d.task_binding&&d.session_id.empty())||!historic_grant(oc,d.representative_grant_id,d.representative,"diplomatic_receive_passage",d.place_id,d.tick))return common_failure("InvalidDiplomaticPresentationHistory",d.id);
        signer=validate_historical_office_signer(c,organizations,oc,d.representative,d.tick,*number);if(!signer.ok)return signer;
        InteractionContext context;context.place_id=d.place_id;context.provider_actor_id=d.provider_actor_id;context.role_id=d.role_id;context.kind=d.context_kind;
        if(!face_to_face_contact_context_valid(context)||(d.session_actor_id!=d.envoy_actor_id&&d.session_actor_id!=d.representative.actor_id)||d.provider_actor_id!=(d.session_actor_id==d.envoy_actor_id?d.representative.actor_id:d.envoy_actor_id)||event->id!=d.id||event->offer_id!=o.id||event->kind!="diplomatic_offer_presented"||event->actor_id!=d.envoy_actor_id||event->terms_id!=o.terms_id||event->tick!=d.tick||event->terms_revision!=1||event->offer_revision!=2||event->cause_fact_ids!=presentation_causes||event->witness_actor_ids!=std::vector<StableId>{d.envoy_actor_id,d.representative.actor_id})return common_failure("InvalidDiplomaticContactHistory",d.id);
        covered.insert(d.id);
        if(o.status==DiplomaticOfferStatus::presented)continue;
        const auto* reply=state.replies.find(o.last_fact_id);const auto* response=state.facts.find(o.last_fact_id);
        if(!reply||!response||o.revision!=3)return common_failure("MissingDiplomaticReply",o.id);
        const auto& r=*reply;const auto reply_number=organization_instance_sequence(r.id,"diplomatic_reply");
        const bool accepted=r.decision==DiplomaticReplyKind::accept;
        if((!accepted&&r.decision!=DiplomaticReplyKind::refuse)||o.status!=(accepted?DiplomaticOfferStatus::accepted_pending_ratification:DiplomaticOfferStatus::refused)||r.id!=o.last_fact_id||r.offer_id!=o.id||r.presentation_fact_id!=d.id||r.terms_id!=o.terms_id||r.terms_revision!=1||r.offer_revision!=3||r.authority.actor_id!=d.representative.actor_id||r.authority.organization_id!=p->target_organization_id||r.authority.office_id!=p->target_office_id||r.tick<d.tick||r.tick>=o.expires_tick||r.tick>c.tick()||!reply_number||*reply_number<=*number||!header(c,r.id,"diplomatic_offer_replied")||!historic_grant(oc,r.grant_id,r.authority,"diplomatic_reply_passage",p->target_site_id,r.tick))return common_failure("InvalidDiplomaticReplyHistory",r.id);
        signer=validate_historical_office_signer(c,organizations,oc,r.authority,r.tick,*reply_number);if(!signer.ok)return signer;
        InteractionContext reply_context;reply_context.place_id=r.place_id;reply_context.provider_actor_id=r.provider_actor_id;reply_context.role_id=r.role_id;reply_context.kind=r.context_kind;
        if(r.place_id!=p->target_site_id||r.session_id.empty()||!face_to_face_contact_context_valid(reply_context)||(r.session_actor_id!=r.authority.actor_id&&r.session_actor_id!=o.envoy_actor_id)||r.provider_actor_id!=(r.session_actor_id==r.authority.actor_id?o.envoy_actor_id:r.authority.actor_id)||response->id!=r.id||response->offer_id!=o.id||response->kind!="diplomatic_offer_replied"||response->actor_id!=r.authority.actor_id||response->terms_id!=o.terms_id||response->tick!=r.tick||response->terms_revision!=1||response->offer_revision!=3||response->cause_fact_ids!=std::vector<StableId>{d.id}||response->witness_actor_ids!=std::vector<StableId>{r.authority.actor_id,o.envoy_actor_id})return common_failure("InvalidDiplomaticReplyContact",r.id);
        covered.insert(r.id);
    }
    for(const auto& row:state.missions.rows()){
        const auto& m=row.second;const auto* o=state.offers.find(m.proposal_id);const auto* p=catalog.mission_policies.find(m.policy_id);const auto* g=oc.grants.find(m.issuer_grant_id);const auto seq=organization_instance_sequence(row.first,"diplomatic_mission");
        if(!o||!p||!p->task_terms||!g||!seq||m.mission_authority_fact_id!=row.first||o->mission_authority_fact_id!=row.first||m.proposal_fact_id!=o->id||m.policy_id!=o->policy_id||m.rule_name_key!=p->name_key||m.terms_revision!=o->terms_revision||m.envoy_actor_id!=o->envoy_actor_id||m.issuer_actor_id!=o->issuer.actor_id||m.issuer_office_id!=o->issuer.office_id||m.issuer_authority_epoch!=o->issuer.authority_epoch||m.issuer_organization_id!=p->issuer_organization_id||m.target_organization_id!=p->target_organization_id||m.target_office_id!=p->target_office_id||m.delivery_site_id!=p->target_site_id||m.report_office_id!=o->issuer.office_id||m.report_site_id!=p->issuer_site_id||m.issued_tick<o->issued_tick||m.issued_tick>c.tick()||m.issued_tick>=o->expires_tick||(m.return_report_required&&m.envoy_actor_id==m.issuer_actor_id)||!header(c,row.first,diplomatic_mission_authority_fact_kind))return common_failure("InvalidDiplomaticMissionHistory",row.first);
        const auto& t=*p->task_terms;ActingAuthority a{m.issuer_organization_id,m.issuer_office_id,m.issuer_authority_epoch,m.issuer_actor_id};
        if(!historic_grant(oc,m.issuer_grant_id,a,"diplomatic_commission_envoy",p->issuer_site_id,m.issued_tick)||t.acceptance_valid_ticks<=0||m.issued_tick>INT64_MAX-t.acceptance_valid_ticks||m.delivery_expires_tick!=o->expires_tick||m.delivery_work_ticks!=t.delivery_work_ticks||m.report_work_ticks!=t.report_work_ticks||m.return_report_required!=t.return_report_required||m.continuity_policy_id!=t.continuity_policy_id||m.delivery_expires_tick-m.issued_tick<=m.delivery_work_ticks)return common_failure("InvalidDiplomaticMissionTerms",row.first);
        Integer deadline=std::min(m.issued_tick+t.acceptance_valid_ticks,o->expires_tick);if(g->valid_until&&*g->valid_until<INT64_MAX)deadline=std::min(deadline,*g->valid_until+1);if(m.acceptance_expires_tick!=deadline)return common_failure("InvalidDiplomaticMissionDeadline",row.first);
        auto signer=validate_historical_office_signer(c,organizations,oc,a,m.issued_tick,*seq);if(!signer.ok)return signer;
    }
    if(covered.size()!=state.facts.size()||state.presentations.size()+state.offers.size()+state.replies.size()!=state.facts.size())return common_failure("OrphanDiplomaticHistory");
    for(const auto& row:c.state().facts.rows())if(row.second.domain=="politics"&&(row.second.kind=="diplomatic_offer_proposed"||row.second.kind=="diplomatic_offer_presented"||row.second.kind=="diplomatic_offer_replied")&&!covered.count(row.first))return common_failure("MissingDiplomaticFactPayload",row.first);
    for(const auto& row:c.state().facts.rows())if(row.second.domain==diplomatic_fact_domain&&row.second.kind==diplomatic_mission_authority_fact_kind&&!state.missions.find(row.first))return common_failure("MissingDiplomaticMissionPayload",row.first);
    return common_success();
}
}
