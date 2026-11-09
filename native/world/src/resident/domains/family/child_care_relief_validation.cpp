// 依实际Common序核两份独立照护的互斥和逐次消费；先验Birth/Life/军事史，不递归完整World。
#include "actor_life_eligibility.hpp"
#include "child_care_relief_resources.hpp"
#include <algorithm>
#include <charconv>
#include <tuple>
namespace hanmo::resident {namespace {
// 接受/结果和原Action各有自己的实际分配前缀，数字大小不替代Common事实顺序。
bool allocated(const CommonStore& c,const StableId& id,const StableId& prefix){const auto head=prefix+":";if(id.rfind(head,0)!=0)return false;const auto raw=id.substr(head.size());Integer n=0;const auto r=std::from_chars(raw.data(),raw.data()+raw.size(),n);return r.ec==std::errc{}&&r.ptr==raw.data()+raw.size()&&n>0&&n<c.state().next_instance_id&&raw==std::to_string(n);}
Status person(const ChildCareReliefAccess& access,const StableId& actor,Integer tick,const StableId& fact){return access.decision_maker_before?access.decision_maker_before(actor,tick,fact):common_failure("ChildCareReliefDecisionHistoryRequired");}
Status player(const ChildCareReliefAccess& access,const StableId& actor,Integer tick,const StableId& fact){if(!access.player_before)return common_failure("ChildCareReliefPlayerHistoryRequired");auto actual=access.player_before(tick,fact);return actual.ok&&actual.value==actor?common_success():actual.ok?common_failure("ChildCareReliefPersonalDecisionRequired"):Status{false,{},actual.error};}
Status living(const CommonStore& c,const FamilyDeathState& life,const StableId& actor,Integer tick,const StableId& fact){auto read=read_actor_alive_before_fact(c,life,actor,tick,fact);return read.ok?(read.value?common_success():common_failure("ChildCareReliefPersonAlreadyDead",actor)):Status{false,{},read.error};}
Status death(const CommonStore& c,const FamilyDeathState& life,const StableId& actor,Integer tick,const StableId& fact,const StableId& death_id){auto read=read_actor_death_before_fact(c,life,actor,tick,fact);return read.ok&&read.value&&read.value->death_fact_id==death_id?common_success():read.ok?common_failure("ChildCareReliefDeathSourceRequired",actor):Status{false,{},read.error};}
}
Outcome<ChildCareReliefTimeline> validate_child_care_relief(const CommonStore& c,const InteractionCatalog& interactions,const ChildCareReliefCatalog& catalog,const FamilyBirthState& births,const FamilyDeathState& life,const ChildCareReliefState& state,const ChildCareReliefAccess& access,const StableId& finishing_action,const StableId& starting_action){
    auto fail=[](const Status& s)->Outcome<ChildCareReliefTimeline>{return {false,{},s.error};};
    if(!interactions.common)return fail(common_failure("ChildCareReliefCommonRequired"));auto valid=validate_child_care_relief_catalog(catalog,*interactions.common,interactions);if(!valid.ok)return fail(valid);
    if(!catalog.enabled&&(!state.agreements.empty()||!state.starts.empty()||!state.care.empty()||!state.closures.empty()))return fail(common_failure("ChildCareReliefProfileRequired"));
    if(!starting_action.empty()&&(!finishing_action.empty()||state.starts.find(starting_action)||c.state().facts.find(starting_action)||!c.action(starting_action)||c.action(starting_action)->kind!="family_child_care_relief"||c.action(starting_action)->start_tick!=c.tick()||c.action(starting_action)->remaining_ticks<=0))return fail(common_failure("ChildCareReliefStartingActionRequired",starting_action));
    ChildCareReliefTimeline out;std::set<StableId> seen;OrderedTable<StableId> last_success;
    for(const auto& id:c.state().facts.order()){
        const auto* h=c.state().facts.find(id);if(!h||h->id!=id)return fail(common_failure("ChildCareReliefFactHeaderMismatch",id));
        if(h->domain=="family_birth"&&h->kind=="family_child_care_started"){
            const auto* original=births.care_starts.find(id);if(!original)return fail(common_failure("ChildCareOriginStartMismatch",id));if(out.active_by_child.find(original->child_actor_id))return fail(common_failure("ChildCareReliefOriginalOverlap",id));
        }
        if(h->domain!="family_care_relief")continue;
        if(!catalog.enabled)return fail(common_failure("ChildCareReliefProfileRequired",id));
        if(const auto* row=state.agreements.find(id)){
            const auto& s=row->source;if(h->kind!="family_care_relief_accepted"||row->kind!=h->kind||!allocated(c,id,"family_care_relief"))return fail(common_failure("ChildCareReliefAcceptanceHeaderMismatch",id));
            valid=validate_child_care_relief_source_before_acceptance(c,interactions,catalog,births,life,s,id);if(!valid.ok)return fail(valid);
            if(out.active_by_child.find(s.child_actor_id)||row->reserved_food!=s.terms.food_per_period*s.terms.prepaid_periods)return fail(common_failure("ChildCareReliefAcceptanceMismatch",id));
            valid=player(access,s.parent_actor_id,s.tick,id);if(!valid.ok)return fail(valid);for(const auto* who:{&s.parent_actor_id,&s.provider_actor_id}){valid=person(access,*who,s.tick,id);if(!valid.ok)return fail(valid);}
            out.active_by_child.insert(s.child_actor_id,id);out.remaining_food.insert(id,row->reserved_food);out.completed_periods.insert(id,0);seen.insert(id);continue;
        }
        if(const auto* row=state.starts.find(id)){
            const auto* accepted=state.agreements.find(row->accepted_id);const auto* active=out.active_by_child.find(row->child_actor_id);const auto* periods=out.completed_periods.find(row->accepted_id);
            if(h->kind!="family_care_relief_started"||row->kind!=h->kind||row->action_id!=id||!allocated(c,id,"action")||!accepted||!seen.count(row->accepted_id)||!active||*active!=row->accepted_id||!periods||out.pending_by_agreement.find(row->accepted_id))return fail(common_failure("ChildCareReliefStartSourceMismatch",id));
            const auto& s=accepted->source;if(std::tie(row->provider_actor_id,row->child_actor_id,row->place_id,row->duration)!=std::tie(s.provider_actor_id,s.child_actor_id,s.terms.place_id,s.terms.period_ticks)||row->period_index!=*periods+1||row->period_index>s.terms.prepaid_periods||row->tick<s.tick||row->tick>c.tick()||row->tick>2147483647-row->duration||row->revision<s.revision||row->revision>c.revision())return fail(common_failure("ChildCareReliefStartTermsMismatch",id));
            const auto* child=c.state().actors.find(row->child_actor_id);if(!child||!child->birth_tick||row->tick<*child->birth_tick||row->tick-*child->birth_tick>=18*720||row->tick+row->duration>*child->birth_tick+18*720)return fail(common_failure("ChildCareReliefMinorRequired",id));
            valid=person(access,row->provider_actor_id,row->tick,id);if(!valid.ok)return fail(valid);valid=living(c,life,row->child_actor_id,row->tick,id);if(!valid.ok)return fail(valid);
            out.pending_by_agreement.insert(row->accepted_id,id);seen.insert(id);continue;
        }
        if(const auto* row=state.care.find(id)){
            const auto* start=state.starts.find(row->action_id);const auto* pending=out.pending_by_agreement.find(row->accepted_id);const auto* accepted=state.agreements.find(row->accepted_id);
            if(h->kind!="family_care_relief_provided"||row->kind!=h->kind||!allocated(c,id,"family_care_relief")||!start||!accepted||!seen.count(row->action_id)||!pending||*pending!=row->action_id||out.completed_actions.count(row->action_id))return fail(common_failure("ChildCareReliefCompletionSourceMismatch",id));
            if(std::tie(row->accepted_id,row->provider_actor_id,row->child_actor_id,row->place_id,row->started_tick,row->period_index)!=std::tie(start->accepted_id,start->provider_actor_id,start->child_actor_id,start->place_id,start->tick,start->period_index)||row->tick<start->tick||row->tick>start->tick+start->duration||row->tick>c.tick()||row->revision<start->revision||row->revision>c.revision())return fail(common_failure("ChildCareReliefCompletionTermsMismatch",id));
            if(row->outcome=="completed"){
                if(row->tick!=start->tick+start->duration||row->food_consumed!=accepted->source.terms.food_per_period||!row->code.empty()||!row->interruption_fact_id.empty())return fail(common_failure("ChildCareReliefConsumptionMismatch",id));
                for(const auto* who:{&row->provider_actor_id,&row->child_actor_id}){valid=living(c,life,*who,row->tick,id);if(!valid.ok)return fail(valid);}
                ++*out.completed_periods.find(row->accepted_id);*out.remaining_food.find(row->accepted_id)-=row->food_consumed;auto* old=last_success.find(row->accepted_id);if(old)*old=id;else last_success.insert(row->accepted_id,id);
            }else if(row->outcome=="cancelled"){
                if(row->food_consumed!=0)return fail(common_failure("ChildCareReliefCancellationConsumedFood",id));
                if(row->code=="CareReliefProviderStopped"){
                    if(!row->interruption_fact_id.empty())return fail(common_failure("ChildCareReliefUnexpectedDeathSource",id));valid=player(access,row->provider_actor_id,row->tick,id);if(!valid.ok)return fail(valid);valid=living(c,life,row->provider_actor_id,row->tick,id);if(!valid.ok)return fail(valid);
                }else if(row->code=="CareReliefProviderDied"||row->code=="CareReliefChildDied"){
                    valid=death(c,life,row->code=="CareReliefProviderDied"?row->provider_actor_id:row->child_actor_id,row->tick,id,row->interruption_fact_id);if(!valid.ok)return fail(valid);
                }else return fail(common_failure("UnknownChildCareReliefOutcome",id));
            }else return fail(common_failure("UnknownChildCareReliefOutcome",id));
            out.pending_by_agreement.erase(row->accepted_id);out.completed_actions.insert(row->action_id);seen.insert(id);continue;
        }
        if(const auto* row=state.closures.find(id)){
            const auto* accepted=state.agreements.find(row->accepted_id);const auto* active=accepted?out.active_by_child.find(accepted->source.child_actor_id):nullptr;auto* remaining=out.remaining_food.find(row->accepted_id);
            if(h->kind!="family_care_relief_closed"||row->kind!=h->kind||!allocated(c,id,"family_care_relief")||!accepted||!active||*active!=row->accepted_id||!remaining||out.closed_agreements.count(row->accepted_id)||out.pending_by_agreement.find(row->accepted_id)||row->tick<accepted->source.tick||row->tick>c.tick()||row->revision<accepted->source.revision||row->revision>c.revision()||row->released_food!=*remaining)return fail(common_failure("ChildCareReliefClosureMismatch",id));
            const auto& s=accepted->source;
            if(row->reason=="finite_care_completed"){
                const auto* last=last_success.find(row->accepted_id);if(*remaining!=0||*out.completed_periods.find(row->accepted_id)!=s.terms.prepaid_periods||!last||row->source_fact_id!=*last||row->actor_id!=s.provider_actor_id)return fail(common_failure("ChildCareReliefCompletionClosureMismatch",id));
            }else if(row->reason=="provider_withdrew"||row->reason=="parent_withdrew"){
                if(!row->source_fact_id.empty()||row->actor_id!=(row->reason=="provider_withdrew"?s.provider_actor_id:s.parent_actor_id))return fail(common_failure("ChildCareReliefWithdrawalSourceMismatch",id));valid=player(access,row->actor_id,row->tick,id);if(!valid.ok)return fail(valid);valid=person(access,row->actor_id,row->tick,id);if(!valid.ok)return fail(valid);
            }else if(row->reason=="provider_died"||row->reason=="child_died"){
                if(row->actor_id!=(row->reason=="provider_died"?s.provider_actor_id:s.child_actor_id))return fail(common_failure("ChildCareReliefDeathSourceRequired",id));valid=death(c,life,row->actor_id,row->tick,id,row->source_fact_id);if(!valid.ok)return fail(valid);
            }else return fail(common_failure("UnknownChildCareReliefClosure",id));
            *remaining=0;out.active_by_child.erase(s.child_actor_id);out.closed_agreements.insert(row->accepted_id);seen.insert(id);continue;
        }
        return fail(common_failure("UnknownChildCareReliefPayload",id));
    }
    auto present=[&](const auto& rows){for(const auto& row:rows.rows())if(!seen.count(row.first))return false;return true;};if(!present(state.agreements)||!present(state.starts)||!present(state.care)||!present(state.closures))return fail(common_failure("MissingChildCareReliefFactHeader"));
    for(const auto& row:out.remaining_food.rows()){if(row.second==0&&!out.closed_agreements.count(row.first))return fail(common_failure("ChildCareReliefCompletionClosureRequired",row.first));valid=validate_child_care_relief_reservation(c,state,row.first,row.second);if(!valid.ok)return fail(valid);}
    bool finishing_seen=finishing_action.empty();
    for(const auto& row:state.starts.rows()){
        const auto* action=c.action(row.first);const bool done=out.completed_actions.count(row.first);if(row.first==finishing_action)finishing_seen=true;
        if(done){if(action&&row.first!=finishing_action)return fail(common_failure("CompletedChildCareReliefActionRetained",row.first));continue;}
        const auto& start=row.second;const auto* actor=c.actor(start.provider_actor_id);if(!action||!actor||actor->action_id!=row.first||action->id!=row.first||action->kind!="family_child_care_relief"||action->actor_id!=start.provider_actor_id||action->participants!=std::vector<StableId>{start.provider_actor_id}||action->start_tick!=start.tick||action->remaining_ticks<0||action->remaining_ticks>start.duration||c.tick()-start.tick!=start.duration-action->remaining_ticks||(action->remaining_ticks==0&&(finishing_action.empty()||c.tick()!=start.tick+start.duration)))return fail(common_failure("ChildCareReliefActionMismatch",row.first));
    }
    for(const auto& row:c.state().actions.rows())if(row.second.kind=="family_child_care_relief"&&!state.starts.find(row.first)&&row.first!=starting_action)return fail(common_failure("MissingChildCareReliefStart",row.first));
    if(!finishing_seen)return fail(common_failure("ChildCareReliefFinishingActionRequired",finishing_action));return {true,std::move(out),{}};
}
}
