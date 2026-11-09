#include "native_action_time_sources.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
template<class T>Outcome<T> bad(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::internal).error};}
bool same_registration(const ActionTimeRegistration&a,const ActionTimeRegistration&b){return a.action_kind==b.action_kind&&a.owner_domain==b.owner_domain&&a.profile_id==b.profile_id&&a.profile_version==b.profile_version&&a.source_schema==b.source_schema&&a.mode==b.mode&&a.remaining_meaning==b.remaining_meaning&&a.shared_gate_kind==b.shared_gate_kind&&a.retained_wait_phases==b.retained_wait_phases;}
StableId period_domain(WorkPeriodOwner owner){switch(owner){case WorkPeriodOwner::local_economy:return "local_economy";case WorkPeriodOwner::estate:return "estate";case WorkPeriodOwner::organization:return "organization";}return {};}
StableId binding_domain(const WorkExecutionBinding&binding){return std::visit([](const auto&v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,ActorActionWorkBinding>)return v.source_owner_ref.domain;else return period_domain(v.domain);},binding);}
}
const ActionTimeOriginalSourceRead* NativeActionTimeSourceRead::owner(const StableId&domain)const{const ActionTimeOriginalSourceRead*found=nullptr;for(const auto&v:owners_)if(v.owner_domain==domain){if(found)return nullptr;found=v.source.get();}return found;}
Outcome<const ActionTimeOriginalSourceRead*> NativeActionTimeSourceRead::selected(const ActionTimeRegistration&r)const{const ActionTimeRegistration*found=nullptr;for(const auto&v:registrations_)if(v.action_kind==r.action_kind){if(found)return bad<const ActionTimeOriginalSourceRead*>("DuplicateCompiledActionTimeKind",r.action_kind);found=&v;}if(!found||!same_registration(*found,r))return bad<const ActionTimeOriginalSourceRead*>("UnselectedActionTimeRegistration",r.action_kind);const auto*source=owner(r.owner_domain);return source?Outcome<const ActionTimeOriginalSourceRead*>{true,source,{}}:bad<const ActionTimeOriginalSourceRead*>("OriginalActionTimeOwnerRequired",r.owner_domain);}
Status NativeActionTimeSourceRead::validate_owner_bindings()const{auto valid=validate_action_time_registry(registrations_);if(!valid.ok)return valid;for(std::size_t i=0;i<owners_.size();++i){const auto&v=owners_[i];if(v.owner_domain.empty()||!v.source||v.source.get()==this)return common_failure("InvalidCompiledActionTimeOwner",v.owner_domain,ErrorKind::internal);for(std::size_t j=0;j<i;++j)if(v.owner_domain==owners_[j].owner_domain)return common_failure("DuplicateCompiledActionTimeOwner",v.owner_domain,ErrorKind::internal);if(std::none_of(registrations_.begin(),registrations_.end(),[&](const auto&r){return r.owner_domain==v.owner_domain;}))return common_failure("UnselectedCompiledActionTimeOwner",v.owner_domain,ErrorKind::internal);}for(const auto&r:registrations_){auto source=selected(r);if(!source.ok)return {false,{},source.error};}return common_success();}
Status NativeActionTimeSourceRead::enumerate_action_time_owners(const ActionTimeRegistration&r,ActionTimeOwnerIdSink& sink)const{auto source=selected(r);return source.ok?source.value->enumerate_action_time_owners(r,sink):Status{false,{},source.error};}
Outcome<ActionTimeOwnerProof> NativeActionTimeSourceRead::read_action_time_owner(const ActionTimeRegistration&r,const StableId&id)const{auto source=selected(r);return source.ok?source.value->read_action_time_owner(r,id):Outcome<ActionTimeOwnerProof>{false,{},source.error};}
Outcome<WorldMoment> NativeActionTimeSourceRead::read_action_time_source(
    const ElapsedTimeSource &ref) const {
  const auto *actual = std::get_if<MomentOwnerSource>(&ref);
  if (actual && actual->source_ref.domain == "growth_market_history" &&
      actual->source_ref.kind == "growth_market_movement" &&
      actual->source_schema == 2) {
    // 原交易开工以市场真实流水计时，所属 action owner 仍是已选 Growth。
    // 只接这一 typed 来源；先逐栏核原注册，不新增市场 owner 或伪造时间源。
    const ActionTimeRegistration *trade = nullptr;
    for (const auto &registration : registrations_)
      if (registration.action_kind == "trade") {
        if (trade)
          return bad<WorldMoment>("DuplicateCompiledActionTimeKind", "trade");
        trade = &registration;
      }
    if (!trade)
      return bad<WorldMoment>("UnselectedActionTimeRegistration", "trade");
    auto original =
        make_growth_action_time_registrations(trade->profile_version);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    const auto expected =
        std::find_if(original.value.begin(), original.value.end(),
                     [](const auto &registration) {
                       return registration.action_kind == "trade";
                     });
    if (expected == original.value.end() ||
        !same_registration(*trade, *expected))
      return bad<WorldMoment>("UnselectedActionTimeRegistration", "trade");
    auto source = selected(*trade);
    if (!source.ok)
      return {false, {}, std::move(source.error)};
    return source.value->read_action_time_source(ref);
  }
  const auto &domain = std::visit(
      [](const auto &value) -> const StableId & {
        return value.source_ref.domain;
      },
      ref);
  const auto *source = owner(domain);
  return source ? source->read_action_time_source(ref)
                : bad<WorldMoment>("OriginalActionTimeSourceOwnerRequired",
                                   domain);
}
Status NativeActionTimeSourceRead::validate_fixed_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&e,const std::vector<FixedServiceTransition>&h)const{auto source=selected(r);return source.ok?source.value->validate_fixed_action_sources(r,p,e,h):Status{false,{},source.error};}
Status NativeActionTimeSourceRead::validate_fixed_action_sources_in_candidate(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const ActionElapsedState&e,const std::vector<FixedServiceTransition>&h,const ActionElapsedStore& actual)const{auto source=selected(r);return source.ok?source.value->validate_fixed_action_sources_in_candidate(r,p,e,h,actual):Status{false,{},source.error};}
Status NativeActionTimeSourceRead::validate_shared_action_sources(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const SharedWorkElapsedState&e)const{auto source=selected(r);return source.ok?source.value->validate_shared_action_sources(r,p,e):Status{false,{},source.error};}
Outcome<RunStop> NativeActionTimeSourceRead::read_action_continuation_stop(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const{auto source=selected(r);return source.ok?source.value->read_action_continuation_stop(r,p,at):Outcome<RunStop>{false,{},source.error};}
Outcome<OriginalActionRemaining> NativeActionTimeSourceRead::read_original_action_remaining(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const{auto source=selected(r);return source.ok?source.value->read_original_action_remaining(r,p,at):Outcome<OriginalActionRemaining>{false,{},source.error};}
Outcome<ExhaustedAuthorizationWindow> NativeActionTimeSourceRead::read_exhausted_authorization_window(const ActionTimeRegistration&r,const ActionTimeOwnerProof&p,const WorldMoment&at)const{auto source=selected(r);return source.ok?source.value->read_exhausted_authorization_window(r,p,at):Outcome<ExhaustedAuthorizationWindow>{false,{},source.error};}
Outcome<const ActionTimeOriginalSourceRead*> NativeActionTimeSourceRead::gate_owner(const SharedWorkGateResult&gate)const{const auto domain=std::visit([](const auto&v)->StableId{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,LifeHalfDayStageGateResult>)return v.work_period_ref.source_ref.domain;else return binding_domain(v.execution_binding);},gate);const auto*source=owner(domain);return source?Outcome<const ActionTimeOriginalSourceRead*>{true,source,{}}:bad<const ActionTimeOriginalSourceRead*>("OriginalSharedGateOwnerRequired",domain);}
Outcome<StableId> NativeActionTimeSourceRead::read_gate_result_id(const SharedWorkGateResult&gate)const{auto source=gate_owner(gate);return source.ok?source.value->read_gate_result_id(gate):Outcome<StableId>{false,{},source.error};}
Outcome<SharedWorkGateResult> NativeActionTimeSourceRead::read_original_gate_result(const StableId&id)const{
 // Re-read original executions each time: writers may have appended a gate since this lease was created.
 // Neither ActionElapsed.gate_results nor a caller result payload is the coverage authority.
 const ActionTimeOriginalSourceRead*found=nullptr;
 class GateOwnerSink final:public ActionTimeOwnerIdSink {
  const ActionTimeOriginalSourceRead& source_;const ActionTimeRegistration& registration_;const StableId& result_;const ActionTimeOriginalSourceRead*& found_;
 public:GateOwnerSink(const ActionTimeOriginalSourceRead& source,const ActionTimeRegistration& registration,const StableId& result,const ActionTimeOriginalSourceRead*& found):source_(source),registration_(registration),result_(result),found_(found){}
  Status append(std::string_view action)override{auto proof=source_.read_action_time_owner_borrowed(registration_,action);if(!proof.ok)return {false,{},proof.error};for(const auto& actual:proof.value.retained_gate_result_refs)if(actual==result_){if(found_)return common_failure("SharedGateHasMultipleOriginalOwners",result_,ErrorKind::internal);found_=&source_;}return common_success();}
 };
 for(const auto&r:registrations_){if(r.mode!=ActionTimeMode::shared_work)continue;auto source=selected(r);if(!source.ok)return {false,{},source.error};GateOwnerSink sink(*source.value,r,id,found);auto status=source.value->enumerate_action_time_owners(r,sink);if(!status.ok)return {false,{},status.error};}
 if(!found)return bad<SharedWorkGateResult>("OriginalSharedGateResultMissing",id);auto result=found->read_original_gate_result(id);if(!result.ok)return result;auto actual=found->read_gate_result_id(result.value);if(!actual.ok)return {false,{},actual.error};if(actual.value!=id)return bad<SharedWorkGateResult>("OriginalSharedGateIdentityMismatch",id);auto routed=gate_owner(result.value);if(!routed.ok)return {false,{},routed.error};if(routed.value!=found)return bad<SharedWorkGateResult>("OriginalSharedGateDomainMismatch",id);return result;
}
Status NativeActionTimeSourceRead::validate_original_gate_result(const SharedWorkGateResult&gate)const{auto source=gate_owner(gate);if(!source.ok)return {false,{},source.error};return source.value->validate_original_gate_result(gate);}
const LifeWorkPeriodSourceRead* NativeActionTimeSourceRead::life_work_period_sources(const LifeWorkPeriodSourceRef&source)const noexcept{const auto*original=owner(source.source_ref.domain);return original?original->life_work_period_sources(source):nullptr;}
}
