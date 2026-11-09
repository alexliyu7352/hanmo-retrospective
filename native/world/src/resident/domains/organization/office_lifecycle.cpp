// 实际来源成功后同候选落任期事实；机构status和资产/继承由各owner独立办理。
#include "office_lifecycle.hpp"
#include "organization_internal.hpp"
#include "organization_service_effects_codec.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool source_equal(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool death_equal(const DeathRef& a,const DeathRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool same_moment(const WorldMoment& a,const WorldMoment& b){auto order=compare_world_moments(a,b);return order.ok&&order.value==0;}
Status source_header(const CommonStore& c,const SourceRef& ref){const auto* h=c.state().facts.find(ref.id);return h&&h->id==ref.id&&h->domain==ref.domain&&h->kind==ref.kind?common_success():common_failure("OfficeChangeSourceMissing",ref.id);}
Status validate_retained_office_service_effects(const OrganizationCatalog& catalog,const StableId& fact_id,const OrganizationFact& fact){
    if(!catalog.service_effects_enabled)return common_success();
    if(fact.id!=fact_id)return common_failure("OrganizationServiceEffectsParentMismatch",fact_id);
    if(!fact.service_effects)return common_failure("OrganizationServiceEffectsPayloadRequired",fact_id);
    return validate_organization_service_effects_parent(fact,*fact.service_effects);
}
// 本私有writer不选择新持有人，不自动升副手；输入只能来自下方真实source重验。
Outcome<OfficeChangeResult> append_change(CommonStore& c,OrganizationState& state,const StableId& id,const OrganizationOfficeChangedPayload& payload,const SourceRef& source,bool exact,const OrganizationCatalog& catalog){
    auto* office=state.offices.find(id);
    if(!office||office->epoch!=payload.before_epoch||office->holder_actor_id!=payload.old_holder||office->epoch==std::numeric_limits<Integer>::max()||payload.after_epoch!=payload.before_epoch+1)return {false,{},common_failure("StaleOfficeEpoch",id).error};
    auto header=source_header(c,source);
    if(!header.ok)return {false,{},header.error};
    auto number=c.next_id("organization");
    if(!number.ok)return {false,{},number.error};
    OrganizationFact fact;
    fact.id=number.value;
    fact.kind="organization_office_changed";
    fact.organization_id=office->organization_id;
    fact.actor_id=payload.new_holder.value_or(StableId{});
    fact.office_id=id;
    fact.tick=payload.effective_moment.half_day_tick;
    if(exact)fact.occurred_at=payload.effective_moment;
    fact.cause_fact_ids={source.id};
    fact.office_change_payload=payload;
    auto appended=c.append_fact({number.value,fact.kind,"organization"});
    if(!appended.ok)return {false,{},appended.error};
    if(!state.facts.insert(number.value,std::move(fact)))return {false,{},common_failure("DuplicateOfficeChangeFact",number.value).error};
    office->holder_actor_id=payload.new_holder;
    office->epoch=payload.after_epoch;
    office->status=payload.new_holder?OfficeStatus::active:OfficeStatus::vacant;
    OfficeChangeResult out;
    out.changed=true;
    out.fact_id=number.value;
    OrganizationServiceEffectsBuildView captured;
    // 旧职责未来资格停止，真实未清债务/活动引用原样保留；任期变更不任意清空身体占用。
    if(payload.old_holder)for(auto& row:state.duties.rows()){
        auto& duty=row.second;
        if(!duty.active||duty.office_id!=id||duty.actor_id!=*payload.old_holder)continue;
        const bool duty_active_before=duty.active;
        std::optional<OrganizationRoleEnabledChange> role_enabled_change;
        duty.active=false;
        out.stopped_duty_ids.push_back(row.first);
        if(!duty.role_id.empty()){
            const auto* original=c.state().roles.find(duty.role_id);
            if(!original||original->actor_id!=duty.actor_id)return {false,{},common_failure("OfficeDutyRoleMismatch",duty.role_id).error};
            if(original->enabled){
                const bool role_enabled_before=original->enabled;
                auto role=*original;
                role.enabled=false;
                auto applied=c.apply_role(duty.role_id,role);
                if(!applied.ok)return {false,{},applied.error};
                out.disabled_role_ids.push_back(duty.role_id);
                if(catalog.service_effects_enabled)role_enabled_change=OrganizationRoleEnabledChange{role_enabled_before,false};
            }
        }
        if(catalog.service_effects_enabled)captured.changes.push_back({duty.id,duty.role_id,duty.actor_id,duty_active_before,duty.active,role_enabled_change});
    }
    if(catalog.service_effects_enabled){
        auto attached=organization_detail::attach_organization_service_effects_in_candidate(state,catalog,number.value,captured);
        if(!attached.ok)return {false,{},attached.error};
    }
    return {true,std::move(out),{}};
}
Status transition_shape(const CommonStore& c,const VerifiedOfficeTransition& t){if(t.organization_id.empty()||t.office_id.empty()||t.expected_epoch<0||t.expected_epoch==std::numeric_limits<Integer>::max()||t.old_holder==t.new_holder)return common_failure("InvalidOfficeTransitionSource");auto time=encode_world_moment(t.effective_moment);if(!time.ok)return {false,{},time.error};switch(t.kind){case OfficeTransitionKind::install:if(t.old_holder||!t.new_holder)return common_failure("InvalidOfficeInstallSource");break;case OfficeTransitionKind::dismiss:case OfficeTransitionKind::resign:if(!t.old_holder||t.new_holder)return common_failure("InvalidOfficeReleaseSource");break;case OfficeTransitionKind::term_handover:case OfficeTransitionKind::accession:if(!t.new_holder)return common_failure("InvalidOfficeHandoverSource");break;default:return common_failure("UnknownOfficeTransitionKind");}if(t.new_holder){const auto* actor=c.actor(*t.new_holder);if(!actor||!actor->alive)return common_failure("OfficeNewHolderNotLiving",*t.new_holder);}return source_header(c,t.source);}
}
Outcome<OfficeChangeResult> consume_office_transition(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const SourceRef& ref,const ReadStamp& stamp,const OfficeTransitionSourceRead& reader){if(!catalog.authority_lifecycle_enabled)return {false,{},common_failure("OfficeLifecycleCapabilityRequired").error};auto source=reader.read_office_transition(ref,stamp);if(!source.ok)return {false,{},source.error};const auto& t=source.value;if(!source_equal(t.source,ref))return {false,{},common_failure("OfficeTransitionSourceMismatch").error};auto valid=transition_shape(c,t);if(!valid.ok)return {false,{},valid.error};for(const auto& row:s.facts.rows())if(row.second.office_id==t.office_id&&row.second.office_change_payload){const auto& payload=*row.second.office_change_payload;if(const auto* cause=std::get_if<ByOfficeTransition>(&payload.cause);cause&&source_equal(cause->transition_source_ref,ref)){if(payload.before_epoch!=t.expected_epoch||payload.old_holder!=t.old_holder||payload.new_holder!=t.new_holder||!same_moment(payload.effective_moment,t.effective_moment))return {false,{},common_failure("ConflictingOfficeTransitionSource").error};auto retained_effects=validate_retained_office_service_effects(catalog,row.first,row.second);if(!retained_effects.ok)return {false,{},retained_effects.error};return {true,{false,row.first,{},{}},{}};}}const auto* office=s.offices.find(t.office_id);if(!office||office->organization_id!=t.organization_id)return {false,{},common_failure("OfficeTransitionOrganizationMismatch").error};OrganizationOfficeChangedPayload payload;payload.before_epoch=t.expected_epoch;payload.after_epoch=t.expected_epoch+1;payload.old_holder=t.old_holder;payload.new_holder=t.new_holder;payload.effective_moment=t.effective_moment;payload.cause=ByOfficeTransition{ref};return append_change(c,s,t.office_id,payload,ref,catalog.exact_fact_clock_enabled,catalog);}
Outcome<OfficeChangeResult> consume_verified_office_death(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const StableId& id,Integer epoch,const DeathRef& ref,const ReadStamp& stamp,const VerifiedDeathRead& reader){if(!catalog.authority_lifecycle_enabled)return {false,{},common_failure("OfficeLifecycleCapabilityRequired").error};auto death=reader.by_ref(ref,stamp);if(!death.ok)return {false,{},death.error};const auto& d=death.value;if(!death_equal(d.death_ref,ref)||d.world_id!=c.state().identity.world_id||d.scenario_id!=c.state().metadata.scenario_id||d.effective_at.kind!=ActorFoundationAt::Kind::exact_moment)return {false,{},common_failure("ExactOfficeDeathSourceRequired").error};for(const auto& row:s.facts.rows())if(row.second.office_id==id&&row.second.office_change_payload){const auto& p=*row.second.office_change_payload;if(const auto* cause=std::get_if<VerifiedHolderDeath>(&p.cause);cause&&death_equal(cause->death_ref,ref)&&p.before_epoch==epoch){if(p.old_holder!=std::optional<StableId>{d.actor_id}||p.new_holder||!same_moment(p.effective_moment,d.effective_at.exact_moment))return {false,{},common_failure("ConflictingOfficeDeathSource").error};auto retained_effects=validate_retained_office_service_effects(catalog,row.first,row.second);if(!retained_effects.ok)return {false,{},retained_effects.error};return {true,{false,row.first,{},{}},{}};}}const auto* office=s.offices.find(id);if(!office)return {false,{},common_failure("UnknownOffice",id).error};if(office->holder_actor_id!=std::optional<StableId>{d.actor_id})return {true,{}, {}};if(office->epoch!=epoch||epoch==std::numeric_limits<Integer>::max())return {false,{},common_failure("StaleOfficeEpoch",id).error};OrganizationOfficeChangedPayload p;p.before_epoch=epoch;p.after_epoch=epoch+1;p.old_holder=d.actor_id;p.effective_moment=d.effective_at.exact_moment;p.cause=VerifiedHolderDeath{ref};return append_change(c,s,id,p,{ref.domain,ref.kind,ref.id},catalog.exact_fact_clock_enabled,catalog);}
// 重放任期来源链并核对当前职位；历史死亡与交接来源不符时保留原失败。
Status validate_office_lifecycle(const CommonStore &c,
                                 const OrganizationState &s,
                                 const OrganizationCatalog &catalog,
                                 const ReadStamp &stamp,
                                 const OfficeTransitionSourceRead &transitions,
                                 const VerifiedDeathRead &deaths) {
  if (!catalog.authority_lifecycle_enabled) {
    for (const auto &row : s.facts.rows())
      if (row.second.office_change_payload || row.second.grant_change_payload)
        return common_failure("UnexpectedOfficeLifecyclePayload");
    return common_success();
  }
  auto derived = catalog.offices;
  std::set<StableId> covered;
  for (const auto &id : c.state().facts.order()) {
    const auto *f = s.facts.find(id);
    if (!f)
      continue;
    if (f->kind != "organization_office_changed") {
      if (f->office_change_payload)
        return common_failure("UnexpectedOfficeChangePayload", id);
      continue;
    }
    if (!f->office_change_payload || f->grant_change_payload)
      return common_failure("OfficeChangePayloadRequired", id);
    const auto &p = *f->office_change_payload;
    if (catalog.exact_fact_clock_enabled &&
        (!f->occurred_at || !same_moment(*f->occurred_at, p.effective_moment)))
      return common_failure("OfficeFactExactMomentMismatch", id);
    auto *office = derived.find(f->office_id);
    const auto *header = c.state().facts.find(id);
    if (!office || !header || header->kind != f->kind ||
        header->domain != "organization" || f->id != id ||
        p.schema_version != 2 || p.before_epoch != office->epoch ||
        p.before_epoch == std::numeric_limits<Integer>::max() ||
        p.after_epoch != p.before_epoch + 1 ||
        p.old_holder != office->holder_actor_id ||
        p.new_holder == p.old_holder ||
        f->actor_id != p.new_holder.value_or(StableId{}) ||
        f->tick != p.effective_moment.half_day_tick ||
        f->organization_id != office->organization_id ||
        !covered.insert(id).second)
      return common_failure("OfficeLifecycleChainMismatch", id);
    StableId cause_id;
    if (const auto *cause = std::get_if<ByOfficeTransition>(&p.cause)) {
      auto source = transitions.read_office_transition(
          cause->transition_source_ref, stamp);
      if (!source.ok)
        return {false, {}, source.error};
      const auto &t = source.value;
      if (t.office_id != f->office_id ||
          t.organization_id != f->organization_id ||
          t.expected_epoch != p.before_epoch || t.old_holder != p.old_holder ||
          t.new_holder != p.new_holder ||
          !same_moment(t.effective_moment, p.effective_moment))
        return common_failure("OfficeTransitionHistoryMismatch", id);
      cause_id = t.source.id;
    } else {
      const auto &death_cause = std::get<VerifiedHolderDeath>(p.cause);
      auto source = deaths.by_ref(death_cause.death_ref, stamp);
      if (!source.ok)
        return {false, {}, source.error};
      const auto &d = source.value;
      if (d.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
          p.new_holder || p.old_holder != std::optional<StableId>{d.actor_id} ||
          !same_moment(d.effective_at.exact_moment, p.effective_moment))
        return common_failure("OfficeDeathHistoryMismatch", id);
      cause_id = d.death_ref.id;
    }
    const auto &order = c.state().facts.order();
    if (f->cause_fact_ids != std::vector<StableId>{cause_id} ||
        std::find(order.begin(), order.end(), cause_id) >=
            std::find(order.begin(), order.end(), id))
      return common_failure("OfficeLifecycleCauseOrderMismatch", id);
    office->epoch = p.after_epoch;
    office->holder_actor_id = p.new_holder;
    office->status = p.new_holder ? OfficeStatus::active : OfficeStatus::vacant;
  }
  for (const auto &row : derived.rows()) {
    const auto *current = s.offices.find(row.first);
    if (!current || current->epoch != row.second.epoch ||
        current->holder_actor_id != row.second.holder_actor_id ||
        current->status != row.second.status)
      return common_failure("OfficeLifecycleCurrentMismatch", row.first);
  }
  for (const auto &row : s.facts.rows())
    if (row.second.kind == "organization_office_changed" &&
        !covered.count(row.first))
      return common_failure("OrphanOfficeLifecycleFact", row.first);
  return common_success();
}
}
