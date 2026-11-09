// 师承永久账按 Common 实际事实顺序重放；姓名相同、当前关系或后来的控制变化不构成来源。
#include "family_apprenticeship.hpp"
#include <charconv>
#include <tuple>
namespace hanmo::resident {namespace {
// 分配号只证明是原 Common 实例，不用数字大小替代真实跨域事实序。
bool allocated(const CommonStore& c,const StableId& id){const StableId prefix="apprenticeship:";if(id.rfind(prefix,0)!=0)return false;const auto digits=id.substr(prefix.size());Integer value=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&value>0&&value<c.state().next_instance_id&&std::to_string(value)==digits;}
// 每一份本人决定要从原控制链证明，不能在接受时借今日玩家代签旧提议。
Status decision_person(const CommonStore& c,const CommonCatalog& catalog,const ApprenticeshipIdentityRead& access,const StableId& actor,Integer tick,const StableId& boundary){
    if(!access.player_before)return common_failure("ApprenticeshipHistoricalPlayerRequired");auto player=access.player_before(tick,boundary);if(!player.ok)return {false,{},player.error};if(player.value!=actor)return common_failure("ApprenticeshipPersonalDecisionRequired",actor);return validate_apprenticeship_person(c,catalog,access,actor,tick,boundary);
}
// 一份关系的核心双方与技艺必须不变，不能以重访同一个节点改写旧条款。
bool same_terms(const ApprenticeshipStatementSource& a,const ApprenticeshipStatementSource& b){return std::tie(a.statement_id,a.speaker_actor_id,a.listener_actor_id,a.teacher_actor_id,a.student_actor_id,a.skill_id,a.terms_version)==std::tie(b.statement_id,b.speaker_actor_id,b.listener_actor_id,b.teacher_actor_id,b.student_actor_id,b.skill_id,b.terms_version);}
bool active_pair(const ApprenticeshipTimeline& timeline,const ApprenticeshipStatementSource& s){for(const auto& row:timeline.bonds.rows()){const auto& b=row.second;if(!b.exit_fact_id&&std::tie(b.teacher_actor_id,b.student_actor_id,b.skill_id)==std::tie(s.teacher_actor_id,s.student_actor_id,s.skill_id))return true;}return false;}
// 一轮只允许同一实际双方/技艺的一项待办；另一个已终结提议不被删除或改名重用。
bool pending_pair(const FamilyApprenticeshipState& state,const ApprenticeshipTimeline& timeline,const ApprenticeshipStatementSource& s){for(const auto& row:state.proposals.rows())if(!timeline.terminal_proposals.count(row.first)&&std::tie(row.second.source.teacher_actor_id,row.second.source.student_actor_id,row.second.source.skill_id)==std::tie(s.teacher_actor_id,s.student_actor_id,s.skill_id))return true;return false;}
// 零时本人命令不接管其他行动；历史自由身另由具名纯读按原边界证明。
Status current_actor(const CommonStore& c,const CommonCatalog& catalog,const ApprenticeshipIdentityRead& access){const auto& actor=c.state().metadata.player_actor_id;const auto* person=c.actor(actor);if(!person||!person->available||!person->action_id.empty())return common_failure("ApprenticeshipDecisionMakerBusy",actor);return validate_apprenticeship_person(c,catalog,access,actor,c.tick());}
template<class T>Outcome<T> failed(const Status& status){return {false,{},status.error};}
}
Outcome<ApprenticeshipTimeline> validate_family_apprenticeship(const CommonStore& c,const InteractionCatalog& interaction,const ApprenticeshipCatalog& catalog,const FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access){
    auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<ApprenticeshipTimeline>{return {false,{},common_failure(code,id).error};};
    if(!interaction.common)return fail("ApprenticeshipCommonCatalogRequired");auto valid=validate_apprenticeship_catalog(catalog,*interaction.common,interaction);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);valid=validate_family_apprenticeship_shape(state);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);
    const auto total=state.proposals.size()+state.replies.size()+state.exits.size();if(total>65536)return fail("ApprenticeshipFactLimit");
    ApprenticeshipTimeline out;std::set<StableId> seen,pending;std::vector<StableId> proposals,replies,exits;Integer last_tick=-1,last_revision=-1;
    for(const auto& head:c.state().facts.rows()){
        const auto* p=state.proposals.find(head.first);const auto* r=state.replies.find(head.first);const auto* e=state.exits.find(head.first);
        const auto count=Integer(p!=nullptr)+Integer(r!=nullptr)+Integer(e!=nullptr);
        if(!count&&head.second.domain!="family_apprenticeship"&&head.second.kind.rfind("family_apprenticeship_",0)!=0)continue;
        if(!catalog.enabled)return fail("ApprenticeshipProfileRequired",head.first);
        if(count!=1||head.second.id!=head.first||head.second.domain!="family_apprenticeship"||!allocated(c,head.first))return fail("ApprenticeshipFactPayloadMismatch",head.first);
        const auto tick=p?p->source.tick:r?r->tick:e->tick,revision=p?p->source.revision:r?r->revision:e->revision;
        if(tick<0||tick>c.tick()||revision<0||revision>c.state().identity.revision||tick<last_tick||revision<last_revision)return fail("ApprenticeshipFactOrderMismatch",head.first);last_tick=tick;last_revision=revision;seen.insert(head.first);
        if(p){
            proposals.push_back(head.first);
            if(p->kind!="family_apprenticeship_proposed"||head.second.kind!=p->kind)return fail("InvalidApprenticeshipProposal",head.first);
            valid=validate_apprenticeship_statement_source(c,*interaction.common,interaction,catalog,access,p->source,head.first);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);
            if(active_pair(out,p->source))return fail("ApprenticeshipAlreadyActive",head.first);
            for(const auto& previous:pending){const auto& old=state.proposals.find(previous)->source;if(std::tie(old.teacher_actor_id,old.student_actor_id,old.skill_id)==std::tie(p->source.teacher_actor_id,p->source.student_actor_id,p->source.skill_id))return fail("ApprenticeshipProposalPending",head.first);}
            pending.insert(head.first);
        }else if(r){
            replies.push_back(head.first);
            if(r->kind!="family_apprenticeship_replied"||head.second.kind!=r->kind||(r->decision!="accepted"&&r->decision!="refused"&&r->decision!="withdrawn")||bool(r->confirmation)!=(r->decision=="accepted"))return fail("InvalidApprenticeshipReply",head.first);
            const auto* proposal=state.proposals.find(r->proposal_id);if(!proposal||!pending.count(r->proposal_id)||!seen.count(r->proposal_id)||out.terminal_proposals.count(r->proposal_id))return fail("ApprenticeshipProposalNotPending",head.first);
            const auto& source=proposal->source;const auto expected=r->decision=="withdrawn"?source.speaker_actor_id:source.listener_actor_id;if(r->actor_id!=expected)return fail("ApprenticeshipReplyAuthorityRequired",head.first);
            valid=decision_person(c,*interaction.common,access,r->actor_id,tick,head.first);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);
            if(r->confirmation){const auto& confirm=*r->confirmation;if(!same_terms(source,confirm)||confirm.tick!=tick||confirm.revision!=revision)return fail("ApprenticeshipConfirmationMismatch",head.first);valid=validate_apprenticeship_statement_source(c,*interaction.common,interaction,catalog,access,confirm,head.first);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);if(active_pair(out,source))return fail("ApprenticeshipAlreadyActive",head.first);out.bonds.insert(head.first,{head.first,r->proposal_id,source.teacher_actor_id,source.student_actor_id,source.skill_id,tick,{}});}
            pending.erase(r->proposal_id);out.terminal_proposals.insert(r->proposal_id);
        }else{
            exits.push_back(head.first);
            if(e->kind!="family_apprenticeship_exited"||head.second.kind!=e->kind)return fail("InvalidApprenticeshipExit",head.first);
            auto* bond=out.bonds.find(e->bond_id);if(!bond||bond->exit_fact_id||(e->actor_id!=bond->teacher_actor_id&&e->actor_id!=bond->student_actor_id))return fail("ApprenticeshipExitAuthorityRequired",head.first);
            valid=decision_person(c,*interaction.common,access,e->actor_id,tick,head.first);if(!valid.ok)return failed<ApprenticeshipTimeline>(valid);bond->exit_fact_id=head.first;
        }
    }
    if(seen.size()!=total)return fail("ApprenticeshipMissingFactHeader");if(proposals!=state.proposals.order()||replies!=state.replies.order()||exits!=state.exits.order())return fail("ApprenticeshipTableOrderMismatch");return {true,std::move(out),{}};
}
namespace {
Outcome<std::optional<ApprenticeshipBond>> read_bond_at_cut(const CommonStore& c,const FamilyApprenticeshipState& state,const StableId& bond_id,Integer tick,std::size_t limit){
    auto fail=[](const StableId& code)->Outcome<std::optional<ApprenticeshipBond>>{return {false,{},common_failure(code).error};};
    const auto& order=c.state().facts.order();if(tick<0||tick>c.tick()||limit>order.size())return fail("ApprenticeshipActualBoundaryRequired");const auto end=order.begin()+limit,at=std::find(order.begin(),order.end(),bond_id);
    const auto* accepted=state.replies.find(bond_id);const auto* header=c.state().facts.find(bond_id);
    if(!accepted||accepted->decision!="accepted"||!accepted->confirmation||!header||header->id!=bond_id||header->kind!=accepted->kind||header->domain!="family_apprenticeship"||!allocated(c,bond_id)||at==order.end())return fail("ApprenticeshipBondSourceRequired");
    const auto* proposal=state.proposals.find(accepted->proposal_id);if(!proposal||!same_terms(proposal->source,*accepted->confirmation))return fail("ApprenticeshipBondSourceRequired");
    if(at>=end){if(accepted->tick<tick)return fail("ApprenticeshipFactTimeMismatch");return {true,std::nullopt,{}};}
    if(accepted->tick>tick)return fail("ApprenticeshipFactTimeMismatch");
    ApprenticeshipBond out{bond_id,accepted->proposal_id,proposal->source.teacher_actor_id,proposal->source.student_actor_id,proposal->source.skill_id,accepted->tick,{}};
    for(const auto& row:state.exits.rows())if(row.second.bond_id==bond_id){const auto exited=std::find(order.begin(),order.end(),row.first);if(exited==order.end()||exited<=at||row.second.tick<accepted->tick)return fail("ApprenticeshipExitSourceRequired");if(exited<end){if(row.second.tick>tick||out.exit_fact_id)return fail("ApprenticeshipFactTimeMismatch");out.exit_fact_id=row.first;}else if(row.second.tick<tick)return fail("ApprenticeshipFactTimeMismatch");}
    return {true,std::move(out),{}};
}
}
Outcome<std::optional<ApprenticeshipBond>> read_apprenticeship_bond_before_fact(const CommonStore& c,const FamilyApprenticeshipState& state,const StableId& bond,Integer tick,const StableId& before){
 const auto& order=c.state().facts.order();const auto found=std::find(order.begin(),order.end(),before);const auto* head=c.state().facts.find(before);
 if(before.empty()||found==order.end()||!head||head->id!=before)return {false,{},common_failure("ApprenticeshipActualBoundaryRequired").error};
 return read_bond_at_cut(c,state,bond,tick,static_cast<std::size_t>(found-order.begin()));
}
Outcome<std::optional<ApprenticeshipBond>> read_apprenticeship_bond_at_original_prefix(const CommonStore& c,const FamilyApprenticeshipState& state,const StableId& bond,Integer tick,const CommonActualFactPrefix& prefix){auto cut=read_actual_common_fact_prefix(c,prefix);if(!cut.ok)return {false,{},cut.error};return read_bond_at_cut(c,state,bond,tick,cut.value);}
Outcome<StableId> receive_apprenticeship_proposal_in_candidate(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interaction,const InteractionAccess& contact,const ApprenticeshipCatalog& catalog,FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access,const ApprenticeshipStatementQuote& quote){
    auto current=validate_family_apprenticeship(c,interaction,catalog,state,access);if(!current.ok)return {false,{},current.error};auto valid=current_actor(c,*interaction.common,access);if(!valid.ok)return failed<StableId>(valid);valid=revalidate_apprenticeship_statement(c,meeting,interaction,contact,catalog,access,quote);if(!valid.ok)return failed<StableId>(valid);
    if(active_pair(current.value,quote.source))return failed<StableId>(common_failure("ApprenticeshipAlreadyActive"));if(pending_pair(state,current.value,quote.source))return failed<StableId>(common_failure("ApprenticeshipProposalPending"));
    auto id=c.next_id("apprenticeship");if(!id.ok)return id;ApprenticeshipProposal p;p.source=quote.source;if(!state.proposals.insert(id.value,p))return failed<StableId>(common_failure("DuplicateApprenticeshipProposal"));valid=c.append_fact({id.value,p.kind,"family_apprenticeship"});if(!valid.ok)return failed<StableId>(valid);auto full=validate_family_apprenticeship(c,interaction,catalog,state,access);return full.ok?id:Outcome<StableId>{false,{},full.error};
}
Outcome<StableId> reply_apprenticeship_proposal_in_candidate(CommonStore& c,const InteractionState& meeting,const InteractionCatalog& interaction,const InteractionAccess& contact,const ApprenticeshipCatalog& catalog,FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access,const StableId& proposal_id,const StableId& decision,const std::optional<ApprenticeshipStatementQuote>& confirmation){
    auto current=validate_family_apprenticeship(c,interaction,catalog,state,access);if(!current.ok)return {false,{},current.error};auto valid=current_actor(c,*interaction.common,access);if(!valid.ok)return failed<StableId>(valid);const auto* p=state.proposals.find(proposal_id);if(!p||current.value.terminal_proposals.count(proposal_id))return failed<StableId>(common_failure("ApprenticeshipProposalNotPending"));
    if((decision!="accepted"&&decision!="refused"&&decision!="withdrawn")||bool(confirmation)!=(decision=="accepted"))return failed<StableId>(common_failure("InvalidApprenticeshipReply"));const auto actor=c.state().metadata.player_actor_id;if(actor!=(decision=="withdrawn"?p->source.speaker_actor_id:p->source.listener_actor_id))return failed<StableId>(common_failure("ApprenticeshipReplyAuthorityRequired"));
    if(confirmation){if(!same_terms(p->source,confirmation->source))return failed<StableId>(common_failure("ApprenticeshipConfirmationMismatch"));valid=revalidate_apprenticeship_statement(c,meeting,interaction,contact,catalog,access,*confirmation);if(!valid.ok)return failed<StableId>(valid);}
    auto id=c.next_id("apprenticeship");if(!id.ok)return id;ApprenticeshipReply r;r.proposal_id=proposal_id;r.actor_id=actor;r.decision=decision;r.tick=c.tick();r.revision=c.state().identity.revision;if(confirmation)r.confirmation=confirmation->source;if(!state.replies.insert(id.value,r))return failed<StableId>(common_failure("DuplicateApprenticeshipReply"));valid=c.append_fact({id.value,r.kind,"family_apprenticeship"});if(!valid.ok)return failed<StableId>(valid);auto full=validate_family_apprenticeship(c,interaction,catalog,state,access);return full.ok?id:Outcome<StableId>{false,{},full.error};
}
Outcome<StableId> exit_apprenticeship_in_candidate(CommonStore& c,const InteractionCatalog& interaction,const ApprenticeshipCatalog& catalog,FamilyApprenticeshipState& state,const ApprenticeshipIdentityRead& access,const StableId& bond_id){
    auto current=validate_family_apprenticeship(c,interaction,catalog,state,access);if(!current.ok)return {false,{},current.error};auto valid=current_actor(c,*interaction.common,access);if(!valid.ok)return failed<StableId>(valid);const auto* bond=current.value.bonds.find(bond_id);const auto actor=c.state().metadata.player_actor_id;if(!bond||bond->exit_fact_id||(actor!=bond->teacher_actor_id&&actor!=bond->student_actor_id))return failed<StableId>(common_failure("ApprenticeshipExitAuthorityRequired"));
    auto id=c.next_id("apprenticeship");if(!id.ok)return id;ApprenticeshipExit e;e.bond_id=bond_id;e.actor_id=actor;e.tick=c.tick();e.revision=c.state().identity.revision;if(!state.exits.insert(id.value,e))return failed<StableId>(common_failure("DuplicateApprenticeshipExit"));valid=c.append_fact({id.value,e.kind,"family_apprenticeship"});if(!valid.ok)return failed<StableId>(valid);auto full=validate_family_apprenticeship(c,interaction,catalog,state,access);return full.ok?id:Outcome<StableId>{false,{},full.error};
}
}
