#include "resident/domains/growth/growth_model.hpp"
// 逐Common事实重放原伤势增益和真实恢复点，最终对照唯一Actor现值并截取实际回队前界。
#include "growth_recovery_history.hpp"
#include "growth_personal_injury_history.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {namespace {
std::size_t pos(const CommonStore& c,const StableId& id){const auto& order=c.state().facts.order();return std::size_t(std::find(order.begin(),order.end(),id)-order.begin());}
bool header(const CommonStore& c,const StableId& id,const StableId& kind,const StableId& domain){const auto* f=c.state().facts.find(id);return f&&f->id==id&&f->kind==kind&&f->domain==domain&&pos(c,id)<c.state().facts.size();}
bool allocated(const CommonStore& c,const StableId& id,const StableId& prefix){const auto key=prefix+":";if(id.rfind(key,0)!=0)return false;const auto tail=id.substr(key.size());Integer n=0;auto read=std::from_chars(tail.data(),tail.data()+tail.size(),n);return read.ec==std::errc{}&&read.ptr==tail.data()+tail.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==tail;}
Outcome<Integer> limit(const CommonStore& c,const DynamicStepInput* phase){if(c.tick()<0)return {false,{},common_failure("GrowthRecoveryPhaseInvalid").error};if(!phase)return {true,c.tick(),{}};if(c.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1)return {false,{},common_failure("GrowthRecoveryPhaseInvalid").error};return {true,phase->to_tick,{}};}
// 生命资格按原死因头在业务边界之前与否判定；当前alive仅用于发现缺失死亡来源。
Outcome<bool> alive_before(const CommonStore& c,const GrowthRecoveryHistoryAccess& access,const StableId& actor,Integer tick,const StableId& boundary,const DynamicStepInput* phase){
    if(!access.death_source)return {false,false,common_failure("GrowthRecoveryDeathSourceRequired",actor).error};const auto* p=c.actor(actor);if(!p)return {false,false,common_failure("UnknownRecoveryActor",actor).error};auto read=access.death_source(actor,phase);if(!read.ok)return {false,false,read.error};if(!read.value){if(!p->alive)return {false,false,common_failure("GrowthRecoveryDeathSourceUnknown",actor).error};return {true,true,{}};}
    const auto& death=*read.value;auto through=limit(c,phase);if(!through.ok)return {false,false,through.error};if(death.actor_id!=actor||death.tick<0||death.tick>through.value||!header(c,death.casualty_fact_id,"military_casualties","military")||p->alive)return {false,false,common_failure("GrowthRecoveryDeathSourceMismatch",actor).error};const bool died=boundary.empty()||pos(c,death.casualty_fact_id)<pos(c,boundary);if((died&&death.tick>tick)||(!died&&death.tick<tick))return {false,false,common_failure("GrowthRecoveryDeathOrderMismatch",actor).error};return {true,!died,{}};
}
// 原life条款决定谁何时向谁购买何种服务；本账不重算或复制钱款。
Status service_source(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const StableId& id,const GrowthRecoveryService& s){
    const auto* original=growth.life_activities.find(id);const auto* selected=catalog.service_roles.find(s.role_id);const auto* terms=original?std::get_if<ServiceTerms>(&original->terms):nullptr;
    if(!c.actor(s.actor_id)||!c.actor(s.provider_actor_id)||!allocated(c,id,"action")||!header(c,id,"life_activity","growth")||!original||!terms||!selected||*selected!=s.method)return common_failure("GrowthRecoveryServiceSourceMissing",id);const auto& v=original->view;
    if(v.action_id!=id||v.fact_id!=id||v.kind!=LifeActivityKind::service||v.activity!="rest"||v.actor_id!=s.actor_id||v.provider_actor_id!=s.provider_actor_id||original->context.role_id!=s.role_id||original->context.place_id!=s.place_id||v.start_tick!=s.start_tick||terms->duration!=s.duration||original->cost_paid!=terms->cost)return common_failure("GrowthRecoveryServiceSourceMismatch",id);
    return common_success();
}
struct Snapshot {GrowthRecoveryProgress progress;StableId last_recovery;Integer recovered_tick=0;};
struct ActorRead {Snapshot observed;std::map<StableId,GrowthInjuryCause> causes;};
Outcome<ActorRead> actor_history(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& access,const StableId& actor,std::size_t boundary,Integer observed,const DynamicStepInput* phase){
    auto bad=[&](const StableId& code,const StableId& id=StableId{})->Outcome<ActorRead>{return {false,{},common_failure(code,id.empty()?actor:id).error};};auto through=limit(c,phase);if(!through.ok)return {false,{},through.error};
    // 原四种真实人物构造都从injury0开始；技能值只用于复用既有身份来源核，并不参与恢复数学。
    auto origin=read_growth_skill_origin(c,access.origins,actor,"commerce");if(!origin.ok)return {false,{},origin.error};const auto* person=c.state().actors.find(actor);if(!person||origin.value.created_tick>observed)return bad("GrowthRecoveryOriginUnavailable");std::size_t first_source=0;bool allocated_origin=!origin.value.source_fact_ids.empty();if(allocated_origin){first_source=pos(c,origin.value.source_fact_ids.back());if(first_source>=boundary)return bad("GrowthRecoveryOriginAfterBoundary");}
    auto damage=access.injuries.injury_causes(actor,phase);if(!damage.ok)return {false,{},damage.error};auto ordered=validate_growth_injury_cause_sequence(c,actor,damage.value,phase);if(!ordered.ok)return {false,{},ordered.error};ActorRead out;for(const auto& cause:damage.value){if(cause.tick<origin.value.created_tick||(allocated_origin&&pos(c,cause.source_fact_id)<=first_source))return bad("GrowthRecoveryInjuryPredatesIdentity",cause.source_fact_id);out.causes.emplace(cause.source_fact_id,cause);}
    Snapshot actual;bool captured=false;std::map<StableId,Integer> ordinals;std::map<StableId,StableId> last_credit;
    std::size_t index_counter=0;for(const auto& id:c.state().facts.order()){
        const auto index=index_counter++;if(!captured&&index>=boundary){out.observed=actual;captured=true;}
        if(const auto injury=out.causes.find(id);injury!=out.causes.end()){if((index<boundary&&injury->second.tick>observed)||(index>=boundary&&injury->second.tick<observed))return bad("GrowthRecoveryObservedOrderMismatch",id);auto next=apply_growth_injury_cause(actual.progress,injury->second);if(!next.ok)return {false,{},next.error};actual.progress=next.value;if(actual.progress.injury){actual.last_recovery.clear();actual.recovered_tick=0;}continue;}
        const auto* step=s.credits.find(id);if(!step)continue;const auto* service=s.services.find(step->action_id);if(!service||service->actor_id!=actor)continue;
        if((index<boundary&&step->tick>observed)||(index>=boundary&&step->tick<observed))return bad("GrowthRecoveryObservedOrderMismatch",id);
        if(!allocated(c,id,"recovery")||!header(c,id,"personal_recovery_credit","personal_injury_recovery")||step->tick>through.value||step->tick<origin.value.created_tick||pos(c,step->action_id)>=index||step->ordinal!=++ordinals[step->action_id]||step->before.injury!=actual.progress.injury||step->before.points!=actual.progress.points)return bad("GrowthRecoveryCreditSourceMismatch",id);
        auto living=alive_before(c,access,actor,step->tick,id,phase);if(!living.ok)return {false,{},living.error};if(!living.value)return bad("GrowthRecoveryAfterDeath",id);living=alive_before(c,access,service->provider_actor_id,step->tick,id,phase);if(!living.ok)return {false,{},living.error};if(!living.value)return bad("GrowthRecoveryProviderDead",id);
        if(!access.service_qualified_before)return bad("GrowthRecoveryQualificationRequired",id);auto qualified=access.service_qualified_before(*service,step->tick,id);if(!qualified.ok)return {false,{},qualified.error};auto next=apply_growth_recovery_points(actual.progress,step->offered_points);if(!next.ok)return {false,{},next.error};actual.progress=next.value.after;if(step->used_points>0&&actual.progress.injury==0){actual.last_recovery=id;actual.recovered_tick=step->tick;}last_credit[step->action_id]=id;
    }
    if(!captured)out.observed=actual;if(actual.progress.injury!=person->injury)return bad("GrowthRecoveryCurrentInjuryMismatch");
    for(const auto& row:s.services.rows())if(row.second.actor_id==actor){const auto& v=row.second;auto source=service_source(c,growth,catalog,row.first,v);if(!source.ok)return {false,{},source.error};const auto* original=growth.life_activities.find(row.first);const GrowthRecoveryClosure* closed=nullptr;StableId closure_id;for(const auto& finish:s.closures.rows())if(finish.second.action_id==row.first){closed=&finish.second;closure_id=finish.first;}
        const auto count=ordinals[row.first];if(!closed){const auto* action=c.action(row.first);if(original->view.phase!=LifeActivityPhase::active||!action||action->actor_id!=actor||action->kind!="service"||action->start_tick!=v.start_tick||action->remaining_ticks!=v.duration-count||v.start_tick>through.value||count>through.value-v.start_tick||count<c.tick()-v.start_tick)return bad("GrowthRecoveryActiveSourceMismatch",row.first);}
        else{if(!allocated(c,closure_id,"recovery")||!header(c,closure_id,"personal_recovery_closed","personal_injury_recovery")||closed->tick>through.value||closed->provided_half_days!=count||pos(c,closure_id)<=pos(c,row.first)||(!last_credit[row.first].empty()&&pos(c,closure_id)<=pos(c,last_credit[row.first]))||original->view.completed_tick!=closed->tick||original->view.phase!=(closed->reason=="completed"?LifeActivityPhase::completed:LifeActivityPhase::cancelled))return bad("GrowthRecoveryClosureSourceMismatch",closure_id);
            if(original->view.code!=(closed->reason=="completed"?StableId{}:closed->reason=="cancelled"?StableId{"ActionCancelled"}:StableId{"RecoveryParticipantDied"}))return bad("GrowthRecoveryClosureReasonMismatch",closure_id);
            if(closed->reason=="cancelled"&&closed->provided_half_days!=closed->tick-v.start_tick)return bad("GrowthRecoveryCancellationElapsedMismatch",closure_id);
            if(closed->reason=="patient_dead"||closed->reason=="provider_dead"){const auto& dead=closed->reason=="patient_dead"?actor:v.provider_actor_id;auto death=access.death_source?access.death_source(dead,phase):Outcome<std::optional<FamilyDeathOrigin>>{false,{},common_failure("GrowthRecoveryDeathSourceRequired",dead).error};if(!death.ok)return {false,{},death.error};if(!death.value||death.value->actor_id!=dead||!header(c,closed->cause_fact_id,"military_casualties","military")||!c.actor(dead)||c.actor(dead)->alive||death.value->casualty_fact_id!=closed->cause_fact_id||death.value->tick!=closed->tick||pos(c,closed->cause_fact_id)>=pos(c,closure_id))return bad("GrowthRecoveryClosureDeathMismatch",closure_id);const auto* battle=c.state().facts.find(death.value->battle_id);if(!battle)return bad("GrowthRecoveryDeathPhaseUnknown",closure_id);
                // 原陆战advance先伤亡后actions_step，水战决定则为稳定tick零时入口；未知新增阶段不猜提供时长。
                const bool before_actions=battle->kind=="military_battle"&&battle->domain=="military";
                const bool after_clock=(battle->kind=="naval_round"||battle->kind=="naval_capture")&&battle->domain=="military_naval";
                if((!before_actions&&!after_clock)||closed->provided_half_days!=closed->tick-v.start_tick-(before_actions?1:0))return bad("GrowthRecoveryDeathElapsedMismatch",closure_id);}
        }
    }
    return {true,std::move(out),{}};
}
}
Outcome<GrowthRecoveryProgress> read_growth_recovery_progress_in_candidate(const CommonStore& c,const GrowthState& g,const GrowthRecoveryCatalog& cat,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& a,const StableId& actor,const DynamicStepInput* phase){auto shape=validate_growth_recovery_shape(s);if(!shape.ok)return {false,{},shape.error};auto through=limit(c,phase);if(!through.ok)return {false,{},through.error};auto result=actor_history(c,g,cat,s,a,actor,c.state().facts.size(),through.value,phase);if(!result.ok)return {false,{},result.error};return {true,result.value.observed.progress,{}};}
Outcome<std::vector<StableId>> growth_recovery_life_pins(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& s){auto shape=validate_growth_recovery_shape(s);if(!shape.ok)return {false,{},shape.error};std::vector<StableId> out;for(const auto& row:s.services.rows()){auto valid=service_source(c,growth,catalog,row.first,row.second);if(!valid.ok)return {false,{},valid.error};out.push_back(row.first);}return {true,std::move(out),{}};}
Status validate_growth_recovery_history(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& access,const DynamicStepInput* phase){
    auto pins=growth_recovery_life_pins(c,growth,catalog,s);if(!pins.ok)return {false,{},pins.error};auto through=limit(c,phase);if(!through.ok)return {false,{},through.error};
    for(const auto& row:growth.life_activities.rows())if(row.second.view.kind==LifeActivityKind::service&&row.second.view.activity=="rest"&&catalog.service_roles.find(row.second.context.role_id)&&!s.services.find(row.first))return common_failure("GrowthRecoveryServiceBindingMissing",row.first);
    for(const auto& row:s.credits.rows())if(!allocated(c,row.first,"recovery")||!header(c,row.first,"personal_recovery_credit","personal_injury_recovery"))return common_failure("GrowthRecoveryCreditHeaderMissing",row.first);
    for(const auto& row:s.closures.rows())if(!allocated(c,row.first,"recovery")||!header(c,row.first,"personal_recovery_closed","personal_injury_recovery"))return common_failure("GrowthRecoveryClosureHeaderMissing",row.first);
    for(const auto& row:c.state().facts.rows())if(row.second.domain=="personal_injury_recovery"){if(row.second.kind=="personal_recovery_credit"){if(!s.credits.find(row.first))return common_failure("GrowthRecoveryCreditMissing",row.first);}else if(row.second.kind=="personal_recovery_closed"){if(!s.closures.find(row.first))return common_failure("GrowthRecoveryClosureMissing",row.first);}else return common_failure("UnknownGrowthRecoveryFact",row.first);}
    for(const auto& row:c.state().actors.rows()){auto read=actor_history(c,growth,catalog,s,access,row.first,c.state().facts.size(),through.value,phase);if(!read.ok)return {false,{},read.error};}return common_success();
}
Outcome<std::optional<GrowthRecoveredActor>> read_growth_recovered_actor(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& access,const StableId& actor,const StableId& casualty,Integer observed,const StableId& boundary,const DynamicStepInput* phase){
    auto bad=[&](const StableId& code)->Outcome<std::optional<GrowthRecoveredActor>>{return {false,{},common_failure(code,actor).error};};auto valid=validate_growth_recovery_history(c,growth,catalog,s,access,phase);if(!valid.ok)return {false,{},valid.error};auto through=limit(c,phase);if(!through.ok)return {false,{},through.error};if(observed<0||observed>through.value||(boundary.empty()&&observed!=through.value))return bad("GrowthRecoveryReadBoundaryInvalid");auto edge=c.state().facts.size();if(!boundary.empty()){if(!allocated(c,boundary,"military_recovery")||!header(c,boundary,"military_wounded_recovery","military"))return bad("GrowthRecoveryReadBoundaryInvalid");edge=pos(c,boundary);}
    auto read=actor_history(c,growth,catalog,s,access,actor,edge,observed,phase);if(!read.ok)return {false,{},read.error};const auto cause=read.value.causes.find(casualty);if(cause==read.value.causes.end()||cause->second.kind!=GrowthInjuryCauseKind::military_wound||cause->second.tick>observed||pos(c,casualty)>=edge)return bad("GrowthRecoveryOriginalWoundRequired");const auto& healed=read.value.observed;auto living=alive_before(c,access,actor,observed,boundary,phase);if(!living.ok)return {false,{},living.error};if(!living.value||healed.progress.injury||healed.last_recovery.empty()||pos(c,healed.last_recovery)<=pos(c,casualty))return {true,std::nullopt,{}};if(healed.recovered_tick>observed)return bad("GrowthRecoveryReadTimeMismatch");return {true,GrowthRecoveredActor{c.state().identity,actor,casualty,healed.last_recovery,boundary,observed,healed.recovered_tick},{}};
}
namespace {
// 稳定零时前界复用原逐因重放；全局分配和Common次序必须同侧，不能只移动头序伪造旧伤情。
Outcome<GrowthPersonalInjuryObservation> personal_injury(const CommonStore& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& state,const GrowthRecoveryHistoryAccess& access,const StableId& actor,Integer tick,const StableId& boundary){
    auto fail=[&](const StableId& code,const StableId& source=StableId{})->Outcome<GrowthPersonalInjuryObservation>{return {false,{},common_failure(code,source.empty()?actor:source).error};};
    if(tick<0||tick>c.tick()||(boundary.empty()&&tick!=c.tick()))return fail("GrowthPersonalInjuryBoundaryInvalid");
    const auto edge=boundary.empty()?c.state().facts.size():pos(c,boundary);
    if(!boundary.empty()&&(!allocated(c,boundary,"personal_entry")||!header(c,boundary,"personal_combat_entered","personal")))return fail("GrowthPersonalInjuryBoundaryInvalid");
    auto valid=validate_growth_recovery_history(c,growth,catalog,state,access);if(!valid.ok)return {false,{},valid.error};
    auto history=actor_history(c,growth,catalog,state,access,actor,edge,tick,nullptr);if(!history.ok)return {false,{},history.error};
    auto origin=boundary.empty()?read_growth_skill_origin(c,access.origins,actor,"commerce"):read_growth_skill_origin_before_fact(c,access.origins,actor,"commerce",tick,boundary);if(!origin.ok)return {false,{},origin.error};
    std::set<StableId> causes;
    auto serial=[](const StableId& id)->Integer{const auto colon=id.rfind(':');if(colon==StableId::npos)return -1;const auto digits=id.substr(colon+1);Integer n=0;const auto p=std::from_chars(digits.data(),digits.data()+digits.size(),n);return p.ec==std::errc{}&&p.ptr==digits.data()+digits.size()&&n>0&&std::to_string(n)==digits?n:-1;};
    auto observe=[&](const StableId& source,const StableId& completion=StableId{})->Status{
        const auto original=serial(source),n=completion.empty()?original:serial(completion);if(original<=0||original>=c.state().next_instance_id||pos(c,source)>=c.state().facts.size())return common_failure("GrowthPersonalInjurySourceInvalid",source);
        if(!completion.empty()&&(!allocated(c,completion,"result")||n<=original))return common_failure("GrowthPersonalInjuryCompletionSourceInvalid",source);
        if(!boundary.empty()&&((n<serial(boundary))!=(pos(c,source)<edge)||n==serial(boundary)))return common_failure("GrowthPersonalInjurySourceOrderInvalid",source);
        if(pos(c,source)<edge)causes.insert(source);return common_success();
    };
    for(const auto& id:origin.value.source_fact_ids){valid=observe(id);if(!valid.ok)return {false,{},valid.error};}
    // 连原边界后的真实新伤也核同侧；它可以改变今天，却不能倒填此前入场。
    std::set<StableId> completions;
    for(const auto& row:history.value.causes){const auto& cause=row.second;
        // 真实个人终局发生于battle分配之后，必须比较其原result分配序而非battle旧号。
        if(cause.kind==GrowthInjuryCauseKind::personal_defeat){if(cause.completion_id.empty()||!completions.insert(cause.completion_id).second)return fail("GrowthPersonalInjuryCompletionSourceRequired",row.first);}
        else if(!cause.completion_id.empty())return fail("GrowthPersonalInjuryCompletionSourceInvalid",row.first);
        valid=observe(row.first,cause.completion_id);if(!valid.ok)return {false,{},valid.error};}
    for(const auto& row:state.credits.rows()){
        const auto* service=state.services.find(row.second.action_id);if(!service||service->actor_id!=actor)continue;
        valid=observe(row.first);if(!valid.ok)return {false,{},valid.error};
        if(pos(c,row.first)<edge){valid=observe(row.second.action_id);if(!valid.ok)return {false,{},valid.error};}
    }
    GrowthPersonalInjuryObservation out;out.source_identity=c.state().identity;out.actor_id=actor;out.before_fact_id=boundary;out.observed_tick=tick;out.injury=history.value.observed.progress.injury;out.recovery_points=history.value.observed.progress.points;out.causal_fact_ids.assign(causes.begin(),causes.end());
    std::sort(out.causal_fact_ids.begin(),out.causal_fact_ids.end(),[&](const StableId& a,const StableId& b){return pos(c,a)<pos(c,b);});return {true,std::move(out),{}};
}
}
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury(const CommonStore& c,const GrowthState& g,const GrowthRecoveryCatalog& cat,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& a,const StableId& actor){return personal_injury(c,g,cat,s,a,actor,c.tick(),{});}
Outcome<GrowthPersonalInjuryObservation> read_growth_personal_entry_injury_before_fact(const CommonStore& c,const GrowthState& g,const GrowthRecoveryCatalog& cat,const GrowthRecoveryState& s,const GrowthRecoveryHistoryAccess& a,const StableId& actor,Integer tick,const StableId& fact){if(fact.empty())return {false,{},common_failure("GrowthPersonalInjuryBoundaryInvalid",actor).error};return personal_injury(c,g,cat,s,a,actor,tick,fact);}
}
