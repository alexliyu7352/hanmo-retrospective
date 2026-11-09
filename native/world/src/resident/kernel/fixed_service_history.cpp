// 固定服务的真实边界序列与已供区间投影；技术切片不产生新history或业务成果。
#include "fixed_service_history.hpp"
#include "world_copy_plan.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <new>
#include <stdexcept>
#include <string_view>
#include <tuple>
namespace hanmo::resident {
Outcome<const FixedServiceTransition*> read_fixed_service_transition_source(
    const ActionElapsedStore& store,const FixedServiceTransitionRef& ref){
    if(ref.transition_id.empty()||ref.source_schema!=1)
        return {false,nullptr,common_failure("InvalidFixedServiceTransitionRef",ref.transition_id).error};
    const FixedServiceTransition* found=nullptr;
    for(const auto& row:store.fixed_service_transitions)if(row.transition_id==ref.transition_id){
        if(found)return {false,nullptr,common_failure("DuplicateFixedServiceTransition",ref.transition_id).error};
        found=&row;
    }
    if(!found||found->source_schema!=ref.source_schema)
        return {false,nullptr,common_failure("ActualFixedServiceTransitionRequired",ref.transition_id).error};
    return {true,found,{}};
}
namespace {
struct HistoryFailure { RuleError error; };
// All keys and row pointers borrow the unchanged input Store for this read.
template<class Row>using BorrowedHistoryMap=std::map<std::string_view,const Row*,
    std::less<std::string_view>,CheckpointAllocator<std::pair<const std::string_view,const Row*>>>;
using BorrowedHistoryIds=std::set<std::string_view,std::less<std::string_view>,CheckpointAllocator<std::string_view>>;
// 失配总是拒绝候选，不补历史或用当前身体猜过去。
void fail(const char* code){RuleError error;error.original={code,"action_elapsed/fixed_service_transitions"};throw HistoryFailure{std::move(error)};}
// 原算术技术故障继续原kind，不能在history层变成普通无来源。
template<class T>T take(Outcome<T> result){if(!result.ok)throw HistoryFailure{std::move(result.error)};return std::move(result.value);}
// 本纯叶失败没有任何世界写入或部分成功。
template<class T,class F>Outcome<T> guard(F&& fn){try{return {true,fn(),{}};}catch(const HistoryFailure& e){return {false,{},e.error};}catch(const std::bad_alloc&){RuleError e;e.original={"FixedHistoryResourceExhausted","action_elapsed"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){RuleError e;e.original={"FixedHistoryResourceExhausted","action_elapsed"};e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
// 完整source地址不能空缺，且不能由S0引用自己制造参与证明。
void source(const SourceRef& s,Integer schema){if(s.domain.empty()||s.kind.empty()||s.id.empty()||schema<=0)fail("InvalidFixedHistorySource");if(s.domain=="action_elapsed")fail("SelfReferencingFixedHistorySource");}
// 锁内规则身份按实际完整目录值匹配，不按相同显示名放行。
bool same_catalog(const CatalogIdentity& a,const CatalogIdentity& b){return std::tie(a.content_id,a.content_version,a.content_digest,a.ruleset_id,a.schema_sha256,a.pack_set_digest,a.module_versions,a.artifact_sha256)==std::tie(b.content_id,b.content_version,b.content_digest,b.ruleset_id,b.schema_sha256,b.pack_set_digest,b.module_versions,b.artifact_sha256);}
// 同一个输入不能靠重复排列扩成多份工时证据。
void inputs(const std::vector<FixedServiceInputRef>& values){for(std::size_t i=0;i<values.size();++i){if(auto* s=std::get_if<FixedAppliedInput>(&values[i]))source(s->source_ref,s->source_schema);else{const auto& rule=std::get<FixedLockedRuleInput>(values[i]);if(rule.rule_ref.empty()||rule.catalog_identity.content_id.empty()||rule.catalog_identity.content_digest.empty()||rule.catalog_identity.ruleset_id.empty())fail("InvalidFixedHistoryRule");}for(std::size_t j=0;j<i;++j)if(same_fixed_input_ref(values[i],values[j]))fail("DuplicateFixedHistoryInput");}}
// 输入有序且完整，后继previous必须逐项等于实际前行采用集。
bool same_inputs(const std::vector<FixedServiceInputRef>& a,const std::vector<FixedServiceInputRef>& b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),same_fixed_input_ref);}
// finite cause与真实边界kind严格分派；不把reason任意字符串当暂停授权。
void cause(const FixedServiceTransition& row){if(auto* c=std::get_if<FixedOwnerAdopted>(&row.cause)){source(c->owner_source_ref,c->owner_source_schema);if(row.kind!=FixedTransitionKind::started)fail("FixedHistoryCauseKindMismatch");}else if(auto* c=std::get_if<FixedOwnerStateChanged>(&row.cause)){source(c->owner_source_ref,c->owner_source_schema);if(c->transition_rule_ref.empty()||row.kind==FixedTransitionKind::started||row.kind==FixedTransitionKind::terminal)fail("FixedHistoryCauseKindMismatch");}else if(auto* c=std::get_if<FixedEligibilityChanged>(&row.cause)){source(c->original_cause_ref,c->original_cause_schema);if(c->transition_rule_ref.empty()||c->original_owner_execution_ref.empty()||row.kind==FixedTransitionKind::started||row.kind==FixedTransitionKind::terminal)fail("FixedHistoryCauseKindMismatch");}else if(auto* c=std::get_if<FixedOwnerTerminated>(&row.cause)){source(c->owner_result_ref,c->owner_result_schema);if(row.kind!=FixedTransitionKind::terminal)fail("FixedHistoryCauseKindMismatch");}else fail("InvalidFixedHistoryCause");}
// InputRebound changes only adopted inputs. Running/paused comes from the real
// preceding chain, so a changed input while paused cannot earn service.
bool running_after(FixedTransitionKind kind,bool prior){switch(kind){case FixedTransitionKind::started:case FixedTransitionKind::resumed:return true;case FixedTransitionKind::suspended:case FixedTransitionKind::terminal:return false;case FixedTransitionKind::input_rebound:return prior;}fail("InvalidFixedHistoryKind");return false;}
// 工量比较不能丢弃算术失败的状态。
bool same_units(const WorkUnits& a,const WorkUnits& b,ExactArithmeticBudget budget){return take(compare_fractions(a.value,b.value,budget))==0;}
// 用真实Running区间精确推导一个已发生边界的信用，不更新entry。
WorkUnits through(const FixedServiceTransition& row,const WorldMoment& at,const TimeScaleRef& scale,ExactArithmeticBudget budget,bool active){if(!active)return row.credited_at;return take(add_work_units(row.credited_at,take(duration_to_scale_units(take(elapsed_world_duration(row.occurred_at,at,budget)),scale.units_per_half_day,budget)),budget));}
// 按原表序收集单Action，不能按ID字典顺序偷改同刻因果。
CheckpointVector<const FixedServiceTransition*> chain(const ActionElapsedStore& store,std::string_view action){CheckpointVector<const FixedServiceTransition*> result;for(const auto& row:store.fixed_service_transitions)if(row.action_ref==action)result.push_back(&row);return result;}
}
namespace {
CheckpointCopyLease interval_view_memory(const ActualServiceIntervalViewBuildView& value){

    return CheckpointCopyLease::reserve();
}
std::vector<ActualServiceInterval> copied_intervals(const ActualServiceIntervalsBuildView& value){
    std::vector<ActualServiceInterval> result;result.reserve(value.values.size());
    for(const auto& row:value.values){
        std::optional<StableId> closing;
        if(row.closing_transition_ref)closing.emplace(row.closing_transition_ref->data(),row.closing_transition_ref->size());
        result.push_back(ActualServiceInterval{row.from,row.until,
            StableId(row.opening_transition_ref.data(),row.opening_transition_ref.size()),
            std::move(closing),row.input_basis_refs});
    }
    return result;
}
ActualServiceIntervalViewBuildView borrowed_interval_view(const ActualServiceIntervalView& value){
    ActualServiceIntervalViewBuildView result{value.action_ref,{},value.credited_before,
        value.credited_after,value.covered_from,value.covered_until,value.complete};
    result.intervals.values.reserve(value.intervals.size());
    for(const auto& row:value.intervals){
        std::optional<std::string_view> closing;
        if(row.closing_transition_ref)closing=*row.closing_transition_ref;
        result.intervals.values.push_back({row.from,row.until,row.opening_transition_ref,
            closing,row.input_basis_refs});
    }
    return result;
}
}
ActualServiceIntervalView::ActualServiceIntervalView(const ActualServiceIntervalViewBuildView& value)
    :copied_fields_(interval_view_memory(value)),action_ref(value.action_ref),
     intervals(copied_intervals(value.intervals)),credited_before(value.credited_before),
     credited_after(value.credited_after),covered_from(value.covered_from),
     covered_until(value.covered_until),complete(value.complete){}
ActualServiceIntervalView::ActualServiceIntervalView(const ActualServiceIntervalView& value)
    :ActualServiceIntervalView(borrowed_interval_view(value)){}
ActualServiceIntervalView::ActualServiceIntervalView(ActualServiceIntervalView&& value)
    :ActualServiceIntervalView(borrowed_interval_view(value)){}
ActualServiceIntervalView& ActualServiceIntervalView::operator=(const ActualServiceIntervalView& value){
    if(this!=&value){ActualServiceIntervalView fresh(value);swap(fresh);}return *this;
}
ActualServiceIntervalView& ActualServiceIntervalView::operator=(ActualServiceIntervalView&& value){
    if(this!=&value){ActualServiceIntervalView fresh(std::move(value));swap(fresh);}return *this;
}
void ActualServiceIntervalView::swap(ActualServiceIntervalView& value){
    if(this==&value)return;
    // A failed exact-number swap may leave fields on either side. Both actual
    // owners stay alive until each resulting private object is destroyed.
    auto left=copied_fields_,right=value.copied_fields_;
    auto both=CheckpointCopyLease::hold_both(left,right);copied_fields_=both;value.copied_fields_=both;
    using std::swap;action_ref.swap(value.action_ref);intervals.swap(value.intervals);
    swap(credited_before,value.credited_before);swap(credited_after,value.credited_after);
    swap(covered_from,value.covered_from);swap(covered_until,value.covered_until);swap(complete,value.complete);
    copied_fields_=std::move(right);value.copied_fields_=std::move(left);
}
// SourceRef只身份相等，原source实际payload须由对应owner reader核验。
// 不同输入联合分支不互换；LockedRule同时反验目录身份与原rule键。
bool same_fixed_input_ref(const FixedServiceInputRef& a,const FixedServiceInputRef& b){if(a.index()!=b.index())return false;if(auto* left=std::get_if<FixedAppliedInput>(&a)){const auto& right=std::get<FixedAppliedInput>(b);return same_source_ref(left->source_ref,right.source_ref)&&left->source_schema==right.source_schema;}const auto& left=std::get<FixedLockedRuleInput>(a);const auto& right=std::get<FixedLockedRuleInput>(b);return same_catalog(left.catalog_identity,right.catalog_identity)&&left.rule_ref==right.rule_ref;}
// 重放只比较原不可变转换；不会把旧entry投影覆盖到已发生的后继时间。
Outcome<bool> same_fixed_service_transition(const FixedServiceTransition& a,const FixedServiceTransition& z,ExactArithmeticBudget b){return guard<bool>([&]{
    if(a.transition_id!=z.transition_id||a.source_schema!=z.source_schema||a.action_ref!=z.action_ref||a.transition_ordinal!=z.transition_ordinal||a.previous_transition_ref!=z.previous_transition_ref||a.kind!=z.kind||!same_source_ref(a.execution_source_ref,z.execution_source_ref)||a.execution_source_schema!=z.execution_source_schema||a.terminal_disposition!=z.terminal_disposition||a.cause.index()!=z.cause.index()||!same_inputs(a.input_basis_refs,z.input_basis_refs)||!same_inputs(a.previous_input_basis_refs,z.previous_input_basis_refs))return false;
    if(take(compare_world_moments(a.occurred_at,z.occurred_at,b))!=0||!same_units(a.credited_at,z.credited_at,b))return false;
    if(const auto* x=std::get_if<FixedOwnerAdopted>(&a.cause)){const auto& y=std::get<FixedOwnerAdopted>(z.cause);return same_source_ref(x->owner_source_ref,y.owner_source_ref)&&x->owner_source_schema==y.owner_source_schema;}
    if(const auto* x=std::get_if<FixedOwnerStateChanged>(&a.cause)){const auto& y=std::get<FixedOwnerStateChanged>(z.cause);return same_source_ref(x->owner_source_ref,y.owner_source_ref)&&x->owner_source_schema==y.owner_source_schema&&x->transition_rule_ref==y.transition_rule_ref;}
    if(const auto* x=std::get_if<FixedEligibilityChanged>(&a.cause)){const auto& y=std::get<FixedEligibilityChanged>(z.cause);return same_source_ref(x->original_cause_ref,y.original_cause_ref)&&x->original_cause_schema==y.original_cause_schema&&x->transition_rule_ref==y.transition_rule_ref&&x->original_owner_execution_ref==y.original_owner_execution_ref;}
    const auto& x=std::get<FixedOwnerTerminated>(a.cause);const auto& y=std::get<FixedOwnerTerminated>(z.cause);return same_source_ref(x.owner_result_ref,y.owner_result_ref)&&x.owner_result_schema==y.owner_result_schema;
});}
// 所有已存链和尾一起校验；删掉末事件后不能用合法current和due掩盖信用错配。
static Status validate_fixed_history_phase_values(const ActionElapsedStore& store,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget,const StableId* selected_action,bool completion_due){return guard<Done>([&]{
    take(validate_time_scale(scale));BorrowedHistoryMap<ActionElapsedState> entries;BorrowedHistoryIds ids;BorrowedHistoryMap<ActionElapsedTerminalBinding> terminals;
    for(const auto& variant:store.entries){const auto* fixed=std::get_if<ActionElapsedState>(&variant);if(!fixed)continue;const auto& entry=*fixed;if(selected_action&&entry.action_ref!=*selected_action)continue;if(!entries.emplace(entry.action_ref,&entry).second)fail("DuplicateFixedElapsedAction");take(completion_due?validate_fixed_elapsed_at_completion_due(entry,current,scale,budget):validate_fixed_elapsed_value(entry,current,scale,budget));}
    for(const auto& binding:store.terminal_bindings){if(binding.timing_kind!=0||(selected_action&&binding.action_ref!=*selected_action))continue;if(!terminals.emplace(binding.action_ref,&binding).second)fail("DuplicateFixedTerminalBinding");if(!entries.count(binding.action_ref))fail("OrphanFixedTerminalBinding");if(binding.action_kind.empty()||binding.actor_id.empty()||binding.terminal_result_ref.empty()||binding.last_closed_gate_result_ref||!binding.retained_gate_result_refs.empty())fail("InvalidFixedTerminalBinding");source(binding.original_execution_source_ref,1);if(!std::holds_alternative<MomentOwnerSource>(binding.started_source_ref)||!std::holds_alternative<MomentOwnerSource>(binding.ended_source_ref))fail("FixedTerminalRequiresMomentSource");for(const auto* timed:{&std::get<MomentOwnerSource>(binding.started_source_ref),&std::get<MomentOwnerSource>(binding.ended_source_ref)})source(timed->source_ref,timed->source_schema);}
    for(const auto& row:store.fixed_service_transitions){if(selected_action&&row.action_ref!=*selected_action)continue;if(row.transition_id.empty()||!ids.insert(row.transition_id).second||row.source_schema!=1||!entries.count(row.action_ref)||row.transition_ordinal<0)fail("InvalidFixedHistoryIdentity");source(row.execution_source_ref,row.execution_source_schema);cause(row);inputs(row.input_basis_refs);inputs(row.previous_input_basis_refs);if(take(compare_world_moments(row.occurred_at,current,budget))>0)fail("FutureFixedHistory");if(take(compare_fractions(row.credited_at.value,entries.at(row.action_ref)->required_units.value,budget))>0)fail("FixedHistoryCreditExceedsRequired");if(row.kind!=FixedTransitionKind::terminal&&row.terminal_disposition)fail("UnexpectedFixedTerminalDisposition");}
    for(const auto& pair:entries){const auto& entry=*pair.second;auto rows=chain(store,pair.first);if(rows.empty())fail("MissingFixedHistory");const auto& first=*rows.front();if(first.kind!=FixedTransitionKind::started||first.transition_ordinal!=0||first.previous_transition_ref||!first.previous_input_basis_refs.empty()||!same_units(first.credited_at,WorkUnits{},budget)||take(compare_world_moments(first.occurred_at,entry.started_at,budget))!=0)fail("InvalidFixedHistoryStart");
        bool active=true;
        for(std::size_t i=1;i<rows.size();++i){const auto& prior=*rows[i-1];const auto& row=*rows[i];if(row.transition_ordinal!=Integer(i)||!row.previous_transition_ref||*row.previous_transition_ref!=prior.transition_id||row.kind==FixedTransitionKind::started||prior.kind==FixedTransitionKind::terminal)fail("BrokenFixedHistoryChain");if(take(compare_world_moments(prior.occurred_at,row.occurred_at,budget))>0)fail("InvalidFixedHistoryTimeOrder");if(!same_inputs(row.previous_input_basis_refs,prior.input_basis_refs))fail("FixedHistoryInputPredecessorMismatch");if(!same_units(through(prior,row.occurred_at,scale,budget,active),row.credited_at,budget))fail("FixedHistoryCreditMismatch");if(row.kind==FixedTransitionKind::resumed&&active)fail("InvalidFixedHistoryResume");if(row.kind==FixedTransitionKind::suspended&&!active)fail("InvalidFixedHistoryStateChange");if(row.kind==FixedTransitionKind::input_rebound&&same_inputs(row.input_basis_refs,row.previous_input_basis_refs))fail("UnchangedFixedHistoryInput");active=running_after(row.kind,active);}
        const auto& last=*rows.back();if(!same_units(through(last,entry.last_settled_at,scale,budget,active),entry.credited_units,budget))fail("FixedHistoryTailCreditMismatch");
        if(const auto* terminal=std::get_if<ElapsedTerminal>(&entry.timing)){if(last.kind!=FixedTransitionKind::terminal||!last.terminal_disposition||!last.input_basis_refs.empty()||!terminals.count(entry.action_ref)||take(compare_world_moments(last.occurred_at,entry.last_settled_at,budget))!=0)fail("InvalidFixedHistoryTerminal");auto disposition=*last.terminal_disposition;if(disposition<FixedTerminalDisposition::completed||disposition>FixedTerminalDisposition::replaced_by_original_action)fail("InvalidFixedTerminalDisposition");if(disposition==FixedTerminalDisposition::completed&&!same_units(entry.credited_units,entry.required_units,budget))fail("IncompleteFixedServiceCompleted");const auto& binding=*terminals.at(entry.action_ref);const auto& adopted=std::get<FixedOwnerAdopted>(first.cause);const auto& ended=std::get<FixedOwnerTerminated>(last.cause);const auto& start_source=std::get<MomentOwnerSource>(binding.started_source_ref);const auto& end_source=std::get<MomentOwnerSource>(binding.ended_source_ref);if(binding.terminal_result_ref!=terminal->outcome_ref||!same_source_ref(start_source.source_ref,adopted.owner_source_ref)||start_source.source_schema!=adopted.owner_source_schema||!same_source_ref(end_source.source_ref,ended.owner_result_ref)||end_source.source_schema!=ended.owner_result_schema)fail("FixedHistoryTerminalSourceMismatch");}
        else{if(terminals.count(entry.action_ref))fail("ActiveFixedHasTerminalBinding");if(last.kind==FixedTransitionKind::terminal||std::holds_alternative<ElapsedRunning>(entry.timing)!=active)fail("FixedHistoryCurrentStateMismatch");}
    }return Done{};});}
Status validate_fixed_history_values(const ActionElapsedStore& store,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return validate_fixed_history_phase_values(store,current,scale,budget,nullptr,false);}
// 有限只读派生不会把暂停空档、未来时间或零跨度变成已服务区间。
static Outcome<ActualServiceIntervalView> read_fixed_service_phase_intervals(const ActionElapsedStore& store,const StableId& action,const WorldMoment& from,const WorldMoment& until,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget,bool completion_due){return guard<ActualServiceIntervalView>([&]{
    take(validate_fixed_history_phase_values(store,current,scale,budget,&action,completion_due));
    const ActionElapsedState* entry=find_fixed_elapsed(store,action);if(!entry)fail("MissingFixedElapsedAction");
    if(take(compare_world_moments(from,until,budget))>0||take(compare_world_moments(from,entry->started_at,budget))<0||take(compare_world_moments(until,entry->last_settled_at,budget))>0)fail("InvalidFixedServiceReadWindow");
    auto rows=chain(store,action);
    auto credit_at=[&](const WorldMoment& at){const FixedServiceTransition* selected=rows.front();bool active=false;for(const auto* row:rows)if(take(compare_world_moments(row->occurred_at,at,budget))<=0){selected=row;active=running_after(row->kind,active);}else break;return through(*selected,at,scale,budget,active);};
    auto credited_before=credit_at(from);auto credited_after=credit_at(until);
    ActualServiceIntervalsBuildView intervals;
    bool active=false;
    for(std::size_t i=0;i<rows.size();++i){
        const auto& row=*rows[i];active=running_after(row.kind,active);if(!active)continue;
        auto end=i+1<rows.size()?rows[i+1]->occurred_at:entry->last_settled_at;auto begin=row.occurred_at;
        if(take(compare_world_moments(begin,from,budget))<0)begin=from;
        if(take(compare_world_moments(end,until,budget))>0)end=until;
        if(take(compare_world_moments(begin,end,budget))>=0)continue;
        std::optional<std::string_view> closing;
        if(i+1<rows.size()&&take(compare_world_moments(end,rows[i+1]->occurred_at,budget))==0)closing=rows[i+1]->transition_id;
        intervals.values.push_back({std::move(begin),std::move(end),row.transition_id,closing,row.input_basis_refs});
    }
    return ActualServiceIntervalView(ActualServiceIntervalViewBuildView{action,std::move(intervals),
        credited_before,credited_after,from,until,true});
});}
Outcome<ActualServiceIntervalView> read_fixed_service_intervals(const ActionElapsedStore& store,const StableId& action,const WorldMoment& from,const WorldMoment& until,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return read_fixed_service_phase_intervals(store,action,from,until,current,scale,budget,false);}
Outcome<ActualServiceIntervalView> read_fixed_service_intervals_at_completion_due(const ActionElapsedStore& store,const StableId& action,const WorldMoment& from,const WorldMoment& until,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return read_fixed_service_phase_intervals(store,action,from,until,current,scale,budget,true);}
// 按原真实采用次序反解信用，不按ID、当前身体或兼容remaining推算已经供过的服务。
Outcome<WorldMoment> read_fixed_service_credit_boundary(const ActionElapsedStore& store,const StableId& action,const WorkUnits& target,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return guard<WorldMoment>([&]{
    const auto* entry=find_fixed_elapsed(store,action);if(!entry)fail("MissingFixedElapsedAction");
    const bool completing=std::holds_alternative<ElapsedRunning>(entry->timing)&&same_units(entry->credited_units,entry->required_units,budget);
    take(validate_fixed_history_phase_values(store,current,scale,budget,&action,completing));
    if(take(compare_fractions(target.value,entry->credited_units.value,budget))>0)fail("FixedServiceCreditNotReached");
    if(same_units(target,WorkUnits{},budget))return entry->started_at;
    const auto rows=chain(store,action);
    bool active=false;
    for(std::size_t i=0;i<rows.size();++i){const auto& row=*rows[i];active=running_after(row.kind,active);const auto relative=take(compare_fractions(target.value,row.credited_at.value,budget));
        if(relative==0)return row.occurred_at;
        if(relative<0)fail("FixedServiceCreditBoundaryGap");
        if(!active)continue;
        const auto& end=i+1<rows.size()?rows[i+1]->occurred_at:entry->last_settled_at;
        auto after=through(row,end,scale,budget,active);if(take(compare_fractions(target.value,after.value,budget))>0)continue;
        auto needed=take(subtract_work_units(target,row.credited_at,budget));auto reached=take(add_world_duration(row.occurred_at,take(scale_units_to_duration(needed,scale.units_per_half_day,budget)),budget));
        if(take(compare_world_moments(reached,end,budget))>0||take(compare_world_moments(reached,current,budget))>0)fail("FutureFixedServiceCreditBoundary");return reached;
    }
    fail("FixedServiceCreditBoundaryMissing");return WorldMoment{};
});}
// 完工采用前可读真实满额due，采用后反读原Terminal；绝不因预计due已到就补信用。
namespace {
Outcome<FixedServiceCompletionRead> read_full_service_recheck(const ActionElapsedStore& store,const ActionElapsedEntryRef& ref,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget,FixedTerminalDisposition expected){return guard<FixedServiceCompletionRead>([&]{
    if(ref.action_ref.empty())fail("InvalidActionElapsedEntryRef");const auto* entry=find_fixed_elapsed(store,ref.action_ref);if(!entry)fail("MissingFixedElapsedAction");
    if(!same_units(entry->required_units,entry->credited_units,budget))fail("IncompleteFixedServiceCompleted");
    const bool due=std::holds_alternative<ElapsedRunning>(entry->timing);
    if(!due&&!std::holds_alternative<ElapsedTerminal>(entry->timing))fail("FixedServiceCompletionPhaseMismatch");
    auto service=take(read_fixed_service_phase_intervals(store,ref.action_ref,entry->started_at,entry->last_settled_at,current,scale,budget,due));
    if(!due){const auto rows=chain(store,ref.action_ref);if(rows.empty()||rows.back()->terminal_disposition!=std::optional<FixedTerminalDisposition>{expected})fail("FixedServiceCompletionDispositionMismatch");}
    return FixedServiceCompletionRead{ref,entry->started_at,entry->last_settled_at,entry->required_units,std::move(service)};
});}
}
Outcome<FixedServiceCompletionRead> read_fixed_service_completion(const ActionElapsedStore& store,const ActionElapsedEntryRef& ref,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return read_full_service_recheck(store,ref,current,scale,budget,FixedTerminalDisposition::completed);}
Outcome<FixedServiceCompletionRead> read_fixed_service_completed_recheck_interruption(const ActionElapsedStore& store,const ActionElapsedEntryRef& ref,const WorldMoment& current,const TimeScaleRef& scale,ExactArithmeticBudget budget){return read_full_service_recheck(store,ref,current,scale,budget,FixedTerminalDisposition::interrupted);}
} // 所有真实资格/费用/占用source仍必须由原owner完整反扫；本叶不建立provider默认值。
