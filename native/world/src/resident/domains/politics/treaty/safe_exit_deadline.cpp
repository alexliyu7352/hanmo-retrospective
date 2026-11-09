#include "safe_exit_deadline.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
// 期限只从已存在原activation/termination/crossing读，不因漏处理重开窗口。
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void take(Status value){if(!value.ok)throw Failure{std::move(value.error)};}
int compare(const SafeExitEntryContext& x,const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b,x.arithmetic));}
std::size_t position(const CommonStore& common,const SourceRef& ref){const auto* head=common.state().facts.find(ref.id);const auto& order=common.state().facts.order();const auto at=std::find(order.begin(),order.end(),ref.id);if(!head||head->domain!=ref.domain||head->kind!=ref.kind||at==order.end())fail("SafeExitDeadlineOriginalSourceRequired",ref.id);return static_cast<std::size_t>(at-order.begin());}
struct OriginalDeadline {const SafeExitBinding* binding=nullptr;SourceRef termination_basis;WorldMoment termination_at;std::optional<SafeExitWindowView> window;std::optional<SourceRef> first_exit;bool active=false;};
OriginalDeadline original(const SafeExitEntryContext& x,const SourceRef& source){
 auto entry=take(read_safe_exit_original_entry(x,source,1));const auto& binding=*entry.binding;
 take(x.originals.validate_safe_exit_retained_progress(binding,x.state.lines));
 OriginalDeadline result;result.binding=&binding;result.termination_basis=treaty_activation_source(*entry.activation.source);
 result.termination_at={std::min(entry.term->window.until_tick,entry.activation.revision.definition->terms.effective_window.until_tick),{}};
 // 此处原条款自然终界可在未来；实际提前终止必须已有真实源，不能预测未来决定。
 for(const auto& row:x.treaties.terminations.rows())if(same_source_ref(row.second.activation_ref,treaty_activation_source(*entry.activation.source))){
  const auto ended=take(read_original_treaty_termination(x.treaties,treaty_termination_source(row.second),1));
  if(compare(x,ended.source->terminated_at,result.termination_at)<0){result.termination_at=ended.source->terminated_at;result.termination_basis=treaty_termination_source(*ended.source);}
 }
 position(x.common,result.termination_basis);
 const BoundaryCrossingReceipt* first=nullptr;
 for(const auto& id:binding.exit_line_refs){const auto* line=x.state.lines.find(id);if(!line||line->binding_id!=binding.id)fail("SafeExitDeadlineOriginalLineRequired",id);
  result.active|=line->state==SafeExitLineState::inside||line->state==SafeExitLineState::exiting;
  for(const auto& ref:line->actual_crossing_refs){const auto* crossing=x.carriers.crossings.find(ref.id);
   if(ref.domain!="travel"||ref.kind!="boundary_crossing"||!crossing||crossing->receipt_id!=ref.id)fail("SafeExitDeadlineOriginalCrossingRequired",ref.id);
   const auto cut=position(x.common,ref);if(!first||compare(x,crossing->entry_at,first->entry_at)<0||
    (compare(x,crossing->entry_at,first->entry_at)==0&&cut<position(x.common,{"travel","boundary_crossing",first->receipt_id})))first=crossing;
  }
 }
 if(first)result.first_exit=SourceRef{"travel","boundary_crossing",first->receipt_id};
 result.window=take(read_safe_exit_window_sources(x,source));
 if(result.window){if(compare(x,result.window->termination_at,result.termination_at)||!same_source_ref(result.window->termination_source,result.termination_basis)||bool(first)!=bool(result.window->first_actual_exit_entry)||(first&&compare(x,first->entry_at,*result.window->first_actual_exit_entry)))fail("SafeExitDeadlineOriginalWindowMismatch",binding.id);}
 return result;
}
void projected(const SafeExitEntryContext& x,const OriginalDeadline& source){const auto& binding=*source.binding;
 if(!source.window||!binding.termination_source||!binding.termination_at||!binding.exit_from||!binding.exit_until||
  !same_source_ref(*binding.termination_source,source.termination_basis)||compare(x,*binding.termination_at,source.window->termination_at)||
  compare(x,*binding.exit_from,source.window->from)||compare(x,*binding.exit_until,source.window->until))fail("SafeExitDeadlineProjectionMismatch",binding.id);
}
bool same_optional(const std::optional<SourceRef>& a,const std::optional<SourceRef>& b){return bool(a)==bool(b)&&(!a||same_source_ref(*a,*b));}
}
Outcome<std::vector<SafeExitDeadline>> read_safe_exit_deadlines(const SafeExitEntryContext& x){try{
 const auto now=take(x.clock.read_organization_authority_now());std::vector<SafeExitDeadline> result;
 for(const auto& id:x.state.bindings.order()){
  const SourceRef ref{"politics","safe_exit_bound",id};auto source=original(x,ref);const auto& binding=*source.binding;
  if(!binding.termination_at){
   if(binding.termination_source||binding.exit_from||binding.exit_until)fail("SafeExitDeadlinePartialProjection",id);
   if(compare(x,source.termination_at,now)<0)fail("SafeExitTerminationDeadlineMissed",id);
   result.push_back({ref,SafeExitDeadlineKind::install_termination_window,source.termination_at,source.termination_basis,{}});continue;
  }
  projected(x,source);
  if(source.active){const auto elapsed=compare(x,source.window->until,now);if(elapsed<0)fail("SafeExitExitDeadlineMissed",id);
   result.push_back({ref,SafeExitDeadlineKind::expire_exit_window,source.window->until,source.termination_basis,source.first_exit});
   // 等于当前尚待唯一到期writer；不能提前要求本次未发生的expired投影。
   if(elapsed==0)continue;
  }
  take(validate_safe_exit_window_projection(x,ref));
 }
 return {true,std::move(result),{}};
 }catch(const Failure& failure){return {false,{},failure.error};}}
Status validate_safe_exit_deadline_applied(const SafeExitEntryContext& x,const SourceRef& ref,SafeExitDeadlineKind kind,const WorldMoment& at){try{
 const auto now=take(x.clock.read_organization_authority_now());if(compare(x,now,at))fail("SafeExitDeadlineNotAtActualMoment",ref.id);
 auto source=original(x,ref);projected(x,source);
 if(kind==SafeExitDeadlineKind::install_termination_window){if(compare(x,source.termination_at,at))fail("SafeExitTerminationDeadlineChanged",ref.id);}
 else if(kind==SafeExitDeadlineKind::expire_exit_window){if(!source.window||compare(x,source.window->until,at))fail("SafeExitExitDeadlineChanged",ref.id);}
 else fail("SafeExitDeadlineKindUnsupported",ref.id);
 take(validate_safe_exit_window_projection(x,ref));return common_success();
 }catch(const Failure& failure){return {false,{},failure.error};}}
Status apply_safe_exit_deadline_in_candidate(CommonStore& common,SafeExitState& state,
 const SafeExitEntryContext& x,const SafeExitDeadline& predicted){try{
 if(&common!=&x.common||&state!=&x.state)fail("SafeExitDeadlineCandidateMismatch",predicted.binding_source.id);
 const auto now=take(x.clock.read_organization_authority_now());if(compare(x,now,predicted.at))fail("SafeExitDeadlineNotAtActualMoment",predicted.binding_source.id);
 auto source=original(x,predicted.binding_source);
 if(!same_source_ref(predicted.original_termination_basis,source.termination_basis))fail("SafeExitDeadlineOriginalBasisChanged",predicted.binding_source.id);
 if(predicted.kind==SafeExitDeadlineKind::install_termination_window){
  if(compare(x,predicted.at,source.termination_at)||predicted.first_actual_exit_entry||!source.window)fail("SafeExitTerminationDeadlineChanged",predicted.binding_source.id);
 }else if(predicted.kind==SafeExitDeadlineKind::expire_exit_window){
  if(!source.window||compare(x,predicted.at,source.window->until)||!same_optional(predicted.first_actual_exit_entry,source.first_exit))fail("SafeExitExitDeadlineChanged",predicted.binding_source.id);
  projected(x,source);
 }else fail("SafeExitDeadlineKindUnsupported",predicted.binding_source.id);
 // 无ID、无Fact、无费用；重复同原界只反验/投影同一组原派生字段。
 take(project_safe_exit_window_sources_in_candidate(common,state,x,predicted.binding_source));
 take(validate_safe_exit_window_projection(x,predicted.binding_source));return common_success();
 }catch(const Failure& failure){return {false,{},failure.error};}}
}
