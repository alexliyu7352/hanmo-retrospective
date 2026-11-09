// 编译注册闭包实例化N的因果边；不把17阶段整finish搬进分数片，不以顺序号制造实际事件。
#include "exact_phase_graph.hpp"
#include "native_item_craft_state.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "resident/domains/military/military_calendar_phase.hpp"
#include "resident/domains/craft/craft_interval.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
namespace hanmo::resident {
namespace {
using S=ExactStage;
// 写口进入和所有离开路径均失效旧phase-current凭证；候选回滚不恢复authority代次。
class WriterBoundary {
    WorldBundle& world_;
public:
    explicit WriterBoundary(WorldBundle& world) noexcept:world_(world){world_.note_write_boundary();}
    ~WriterBoundary(){world_.note_write_boundary();}
    WriterBoundary(const WriterBoundary&)=delete;
    WriterBoundary& operator=(const WriterBoundary&)=delete;
};
struct PhaseFailure { RuleError error; };
void fail(const char* code,ErrorKind kind=ErrorKind::rule){RuleError e;e.original={code,"exact_phase_graph"};e.kind=kind;throw PhaseFailure{std::move(e)};}
template<class T> T take(Outcome<T> r){if(!r.ok)throw PhaseFailure{std::move(r.error)};return std::move(r.value);}
template<class T,class F> Outcome<T> guard(F fn){try{return {true,fn(),{}};}catch(PhaseFailure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){RuleError e;e.original={"ExactPhaseResourceExhausted","exact_phase_graph"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"ExactPhaseResourceExhausted","exact_phase_graph"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
template<class F> auto invoke_writer(WorldBundle& world,const StableId& phase,ExactProviderId provider,const MomentSpan& span,const WorldMoment& actual,F&& fn)->decltype(fn()) {
    WriterBoundary boundary(world);
    auto read_scope=take(world.enter_phase_read_context(phase,exact_provider_key(provider),span,actual));
    return fn();
}
template<class F> auto invoke_reader(const WorldBundle& world,const StableId& phase,ExactProviderId provider,const MomentSpan& span,const WorldMoment& actual,F&& fn)->decltype(fn()) {
    auto read_scope=take(world.enter_phase_read_context(phase,exact_provider_key(provider),span,actual));
    return fn();
}
// 两个末屏障彼此没有业务边；稳定枚举只决定独立计算次序，不声称实际同时或因果。
const std::vector<std::pair<S,S>>& required_edges(){static const std::vector<std::pair<S,S>> edges={{S::interval_motion_work,S::same_moment_hits},{S::same_moment_hits,S::immediate_consequences},{S::resource_recovery,S::estate_production},{S::estate_production,S::action_completions},{S::interval_motion_work,S::action_completions},{S::immediate_consequences,S::action_completions},{S::action_completions,S::accident_due_loss},{S::accident_due_loss,S::contact_install},{S::interval_motion_work,S::contact_install},{S::contact_install,S::patrol_arrival},{S::patrol_arrival,S::military_duty_period},{S::military_duty_period,S::work_inspection},{S::probe_window_close,S::contact_install},{S::contact_install,S::probe_next_open},{S::immediate_consequences,S::probe_next_open},{S::condition_due,S::life_age},{S::life_age,S::birth_due},{S::birth_due,S::guardianship_monitor},{S::guardianship_monitor,S::arcane_expiry},{S::due_message_receipt,S::political_due_close},{S::political_due_close,S::knowledge_reaction_close},{S::state_satisfaction,S::state_need_deadline_close},{S::growth_recovery,S::state_need_deadline_close},{S::action_completions,S::state_satisfaction},{S::immediate_consequences,S::state_satisfaction},{S::reply_receipts,S::custody_response_deadline_close},{S::exile_review,S::occupancy_evaluation},{S::occupancy_evaluation,S::achievement},{S::contact_install,S::social_exchange},{S::social_exchange,S::siege_results},{S::siege_results,S::campaign_adoption},{S::campaign_adoption,S::economic_supply_affairs},{S::economic_supply_affairs,S::due_message_receipt},{S::interval_motion_work,S::military_siege_period},{S::military_siege_period,S::same_moment_hits},{S::immediate_consequences,S::military_siege_battles_finished},{S::military_siege_battles_finished,S::estate_production}};return edges;}
bool valid_stage(S s){return static_cast<unsigned>(s)<=static_cast<unsigned>(S::military_siege_battles_finished);}
using Key=std::tuple<ExactProviderId,S,StableId,StableId,StableId,Integer,Integer>;
Key key(ExactProviderId p,const ExactEventPrediction& e){return {p,e.stage,e.actual_source_basis.domain,e.actual_source_basis.kind,e.actual_source_basis.id,e.source_schema,0};}
// 两种原派生腿可同刻存在；只有原 kind 入 once key，不采用任意 revision 制造再执行。
Key key(ExactProviderId p,const ExactDerivedDeadlinePrediction& e){auto result=key(p,e.original);std::get<6>(result)=static_cast<Integer>(e.kind)+1;return result;}
bool valid_derived_kind(ExactDerivedDeadlineKind kind){return kind==ExactDerivedDeadlineKind::safe_exit_termination_window||kind==ExactDerivedDeadlineKind::safe_exit_window_expiry;}
// 原围城宏观两腿只消费本期真实热batch，不能被任意分数due入口重复调用。
bool calendar_only_stage(S stage){return stage==S::military_siege_period||stage==S::military_siege_battles_finished;}
void prediction(const ExactPhaseRegistration& p,const ExactEventPrediction& e,const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget b){if(calendar_only_stage(e.stage))fail("CalendarStagePredictedAsExactEvent");if(!valid_stage(e.stage)||std::find(p.stages.begin(),p.stages.end(),e.stage)==p.stages.end()||!valid_source_ref(e.actual_source_basis)||e.source_schema<=0||e.dependency_revision<0||take(compare_world_moments(e.at,from,b))<0||take(compare_world_moments(e.at,to,b))>0)fail("InvalidExactEventPrediction");if(p.schedule_kind==ExactScheduleKind::boundary_only&&take(compare_fractions(e.at.sub_half_day,CanonicalFraction{},b))!=0)fail("BoundaryProviderPredictedFraction");}
// P21.1 仅指定政治期限派生槽；旧入境来源是依据，不能改称发生于本到期 M 的成果。
void derived_prediction(const ExactPhaseRegistration& p,const ExactDerivedDeadlinePrediction& e,const WorldMoment& from,const WorldMoment& to,ExactArithmeticBudget b){
    if(p.provider!=ExactProviderId::politics||e.original.stage!=S::political_due_close||
       !valid_derived_kind(e.kind)||std::find(p.derived_deadline_kinds.begin(),p.derived_deadline_kinds.end(),e.kind)==p.derived_deadline_kinds.end())
        fail("UnregisteredExactDerivedDeadline",ErrorKind::internal);
    prediction(p,e.original,from,to,b);
}
// callback排序只按真实声明边；稳定provider键仅打破无依赖节点的计算顺序。
std::vector<std::size_t> callback_order(const std::vector<ExactPhaseRegistration>& providers,int phase){
    std::map<ExactProviderId,std::size_t> index;std::map<std::size_t,std::size_t> degrees;std::map<std::size_t,std::vector<std::size_t>> followers;
    for(std::size_t i=0;i<providers.size();++i){const auto& p=providers[i];const bool enabled=phase==0?p.interval_writer:p.period_source_owner;const auto& required=phase==0?p.interval_after:phase==1?p.opening_after:p.closing_after;if(!enabled&&!required.empty())fail("DependenciesWithoutExactCallback");if(enabled){index.emplace(p.provider,i);degrees[i]=0;}}
    for(const auto& entry:index){const auto i=entry.second;const auto& p=providers[i];const auto& required=phase==0?p.interval_after:phase==1?p.opening_after:p.closing_after;std::set<ExactProviderId> unique;for(auto predecessor:required){auto at=index.find(predecessor);if(predecessor==p.provider||at==index.end()||!unique.insert(predecessor).second)fail("MissingExactCallbackDependency");followers[at->second].push_back(i);++degrees[i];}}
    std::set<std::size_t> ready;for(const auto& d:degrees)if(d.second==0)ready.insert(d.first);std::vector<std::size_t> out;while(!ready.empty()){auto i=*ready.begin();ready.erase(ready.begin());out.push_back(i);for(auto follower:followers[i])if(--degrees[follower]==0)ready.insert(follower);}if(out.size()!=index.size())fail("ExactCallbackDependencyCycle");return out;
}
// Fixed same-stage producer -> consumer edge. Full-N Craft/Repair outcomes
// must settle before their old facility window is resolved. This is a compiled
// causal dependency, not an incidental ordering of enum values. Other owners
// retain the original stable tie-breaker. No event or persistent state is added.
std::vector<std::size_t> due_provider_order(const std::vector<ExactPhaseRegistration>& providers,S stage){
    std::vector<std::size_t> degree(providers.size(),0),order;
    std::vector<std::vector<std::size_t>> followers(providers.size());
    std::set<std::size_t> selected;
    std::optional<std::size_t> crafting,facility;
    for(std::size_t i=0;i<providers.size();++i){
        const auto& p=providers[i];
        if(std::find(p.stages.begin(),p.stages.end(),stage)==p.stages.end())continue;
        selected.insert(i);
        if(p.provider==ExactProviderId::crafting)crafting=i;
        if(p.provider==ExactProviderId::facility_claims)facility=i;
    }
    if(stage==S::action_completions&&crafting&&facility){
        followers[*crafting].push_back(*facility);++degree[*facility];
    }
    std::set<std::size_t> ready;
    for(auto i:selected)if(!degree[i])ready.insert(i);
    while(!ready.empty()){
        const auto i=*ready.begin();ready.erase(ready.begin());order.push_back(i);
        for(auto next:followers[i])if(--degree[next]==0)ready.insert(next);
    }
    if(order.size()!=selected.size())fail("ExactSameStageDependencyCycle",ErrorKind::internal);
    return order;
}
void sources(const ExactPhaseRegistration& registration,const ExactPeriodOwnerSources& s,bool closed){
    if(!valid_source_ref(s.opening_source)||s.opening_schema<=0||s.closing_source.has_value()!=s.closing_schema.has_value()||(s.closing_source&&(!valid_source_ref(*s.closing_source)||*s.closing_schema<=0)))fail("InvalidExactPeriodSources");
    if(registration.period_closure_kind==ExactPeriodClosureKind::owner_result){if(closed!=s.closing_source.has_value())fail("InvalidExactPeriodSources");}
    else if(registration.period_closure_kind==ExactPeriodClosureKind::stage_barrier){
        // 真实关闭逐Opening项由原owner的close/read/validate_world_sources完整反验。
        // S0仅保同candidate末屏障顺序，不用Timeline索引或虚构业务成果充source。
        if(s.closing_source)fail("CalendarStageHasFabricatedClosureSource");
    }else fail("UnknownExactPeriodClosureKind");
}
}
ExactPhaseWriteContext::ExactPhaseWriteContext(AdvanceDetails& output):advance(output){}
ExactPhaseWriteContext::~ExactPhaseWriteContext()=default;
Outcome<MilitaryCalendarCandidate*> acquire_military_calendar_candidate(ExactPhaseWriteContext& writes,const ExactPeriodContext& period,ExactArithmeticBudget budget){return guard<MilitaryCalendarCandidate*>([&]{
    if(!writes.military_calendar)writes.military_calendar=std::make_unique<MilitaryCalendarCandidate>();
    take(begin_military_calendar_candidate(*writes.military_calendar,period,budget));
    return writes.military_calendar.get();
});}
const char* exact_provider_key(ExactProviderId id) noexcept {
    static constexpr const char* names[]={"motion","military","growth","local_economy","estate","commerce","interaction","social","affairs","tasks","family","politics","medicine","crafting","study","arcana","facility_claims"};
    const auto index=static_cast<unsigned>(id);return index<sizeof(names)/sizeof(names[0])?names[index]:"";
}
const char* exact_stage_key(ExactStage id) noexcept {
    static constexpr const char* names[]={"interval_motion_work","same_moment_hits","immediate_consequences","resource_recovery","estate_production","action_completions","accident_due_loss","contact_install","patrol_arrival","military_duty_period","work_inspection","probe_window_close","probe_next_open","condition_due","life_age","birth_due","guardianship_monitor","arcane_expiry","due_message_receipt","political_due_close","knowledge_reaction_close","state_satisfaction","growth_recovery","state_need_deadline_close","reply_receipts","custody_response_deadline_close","exile_review","occupancy_evaluation","achievement","social_exchange","siege_results","campaign_adoption","economic_supply_affairs","military_siege_period","military_siege_battles_finished"};
    const auto index=static_cast<unsigned>(id);return index<sizeof(names)/sizeof(names[0])?names[index]:"";
}
// 此完整包图必须所有固定stage都有真实注册；旧legacy/rest局部路线保原入口，不以空provider通过本图。
Outcome<ExactPhaseGraph> compose_exact_phase_graph(const std::vector<ExactPhaseRegistration>& compiled,const std::vector<ExactPhaseSelection>& selected,const std::vector<StableId>& legacy){return guard<ExactPhaseGraph>([&]{
    ExactPhaseGraph graph;std::set<ExactProviderId> owners;std::set<StableId> writers(legacy.begin(),legacy.end());std::set<S> stages;
    for(const auto& selection:selected){
        if(exact_provider_key(selection.provider)[0]=='\0'||selection.profile_id.empty()||selection.version<=0||!owners.insert(selection.provider).second)fail("InvalidExactProviderSelection");
        const ExactPhaseRegistration* found=nullptr;
        for(const auto& p:compiled)if(p.provider==selection.provider&&p.profile_id==selection.profile_id&&p.version==selection.version){if(found)fail("DuplicateCompiledExactProvider");found=&p;}
        if(!found)fail("MissingExactProvider",ErrorKind::unsupported_capability);
        const auto& p=*found;
        if(static_cast<unsigned>(p.schedule_kind)>static_cast<unsigned>(ExactScheduleKind::boundary_only))fail("UnknownExactScheduleKind");
        if(p.stages.empty()||p.effect_writer_keys.empty()||!p.validate_author_selection||!p.validate_world_sources)fail("IncompleteExactProvider",ErrorKind::unsupported_capability);
        const bool event_writer=bool(p.predict)||bool(p.apply_due)||bool(p.validate_applied_source);
        if(event_writer&&(!p.predict||!p.apply_due||!p.validate_applied_source))fail("IncompleteExactEventProvider",ErrorKind::unsupported_capability);
        const bool deadline_writer=!p.derived_deadline_kinds.empty()||bool(p.predict_derived_deadlines)||bool(p.apply_derived_deadline)||bool(p.validate_derived_deadline);
        if(deadline_writer&&(p.derived_deadline_kinds.empty()||!p.predict_derived_deadlines||!p.apply_derived_deadline||!p.validate_derived_deadline))fail("IncompleteExactDerivedDeadlineProvider",ErrorKind::unsupported_capability);
        if(deadline_writer&&(p.provider!=ExactProviderId::politics||p.schedule_kind!=ExactScheduleKind::exact_due))fail("InvalidExactDerivedDeadlineOwner",ErrorKind::internal);
        std::set<ExactDerivedDeadlineKind> deadline_kinds;
        for(auto kind:p.derived_deadline_kinds)if(!valid_derived_kind(kind)||!deadline_kinds.insert(kind).second)fail("InvalidExactDerivedDeadlineKind",ErrorKind::internal);
        if(p.calendar_stages.empty()!=!bool(p.apply_calendar_stage))fail("IncompleteExactCalendarProvider",ErrorKind::unsupported_capability);
        if(static_cast<unsigned>(p.period_closure_kind)>static_cast<unsigned>(ExactPeriodClosureKind::stage_barrier))fail("UnknownExactPeriodClosureKind");
        if(p.period_closure_kind==ExactPeriodClosureKind::stage_barrier&&(!p.period_source_owner||p.calendar_stages.empty()))fail("IncompleteExactCalendarPeriodOwner",ErrorKind::unsupported_capability);
        if(!event_writer&&!deadline_writer&&p.calendar_stages.empty())fail("IncompleteExactProvider",ErrorKind::unsupported_capability);
        if(p.adopt_interval_from&&!p.interval_writer)fail("IntervalFromWithoutIntervalWriter",ErrorKind::unsupported_capability);
        if(p.interval_writer!=static_cast<bool>(p.provide_interval)||p.period_source_owner!=static_cast<bool>(p.open_period)||p.period_source_owner!=static_cast<bool>(p.close_period)||p.period_source_owner!=static_cast<bool>(p.read_period_sources))fail("IncompleteExactProviderHooks",ErrorKind::unsupported_capability);
        std::set<S> own_stages;
        for(auto stage:p.stages){if(!valid_stage(stage)||!own_stages.insert(stage).second)fail("InvalidExactProviderStage");stages.insert(stage);}
        std::set<S> calendar_stages;
        for(auto stage:p.calendar_stages)if(!valid_stage(stage)||!own_stages.count(stage)||!calendar_stages.insert(stage).second)fail("InvalidExactCalendarStage");
        for(auto stage:own_stages)if(calendar_only_stage(stage)&&!calendar_stages.count(stage))fail("UnimplementedExactCalendarStage");
        if(deadline_writer&&!own_stages.count(S::political_due_close))fail("MissingExactDerivedDeadlineStage",ErrorKind::internal);
        if(!event_writer){auto implemented=calendar_stages;if(deadline_writer)implemented.insert(S::political_due_close);if(implemented!=own_stages)fail("UnimplementedExactCalendarStage");}
        for(const auto& writer:p.effect_writer_keys)if(writer.empty()||!writers.insert(writer).second)fail("DuplicateExactEffectWriter");
        graph.providers.push_back(p);
    }
    for(unsigned i=0;i<=static_cast<unsigned>(S::military_siege_battles_finished);++i)if(!stages.count(static_cast<S>(i)))fail("MissingExactCausalStage",ErrorKind::unsupported_capability);graph.causal_edges=required_edges();for(const auto& p:graph.providers)for(const auto& edge:p.causal_edges){if(!valid_stage(edge.first)||!valid_stage(edge.second)||edge.first==edge.second)fail("InvalidExactCausalEdge");if(std::find(graph.causal_edges.begin(),graph.causal_edges.end(),edge)==graph.causal_edges.end())graph.causal_edges.push_back(edge);}std::map<S,Integer> degrees;for(auto s:stages)degrees[s]=0;for(const auto& e:graph.causal_edges){if(!stages.count(e.first)||!stages.count(e.second))fail("MissingExactCausalDependency");++degrees[e.second];}std::set<S> ready;for(const auto& item:degrees)if(item.second==0)ready.insert(item.first);while(!ready.empty()){auto s=*ready.begin();ready.erase(ready.begin());graph.ordered_stages.push_back(s);for(const auto& e:graph.causal_edges)if(e.first==s&&--degrees[e.second]==0)ready.insert(e.second);}if(graph.ordered_stages.size()!=stages.size())fail("ExactPhaseDependencyCycle");std::sort(graph.providers.begin(),graph.providers.end(),[](const auto& a,const auto& b){return a.provider<b.provider;});graph.interval_order=callback_order(graph.providers,0);graph.opening_order=callback_order(graph.providers,1);graph.closing_order=callback_order(graph.providers,2);return graph;});}
Status validate_exact_phase_selection(const ExactPhaseGraph& graph,const WorldBundle& world){return guard<Done>([&]{if(!world.world_timeline)fail("MissingExactTimeline");const auto current=take(read_world_moment(world.common.tick(),*world.world_timeline));for(const auto& p:graph.providers)take(invoke_reader(world,"exact_author_selection",p.provider,{current,current},current,[&]{return p.validate_author_selection(world);}));return Done{};});}
Outcome<WorldMoment> select_exact_slice_end(const ExactPhaseGraph& graph,const WorldBundle& world,const WorldMoment& current,const WorldMoment& permit_to,ExactArithmeticBudget b){return guard<WorldMoment>([&]{
    if(take(compare_world_moments(current,permit_to,b))>=0)fail("PositiveExactPermitRequired");
    WorldMoment end=permit_to;
    if(current.half_day_tick==std::numeric_limits<Integer>::max())fail("WorldMomentOverflow");
    WorldMoment boundary{current.half_day_tick+1,{}};if(take(compare_world_moments(boundary,end,b))<0)end=boundary;
    for(const auto& p:graph.providers){
        const auto query_until=end;std::set<Key> unique;
        if(p.predict){
            auto due=take(invoke_reader(world,"predict_exact_boundary",p.provider,{current,query_until},current,[&]{return p.predict(world,current,query_until);}));
            for(const auto& e:due){prediction(p,e,current,query_until,b);if(!unique.insert(key(p.provider,e)).second)fail("DuplicateExactPrediction");if(take(compare_world_moments(e.at,end,b))<0)end=e.at;}
        }
        if(p.predict_derived_deadlines){
            auto due=take(invoke_reader(world,"predict_original_deadline",p.provider,{current,query_until},current,[&]{return p.predict_derived_deadlines(world,current,query_until);}));
            for(const auto& e:due){derived_prediction(p,e,current,query_until,b);if(!unique.insert(key(p.provider,e)).second)fail("DuplicateExactPrediction");if(take(compare_world_moments(e.original.at,end,b))<0)end=e.original.at;}
        }
    }
    return end;
});}
// 每次只采用一个真实来源再重读所有provider，避免旧预测绕过前驱写后的资格/费用变化。
Outcome<std::vector<ExactEventApplied>> consume_exact_due_events(const ExactPhaseGraph& graph,WorldBundle& world,const WorldMoment& at,bool boundary,ExactPhaseWriteContext& writes,ExactPhaseTechnicalBudget technical,ExactArithmeticBudget b,const ExactPeriodContext* closing_period){return guard<std::vector<ExactEventApplied>>([&]{
    if(boundary!=(take(compare_fractions(at.sub_half_day,CanonicalFraction{},b))==0))fail("ExactBoundaryClassificationMismatch");
    if(closing_period){
        const auto& period=*closing_period;
        if(!boundary||!world.world_timeline||world.world_timeline->boundary_state!=PeriodBoundaryState::open||period.period_id!=world.world_timeline->period_id||period.opening_at.half_day_tick==std::numeric_limits<Integer>::max()||period.through_tick!=period.opening_at.half_day_tick+1||take(compare_fractions(period.opening_at.sub_half_day,CanonicalFraction{},b))!=0||at.half_day_tick!=period.through_tick)fail("InvalidExactCalendarStageContext");
        const auto current=take(read_world_moment(world.common.tick(),*world.world_timeline,b));
        if(current.half_day_tick!=period.opening_at.half_day_tick||take(compare_world_moments(current,at,b))>=0)fail("InvalidExactCalendarStageContext");
    }
    // Materialize only this invocation's finite topological ranks. Every real
    // write below still re-predicts every provider before choosing another due.
    std::vector<std::vector<std::size_t>> provider_ranks(graph.ordered_stages.size(),std::vector<std::size_t>(graph.providers.size(),graph.providers.size()));
    for(std::size_t stage_rank=0;stage_rank<graph.ordered_stages.size();++stage_rank){
        const auto order=due_provider_order(graph.providers,graph.ordered_stages[stage_rank]);
        for(std::size_t rank=0;rank<order.size();++rank)provider_ranks[stage_rank][order[rank]]=rank;
    }
    std::set<Key> consumed;
    std::set<std::pair<std::size_t,S>> calendar_done;
    std::vector<ExactEventApplied> results;
    std::optional<std::size_t> completed_rank;
    std::size_t transitions=0;
    for(;;){
        struct Choice { std::size_t provider=0,rank=0;S stage=S::interval_motion_work;bool calendar=false;ExactEventPrediction prediction;std::optional<ExactDerivedDeadlinePrediction> deadline;Key source_key{}; };
        std::optional<Choice> chosen;
        auto earlier=[&](const Choice& a,const Choice& z){
            if(a.rank!=z.rank)return a.rank<z.rank;
            if(a.provider!=z.provider)return provider_ranks.at(a.rank).at(a.provider)<provider_ranks.at(z.rank).at(z.provider);
            // 同owner该stage的原日历producer先执行，再消费它实际产生的同stage来源。
            if(a.calendar!=z.calendar)return a.calendar;
            return a.source_key<z.source_key;
        };
        auto choose=[&](Choice next){if(completed_rank&&next.rank<*completed_rank)fail("UnregisteredSameMomentBackEdge");if(!chosen||earlier(next,*chosen))chosen=std::move(next);};
        for(std::size_t i=0;i<graph.providers.size();++i){
            const auto& p=graph.providers[i];
            if(closing_period)for(auto stage:p.calendar_stages){
                if(calendar_done.count({i,stage}))continue;
                auto pos=std::find(graph.ordered_stages.begin(),graph.ordered_stages.end(),stage);if(pos==graph.ordered_stages.end())fail("UnknownExactStage");
                Choice next;next.provider=i;next.rank=static_cast<std::size_t>(pos-graph.ordered_stages.begin());next.stage=stage;next.calendar=true;choose(std::move(next));
            }
            if(p.schedule_kind==ExactScheduleKind::boundary_only&&!boundary)continue;
            world.note_write_boundary();std::set<Key> unique;
            // 每个旧业务来源与原派生期限都进入同一个有限候选选择，不分两轮偷换前驱次序。
            auto original_choice=[&](const ExactEventPrediction& e,Key k){
                if(!unique.insert(k).second)fail("DuplicateExactPrediction");
                if(consumed.count(k))fail("ExactSourceDidNotAdvance",ErrorKind::internal);
                auto pos=std::find(graph.ordered_stages.begin(),graph.ordered_stages.end(),e.stage);if(pos==graph.ordered_stages.end())fail("UnknownExactStage");
                Choice next;next.provider=i;next.rank=static_cast<std::size_t>(pos-graph.ordered_stages.begin());next.stage=e.stage;next.prediction=e;next.source_key=std::move(k);return next;
            };
            if(p.predict){
                auto predictions=take(invoke_reader(world,"predict_exact_due",p.provider,{at,at},at,[&]{return p.predict(world,at,at);}));
                for(const auto& e:predictions){prediction(p,e,at,at,b);choose(original_choice(e,key(p.provider,e)));}
            }
            if(p.predict_derived_deadlines){
                auto deadlines=take(invoke_reader(world,"predict_original_deadline",p.provider,{at,at},at,[&]{return p.predict_derived_deadlines(world,at,at);}));
                for(const auto& e:deadlines){derived_prediction(p,e,at,at,b);auto next=original_choice(e.original,key(p.provider,e));next.deadline=e;choose(std::move(next));}
            }
        }
        if(!chosen)break;
        if(transitions>=technical.max_same_moment_transitions)fail("ExactPhaseTechnicalBudgetExhausted",ErrorKind::internal);
        const auto& p=graph.providers[chosen->provider];
        if(chosen->calendar){
            // 只记录本栈执行覆盖；实际cursor/钱粮/结果仍由原owner一次写，失败丢整候选。
            take(invoke_writer(world,exact_stage_key(chosen->stage),p.provider,{closing_period->opening_at,at},at,[&]{return p.apply_calendar_stage(world,chosen->stage,*closing_period,writes);}));
            calendar_done.insert({chosen->provider,chosen->stage});
        }else if(chosen->deadline){
            // 原owner只投影真正绝对期限的派生状态；后态验源仍是必需腿，没有空成果豁免。
            take(invoke_writer(world,exact_stage_key(chosen->stage),p.provider,{at,at},at,[&]{return p.apply_derived_deadline(world,*chosen->deadline,writes);}));
            world.note_write_boundary();
            take(invoke_reader(world,exact_stage_key(chosen->stage),p.provider,{at,at},at,[&]{return p.validate_derived_deadline(world,*chosen->deadline);}));
            consumed.insert(chosen->source_key);
        }else{
            auto applied=take(invoke_writer(world,exact_stage_key(chosen->stage),p.provider,{at,at},at,[&]{return p.apply_due(world,chosen->prediction,writes);}));
            if(!valid_source_ref(applied.applied_result)||applied.source_schema<=0)fail("MissingExactAppliedSource");
            world.note_write_boundary();
            take(invoke_reader(world,exact_stage_key(chosen->stage),p.provider,{at,at},at,[&]{return p.validate_applied_source(world,applied);}));
            consumed.insert(chosen->source_key);results.push_back(std::move(applied));
        }
        ++transitions;completed_rank=chosen->rank;
    }
    if(closing_period){
        bool selected=false;for(const auto& provider:graph.providers)for(auto stage:provider.calendar_stages)if(stage==S::military_siege_period)selected=true;
        if(selected&&!writes.military_calendar)fail("MissingMilitaryCalendarCandidate");
        if(writes.military_calendar)take(validate_military_calendar_candidate_consumed(*writes.military_calendar,*closing_period,b));
    }else if(writes.military_calendar)fail("MilitaryCalendarOutsidePeriodClosing");
    return results;
});}

Status provide_exact_intervals(const ExactPhaseGraph& graph,WorldBundle& world,const MomentSpan& span,ExactPhaseWriteContext& writes){return guard<Done>([&]{
    take(make_moment_span(span.from,span.to));
    if(take(compare_world_moments(span.from,span.to))>=0)fail("PositiveExactIntervalRequired");
    if(!world.world_timeline||world.world_timeline->boundary_state!=PeriodBoundaryState::open||take(compare_world_moments(take(read_world_moment(world.common.tick(),*world.world_timeline)),span.from))!=0)fail("ExactIntervalFromMismatch");
    if(writes.craft_interval)fail("CraftIntervalCandidateAlreadyPresent",ErrorKind::internal);
    if(!world.catalog)fail("CraftIntervalCatalogRequired",ErrorKind::internal);
    const auto item_craft=take(selected_native_item_craft_versions(world.catalog->identity));
    const bool craft_selected=item_craft.craft_jobs!=0;
    bool crafting_provider=false;
    for(auto index:graph.interval_order)if(graph.providers.at(index).provider==ExactProviderId::crafting){
        if(crafting_provider)fail("DuplicateCraftIntervalProvider",ErrorKind::internal);
        crafting_provider=true;
        if(craft_selected&&!graph.providers.at(index).adopt_interval_from)fail("CraftIntervalFromProviderRequired",ErrorKind::unsupported_capability);
    }
    if(craft_selected&&!crafting_provider)fail("CraftIntervalFromProviderRequired",ErrorKind::unsupported_capability);
    // select_exact_slice_end已选定该正span。第一轮只在真实from采用原准入/manifest/费用；
    // 第二轮才在to提供实际区间。任一失败使整个World候选不发布，热stamp仍失效。
    for(auto index:graph.interval_order){
        const auto& p=graph.providers.at(index);
        if(p.adopt_interval_from)take(invoke_writer(world,"interval_from_adoption",p.provider,span,span.from,[&]{return p.adopt_interval_from(world,span,writes);}));
    }
    for(auto index:graph.interval_order){
        const auto& p=graph.providers.at(index);
        take(invoke_writer(world,exact_stage_key(ExactStage::interval_motion_work),p.provider,span,span.to,[&]{return p.provide_interval(world,span,writes);}));
    }
    // 原 owner 核同候选/同 span 的全量消费；即使本片没有活 Job，也须真实空集合 token。
    if(craft_selected!=bool(writes.craft_interval))fail("CraftIntervalCandidateSelectionMismatch",ErrorKind::internal);
    if(writes.craft_interval){
        take(invoke_reader(world,"craft_interval_consumed",ExactProviderId::crafting,span,span.to,[&]{
            return validate_craft_interval_candidate_consumed(*writes.craft_interval,world.common,span);
        }));
        writes.craft_interval.reset();
    }
    return Done{};
});}
Outcome<std::vector<ExactPeriodOwnerSources>> open_exact_registered_period(const ExactPhaseGraph& graph,WorldBundle& world,const ExactPeriodContext& period,ExactPhaseWriteContext& writes){return guard<std::vector<ExactPeriodOwnerSources>>([&]{if(period.period_id.empty()||period.opening_at.half_day_tick==std::numeric_limits<Integer>::max()||period.through_tick!=period.opening_at.half_day_tick+1||take(compare_fractions(period.opening_at.sub_half_day,CanonicalFraction{}))!=0)fail("InvalidExactPeriodContext");std::vector<ExactPeriodOwnerSources> out;for(auto index:graph.opening_order){const auto& p=graph.providers.at(index);auto source=take(invoke_writer(world,"period_opening",p.provider,{period.opening_at,WorldMoment{period.through_tick,{}}},period.opening_at,[&]{return p.open_period(world,period,writes);}));sources(p,source,false);out.push_back(std::move(source));}return out;});}
Outcome<std::vector<ExactPeriodOwnerSources>> close_exact_registered_period(const ExactPhaseGraph& graph,WorldBundle& world,const ExactPeriodContext& period,ExactPhaseWriteContext& writes){return guard<std::vector<ExactPeriodOwnerSources>>([&]{std::vector<ExactPeriodOwnerSources> out;for(auto index:graph.closing_order){const auto& p=graph.providers.at(index);auto source=take(invoke_writer(world,"period_closing",p.provider,{period.opening_at,WorldMoment{period.through_tick,{}}},WorldMoment{period.through_tick,{}},[&]{return p.close_period(world,period,writes);}));sources(p,source,true);out.push_back(std::move(source));}return out;});}
Status validate_exact_registered_period_sources(const ExactPhaseGraph& graph,const WorldBundle& world,const ExactPeriodContext& period,PeriodBoundaryState state){return guard<Done>([&]{if(state!=PeriodBoundaryState::open&&state!=PeriodBoundaryState::ready)fail("UnknownExactPeriodBoundary");for(const auto& p:graph.providers)if(p.period_source_owner)sources(p,take(invoke_reader(world,"period_source_read",p.provider,{period.opening_at,WorldMoment{period.through_tick,{}}},take(read_world_moment(world.common.tick(),*world.world_timeline)),[&]{return p.read_period_sources(world,period);})),state==PeriodBoundaryState::ready);return Done{};});}
Status validate_exact_phase_sources(const ExactPhaseGraph& graph,const WorldBundle& world,const WorldMoment& at){return guard<Done>([&]{for(const auto& p:graph.providers)take(invoke_reader(world,"exact_source_closure",p.provider,{at,at},at,[&]{return p.validate_world_sources(world,at);}));return Done{};});}
Status validate_no_exact_due(const ExactPhaseGraph &graph,
                             const WorldBundle &world, const WorldMoment &at) {
  return guard<Done>([&] {
    for (const auto &p : graph.providers) {
      if (p.predict) {
        auto pending =
            take(invoke_reader(world, "predict_exact_due", p.provider, {at, at},
                               at, [&] { return p.predict(world, at, at); }));
        if (!pending.empty()) {
          // 保留原硬拒绝，诊断绑定真实首个未消费预测，不猜领域或制造事件。
          const auto &event = pending.front();
          RuleError error;
          error.original = {"UnconsumedExactHardEvent", "exact_phase_graph"};
          error.phase = exact_stage_key(event.stage);
          error.domain = exact_provider_key(p.provider);
          error.source_id = event.actual_source_basis.id;
          error.reason = event.actual_source_basis.domain + "/" +
                         event.actual_source_basis.kind;
          throw PhaseFailure{std::move(error)};
        }
      }
      if (p.predict_derived_deadlines) {
        auto pending = take(invoke_reader(
            world, "predict_original_deadline", p.provider, {at, at}, at,
            [&] { return p.predict_derived_deadlines(world, at, at); }));
        if (!pending.empty())
          fail("UnconsumedExactDerivedDeadline", ErrorKind::internal);
      }
    }
    return Done{};
  });
}
}
