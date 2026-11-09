#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "work_supervision_execution_terms_fields.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "work_execution_detail.hpp"
namespace hanmo::resident {
Outcome<SupervisionExecutionTerms> decode_supervision_execution_terms(const BoundaryValue&value,Integer selected_version){SupervisionExecutionTerms out;dynamic_codec::JsonArchive a(value);auto s=supervision_execution_codec::fields(a,out,selected_version);if(s.ok)s=a.finish();return s.ok?Outcome<SupervisionExecutionTerms>{true,std::move(out),{}}:Outcome<SupervisionExecutionTerms>{false,{},s.error};}
Outcome<BoundaryValue> encode_supervision_execution_terms(const SupervisionExecutionTerms&v,Integer selected_version){return dynamic_projection::encode(v,[&](auto&a,auto&x){return supervision_execution_codec::fields(a,x,selected_version);});}
Status visit_supervision_execution_terms_checkpoint(CodecVisitor&a,SupervisionExecutionTerms&v,Integer selected_version){return supervision_execution_codec::fields(a,v,selected_version);}
Status emit_supervision_execution_terms_fields(ProjectionWriter&w,const SupervisionExecutionTerms&v,Integer selected_version){DynamicProjectionArchive a(w);return supervision_execution_codec::fields(a,const_cast<SupervisionExecutionTerms&>(v),selected_version);}
namespace {
Outcome<const SupervisionExecutionTerms*> action_terms(const CommonStore&common,
 const TaskWorkExecutionState*state,const ActionCoreView&action){
 auto fail=[&](const char*code,ErrorKind kind=ErrorKind::internal)->Outcome<const SupervisionExecutionTerms*>{return {false,{},common_failure(code,action.id,kind).error};};
 if(action.kind!="work_supervise")return fail("SupervisionActionKindMismatch");
 if(!state||!task_work_fixed_execution_terms_version(state->version))return fail("SupervisionOriginalFixedExecutionTermsRequired",ErrorKind::unsupported_capability);
 const auto*actual=common.action(action.id);const auto*terms=state->supervision_execution_terms.find(action.id);
 if(!actual||actual->kind!=action.kind||actual->actor_id!=action.actor_id||actual->participants!=action.participants||actual->start_tick!=action.start_tick||actual->remaining_ticks!=action.remaining_ticks||!terms||terms->action_id!=action.id||terms->actual_terminal_result_ref||!terms->actual_start_source_ref)return fail("SupervisionActionTermsMismatch");
 auto shape=validate_supervision_execution_terms_shape(*terms);if(!shape.ok)return {false,{},shape.error};
 const auto&ref=terms->actual_start_source_ref->source_ref;const auto*start=state->supervision_starts.find(ref.id);
 const auto*head=common.state().facts.find(ref.id);const auto*work=state->episodes.find(terms->work_id);
 if(!start||start->fact_id!=ref.id||start->action_id!=action.id||start->work_id!=terms->work_id||start->actor_id!=action.actor_id||!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind||!work||work->work_id!=terms->work_id||!work_detail::same(work->authorization,terms->original_authorization_ref)||!std::holds_alternative<SupervisedWorkPlan>(work->frozen_plan)||std::get<SupervisedWorkPlan>(work->frozen_plan).kind!=terms->kind)return fail("SupervisionActionOriginalStartMismatch");
 auto time=compare_world_moments(start->started_at,start->started_at);if(!time.ok)return {false,{},time.error};
 if(action.start_tick!=start->started_at.half_day_tick||action.remaining_ticks<=0||action.participants.size()!=1||action.participants.front()!=start->actor_id||!work->current_execution||!std::holds_alternative<WorkCommonActionBinding>(*work->current_execution)||std::get<WorkCommonActionBinding>(*work->current_execution).action_id!=action.id)return fail("SupervisionActionCurrentBindingMismatch");
 const auto*actor=common.actor(action.actor_id);if(!actor||actor->action_id!=action.id)return fail("SupervisionActionOccupancyMismatch");
 const auto*accepted=state->acceptances.find(work->acceptance_source_ref);
 if(!accepted||accepted->fact_id!=work->acceptance_source_ref||accepted->work_id!=work->work_id||!work_detail::same(start->work_acceptance_ref,{"tasks","work_accepted",accepted->fact_id}))return fail("SupervisionActionAcceptanceMismatch");
 std::size_t count=0;for(const auto&r:state->supervision_starts.rows())if(r.second.action_id==action.id){if(r.first!=start->fact_id)return fail("DuplicateSupervisionActionStart");++count;}
 if(count!=1)return fail("SupervisionActionStartCoverageMismatch");
 for(const auto&r:state->supervision_window_closures.rows())if(r.second.start_source_ref==start->fact_id)return fail("SupervisionClosedActionStillPresent");
 for(const auto&r:state->supervision_terminals.rows())if(r.second.start_source_ref==start->fact_id)return fail("SupervisionEndedActionStillPresent");
 return {true,terms,{}};
}
// 借用原字段表逐项比较参数；不解码、不构造第二份参数或 BoundaryValue 树。
class ParamsMatchArchive {
  const BoundaryValue::Dict *fields_ = nullptr;
  std::size_t next_ = 0;
  Status result_ = common_success();
  // 只接受原字段顺序；缺项或名称不符时保留首次失败。
  const BoundaryValue *next(std::string_view name) {
    if (!result_.ok)
      return nullptr;
    if (!fields_ || next_ >= fields_->size() ||
        (*fields_)[next_].first != name) {
      reject("SupervisionActionParamsMismatch");
      return nullptr;
    }
    return &(*fields_)[next_++].second;
  }

public:
  // 借用待比较的字典；非字典输入直接拒绝。
  explicit ParamsMatchArchive(const BoundaryValue &v)
      : fields_(v.as<BoundaryValue::Dict>()) {
    if (!fields_)
      reject("SupervisionActionParamsMismatch");
  }
  // 比较器属于只写侧，字段访问不得走读取或默认构造分支。
  bool reading() const noexcept {
    return false;
  }
  // 返回累计状态，不清除先前的失败。
  Status status() const {
    return result_;
  }
  // 遵循只写 archive 合同：不构造 T、不分配默认值，保留已有状态。
  template <class T> Status prepare_default_values(std::uint64_t = 1) {
    return result_;
  }
  // 将借用错误标识转为现有错误记录，只记录首次失败。
  Status reject(std::string_view code, std::string_view path = {}) {
    if (result_.ok)
      result_ =
          common_failure(StableId(code), StableId(path), ErrorKind::internal);
    return result_;
  }
  // 只核对已有标量类型和值，不修改原参数。
  template <class T> Status field(std::string_view name, const T &value) {
    const auto *v = next(name);
    if (v) {
      const auto *p = v->as<T>();
      if (!p || *p != value)
        reject("SupervisionActionParamsMismatch");
    }
    return result_;
  }
  // 对嵌套对象复用同一比较规则，并拒绝遗漏或多余字段。
  template <class F> Status object(std::string_view name, F emit) {
    const auto *v = next(name);
    if (!v)
      return result_;
    ParamsMatchArchive child(*v);
    auto s = emit(child);
    if (s.ok)
      s = child.finish();
    if (!s.ok)
      result_ = std::move(s);
    return result_;
  }
  // 按原 present 布局比较可选值，不创建或清除载荷。
  template <class T, class F>
  Status optional(std::string_view name, std::optional<T> &v, F emit) {
    return object(name, [&](auto &c) {
      const bool present = v.has_value();
      c.field("present", present);
      if (present && c.status().ok)
        return emit(c, *v);
      return c.status();
    });
  }
  // 原字段表访问完毕后拒绝额外参数。
  Status finish() {
    if (result_.ok && (!fields_ || next_ != fields_->size()))
      reject("SupervisionActionParamsMismatch");
    return result_;
  }
};
}
Outcome<bool> emit_task_work_original_action_payload(ProjectionWriter&out,const CommonStore&common,const TaskWorkExecutionState*state,const ActionCoreView&action){
 if(action.kind!="work_supervise")return {true,false,{}};auto row=action_terms(common,state,action);if(!row.ok)return {false,{},row.error};auto s=emit_supervision_execution_terms_fields(out,*row.value,state->version);return {s.ok,s.ok,s.error};
}
Outcome<bool> validate_task_work_original_action_payload(const CommonStore&common,const TaskWorkExecutionState*state,const ActionCoreView&action,const BoundaryValue&params){
 if(action.kind!="work_supervise")return {true,false,{}};auto row=action_terms(common,state,action);if(!row.ok)return {false,{},row.error};ParamsMatchArchive a(params);auto s=supervision_execution_codec::fields(a,const_cast<SupervisionExecutionTerms&>(*row.value),state->version);if(s.ok)s=a.finish();return {s.ok,s.ok,s.error};
}
Status validate_task_work_original_action_headers(const CommonStore&common,const TaskWorkExecutionState*state){
 for(const auto&r:common.state().actions.rows())if(r.second.kind=="work_supervise"){if(r.first!=r.second.id)return common_failure("SupervisionActionKeyMismatch",r.first,ErrorKind::internal);auto row=action_terms(common,state,r.second);if(!row.ok)return {false,{},row.error};}
 if(!state)return common_success();if(!task_work_fixed_execution_terms_version(state->version))return state->supervision_execution_terms.empty()?common_success():common_failure("SupervisionExecutionTermsRequirePhysical5",{},ErrorKind::unsupported_capability);
 for(const auto&r:state->supervision_execution_terms.rows()){
  const auto&v=r.second;if(r.first!=v.action_id)return common_failure("SupervisionExecutionTermsKeyMismatch",r.first,ErrorKind::internal);
  const auto*a=common.action(v.action_id);if(v.actual_terminal_result_ref){if(a)return common_failure("SupervisionEndedActionStillPresent",v.action_id,ErrorKind::internal);}
  else{if(!a)return common_failure("SupervisionActiveActionMissing",v.action_id,ErrorKind::internal);auto row=action_terms(common,state,*a);if(!row.ok)return {false,{},row.error};}
 }
 return common_success();
}

}
