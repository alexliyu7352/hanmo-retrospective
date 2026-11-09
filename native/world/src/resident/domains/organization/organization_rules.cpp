#include "organization_service_effects_codec.hpp"
// 任职、成员与组织活动的受控变更；组织不替代真实个人行动或重复推进教师。
#include "organization_internal.hpp"
#include "grant_authority.hpp"
#include "office_authority.hpp"
#include "named_membership_rules.hpp"
#include "organization_original_runtime.hpp"
#include "organization_fact_source.hpp"
#include "organization_calendar_admission.hpp"
#include "resident/domains/local_economy/population_count_revision.hpp"
#include "resident/domains/local_economy/population_labor_change.hpp"
#include "resident/domains/local_economy/production/production_population_change.hpp"
namespace hanmo::resident {
using namespace organization_detail;
// 只保留本人的活职责角色；既有合同接收责任逐角色证明，离职和职责撤销不会扩大服务权限。
Status organization_detail::set_member_services(CommonStore& c,OrganizationState& s,const ActivityRead& access,OrgMemberState& member,bool enabled,bool departing,OrganizationServiceEffectsBuildView* capture){
    for(const auto& id:member.duty_ids){
        auto* duty=s.duties.find(id);
        if(!duty)return common_failure("InvalidMemberDuty",id);
        if(!duty->active)continue;
        const bool duty_active_before=duty->active;
        std::optional<OrganizationRoleEnabledChange> role_enabled_change;
        if(!duty->role_id.empty()){
            const auto* current=c.state().roles.find(duty->role_id);
            if(!current)return common_failure("UnknownRole",duty->role_id);
            const bool available=!departing&&(enabled||access.has_existing_contract_service(member.actor_id,duty->role_id));
            if(current->actor_id==member.actor_id&&current->enabled!=available){
                const bool role_enabled_before=current->enabled;
                auto role=*current;
                role.enabled=available;
                auto result=c.apply_role(duty->role_id,role);
                if(!result.ok)return result;
                if(capture)role_enabled_change=OrganizationRoleEnabledChange{role_enabled_before,available};
            }
        }
        if(departing)duty->active=false;
        if(capture&&(duty_active_before!=duty->active||role_enabled_change)){
            capture->changes.push_back({duty->id,duty->role_id,duty->actor_id,duty_active_before,duty->active,role_enabled_change});
        }
    }
    return common_success();
}

Status organization_detail::attach_organization_service_effects_in_candidate(OrganizationState& s,const OrganizationCatalog& catalog,const StableId& actual_parent,const OrganizationServiceEffectsBuildView& captured){
    if(!catalog.service_effects_enabled)return common_failure("OrganizationServiceEffectsProfileRequired",actual_parent);
    if(!catalog.exact_fact_clock_enabled)return common_failure("OrganizationServiceEffectsExactClockRequired",actual_parent);
    const auto* original=s.facts.find(actual_parent);
    if(!original)return common_failure("OrganizationServiceEffectsParentRequired",actual_parent);
    if(original->id!=actual_parent||(original->kind!="organization_member_response"&&original->kind!="organization_member_services_synchronized"&&original->kind!="organization_office_changed"))return common_failure("OrganizationServiceEffectsParentMismatch",actual_parent);
    if(original->service_effects)return common_failure("OrganizationServiceEffectsAlreadyAttached",actual_parent);
    struct AttachedOrganizationServiceFact {
        CheckpointMemoryCharge copied_fields;
        OrganizationFact value;
        AttachedOrganizationServiceFact(const OrganizationFact& original,const OrganizationServiceEffectsBuildView& captured)
            :copied_fields([&]{return CheckpointMemoryCharge(0);}()),value(original){
            value.service_effects.emplace();
            auto& changes=value.service_effects->changes;
            changes.reserve(captured.changes.size());
            for(const auto& row:captured.changes){
                changes.push_back({StableId(row.duty_id),StableId(row.role_id),StableId(row.actor_id),row.duty_active_before,row.duty_active_after,row.role_enabled_change});
            }
        }
        AttachedOrganizationServiceFact(const AttachedOrganizationServiceFact&)=delete;
        AttachedOrganizationServiceFact& operator=(const AttachedOrganizationServiceFact&)=delete;
        AttachedOrganizationServiceFact(AttachedOrganizationServiceFact&&)=delete;
        AttachedOrganizationServiceFact& operator=(AttachedOrganizationServiceFact&&)=delete;
    } attached(*original,captured);
    auto valid=validate_organization_service_effects_parent(attached.value,*attached.value.service_effects);
    if(!valid.ok)return valid;
    s.facts.set(actual_parent,static_cast<const OrganizationFact&>(attached.value));
    return common_success();
}
namespace {
// 每个成员/活动后果写入唯一领域事实头，供社会消息读取。
Outcome<StableId> append_org_fact(CommonStore& c,OrganizationState& s,OrganizationFact fact,const OrganizationCatalog& catalog,const OrganizationAuthorityClock* clock){if(catalog.exact_fact_clock_enabled){if(!clock)return {false,{},common_failure("OrganizationExactFactClockRequired").error};auto at=read_organization_authority_now(c,clock);if(!at.ok)return {false,{},at.error};if(fact.tick!=at.value.half_day_tick)return {false,{},common_failure("OrganizationFactClockMismatch").error};fact.occurred_at=std::move(at.value);}else if(fact.occurred_at)return {false,{},common_failure("OrganizationExactFactProfileRequired").error};return append_organization_fact_in_candidate(c,s,std::move(fact));}
Outcome<SingleGrantAuthorityRead>
capture_original_issue(const CommonStore &c, const OrganizationState &s,
                       const OrganizationCatalog &catalog,
                       const AuthorityProof &proof,
                       const OrganizationAuthorityClock *clock) {
  auto valid = validate_authority_proof(c, s, catalog, proof, clock);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto grant = read_current_grant(c, s, catalog, proof.grant_id);
  if (!grant.ok)
    return {false, {}, grant.error};
  SingleGrantAuthorityRead out;
  out.source_identity = proof.source_identity;
  out.authority = proof;
  out.office_source = proof.office_source;
  out.grant_source = proof.grant_source;
  out.grant_revision = proof.grant_revision;
  out.effective_grant = std::move(grant.value.terms);
  return {true, std::move(out), {}};
}
Status bind_original_issue(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const OrganizationOriginalRuntimeWriteContext& runtime,const SingleGrantAuthorityRead& captured,const StableId& id){const auto* fact=s.facts.find(id);if(!fact)return common_failure("OrganizationOriginalApprovalSourceRequired",id);const auto& proof=captured.authority;SingleGrantAuthorityRequest request{{proof.organization_id,proof.office_id,proof.authority_epoch,proof.actor_id},proof.command_kind,proof.scope,proof.amount,{},false,{}};NativeOrganizationOriginalAuthorityRead reader(c,s,runtime.clock);auto source=register_authority_basis_for_fact(c,runtime.runtime.authority_bases,s,catalog,catalog.authority_catalog_identity,request,captured,{"organization",fact->kind,id},reader);return source.ok?common_success():Status{false,{},source.error};}
Outcome<WorldMoment> original_now(const CommonStore& c,const OrganizationCatalog& catalog,const OrganizationAuthorityClock* clock){if(catalog.exact_fact_clock_enabled&&!clock)return {false,{},common_failure("OrganizationExactFactClockRequired").error};return read_organization_authority_now(c,clock);}
Outcome<OrganizationEffects> synchronize_member_services(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const ActivityRead& access,OrgMemberState& member,bool enabled,bool departing,Integer tick,const OrganizationAuthorityClock* clock){
    OrganizationEffects effects;
    OrganizationServiceEffectsBuildView captured;
    auto synchronized=set_member_services(c,s,access,member,enabled,departing,catalog.service_effects_enabled?&captured:nullptr);
    if(!synchronized.ok)return {false,{},synchronized.error};
    if(!catalog.service_effects_enabled||captured.changes.empty())return {true,std::move(effects),{}};
    OrganizationFact fact;
    fact.kind="organization_member_services_synchronized";
    fact.organization_id=member.organization_id;
    fact.actor_id=member.actor_id;
    fact.tick=tick;
    fact.response=member.response;
    auto id=append_org_fact(c,s,std::move(fact),catalog,clock);
    if(!id.ok)return {false,{},id.error};
    auto attached=attach_organization_service_effects_in_candidate(s,catalog,id.value,captured);
    if(!attached.ok)return {false,{},attached.error};
    effects.fact_ids.push_back(id.value);
    return {true,std::move(effects),{}};
}
// 只有原Calendar后台责任使用首合法Opening；个人F/W行动仍由原Action源计时。
Outcome<OrganizationCalendarWindow> original_calendar_window(const OrganizationCatalog& catalog,const OrganizationCalendarAdmissionRead* calendar,Integer tick,Integer duration,bool personal=false){
 if(catalog.exact_fact_clock_enabled&&!personal){if(!calendar)return {false,{},common_failure("OrganizationCalendarAdmissionReadRequired",{},ErrorKind::unsupported_capability).error};return calendar->read_new_calendar_window(duration);}
 Integer due=0;if(duration<=0||tick<0||!add(tick,duration,due))return {false,{},common_failure("OrganizationTickOverflow").error};return {true,{tick,due,{}},{}};
}
// The borrowed readers must observe the actual outer candidate before and after
// the original local Org/LE adoption. They must never retain the temporary clones.
Status production_population_candidate(const ProductionPopulationChangeContext& x,const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const WorldMoment& at){
 if(&x.common!=&c||&x.economy!=&economy||&x.original.population.common!=&c||&x.original.population.economy!=&economy||&x.original.population.organizations!=&s||&x.original.training.common!=&c||&x.original.training.organization!=&s||&x.original.training.catalog!=&catalog||&x.original.training.economy!=&economy)return common_failure("OrganizationProductionPopulationCandidateMismatch");
 auto current=x.production.clock.read_current_moment();if(!current.ok)return {false,{},current.error};auto equal=compare_world_moments(current.value,at);if(!equal.ok)return {false,{},equal.error};return equal.value==0?common_success():common_failure("OrganizationProductionPopulationClockMismatch");
}
// 成员状态变化一次一事实；重复相同请求不制造第二笔工资或清空占用。
Outcome<OrganizationEffects> change_member(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead& access,OrgMemberState& member,MemberResponse response,Integer tick,const OrganizationOriginalRuntimeWriteContext* runtime){
    const auto* clock=runtime?&runtime->clock:nullptr;auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return {false,{},common_failure("OrganizationCommandClockMismatch").error};
    OrganizationEffects effects;if(member.response==response){const bool accepts=response==MemberResponse::serving||response==MemberResponse::payment_requested;return synchronize_member_services(c,s,catalog,access,member,accepts||(response!=MemberResponse::departed&&has_personal_action(c,member.actor_id)),response==MemberResponse::departed,tick,clock);}if(member.response==MemberResponse::departed)return {false,{},common_failure("OrganizationMemberDeparted",member.actor_id).error};auto owed=debt(c,s,economy,member);if(!owed.ok)return {false,{},owed.error};const auto* policy=catalog.member_policies.find(member.policy_id);if(!policy)return {false,{},common_failure("UnknownMemberPolicy",member.policy_id).error};
    if(response==MemberResponse::departed){if(!policy->may_leave)return {false,{},common_failure("MemberDepartureNotAllowed",member.actor_id).error};if(personally_committed(c,s,&access,member.actor_id))response=MemberResponse::awaiting_release;}
    OrganizationServiceEffectsBuildView captured;bool serving=response==MemberResponse::serving||response==MemberResponse::payment_requested;auto services=set_member_services(c,s,access,member,serving||(response!=MemberResponse::departed&&has_personal_action(c,member.actor_id)),response==MemberResponse::departed,catalog.service_effects_enabled?&captured:nullptr);if(!services.ok)return {false,{},services.error};
    OrganizationFact fact;fact.kind="organization_member_response";fact.organization_id=member.organization_id;fact.actor_id=member.actor_id;fact.tick=tick;fact.amount=owed.value.amount;fact.cause_fact_ids=owed.value.fact_ids;fact.response=response;auto id=append_org_fact(c,s,std::move(fact),catalog,clock);if(!id.ok)return {false,{},id.error};if(catalog.service_effects_enabled){auto attached=attach_organization_service_effects_in_candidate(s,catalog,id.value,captured);if(!attached.ok)return {false,{},attached.error};}member.response=response;member.response_since_tick=tick;member.next_review_tick=tick+1;member.last_fact_id=id.value;effects.receipt_ids.push_back(id.value);effects.fact_ids.push_back(id.value);if(response==MemberResponse::departed&&catalog.authority_lifecycle_enabled){if(!runtime)return {false,{},common_failure("MembershipOfficeTransitionContextRequired",id.value).error};MembershipOfficeTransitionWriteContext transitions{runtime->runtime.office_transition_sources,runtime->clock,runtime->stamp};auto changed=consume_departed_membership_offices_in_candidate(c,s,catalog,access,runtime->releases,id.value,transitions);if(!changed.ok)return changed;effects.fact_ids.insert(effects.fact_ids.end(),changed.value.fact_ids.begin(),changed.value.fact_ids.end());effects.receipt_ids.insert(effects.receipt_ids.end(),changed.value.receipt_ids.begin(),changed.value.receipt_ids.end());}else if(response==MemberResponse::departed){auto offices=s.offices.order();for(const auto& office_id:offices){const auto* office=s.offices.find(office_id);if(office->organization_id==member.organization_id&&office->holder_actor_id==std::optional<StableId>(member.actor_id)){const auto before=s.facts.size();auto invalidated=invalidate_office(c,s,catalog,office_id,tick,id.value);if(!invalidated.ok)return {false,{},invalidated.error};for(std::size_t n=before;n<s.facts.size();++n){effects.fact_ids.push_back(s.facts.rows()[n].first);effects.receipt_ids.push_back(s.facts.rows()[n].first);}}}}return {true,std::move(effects),{}};
}
// 权限目标必须由同一个明确 grant 覆盖，不能从不同授权拼出更大的权力。
bool scope_matches(const ScopeRef& a,const ScopeRef& b){return a.kind==b.kind&&a.id==b.id;}
}
Outcome<OrganizationState> make_initial_organization(const CommonStore& c,const OrganizationCatalog& catalog){
    OrganizationState out;if(!catalog.enabled)return {true,out,{}};out.organizations=catalog.organizations;out.offices=catalog.offices;out.duties=catalog.duties;out.members=catalog.members;out.activities=catalog.initial_activities;
    auto grant_seed=initialize_grant_authority(c,out,catalog);if(!grant_seed.ok)return {false,{},grant_seed.error};
    for(const auto& row:out.offices.rows())if(row.second.holder_actor_id&&!c.actor(*row.second.holder_actor_id))return {false,{},common_failure("UnknownOfficeHolder",row.first).error};return {true,std::move(out),{}};
}
static bool same_selected_grant(const Grant &a, const Grant &b) {
  if (a.id != b.id || a.office_id != b.office_id ||
      a.allowed_command_kinds != b.allowed_command_kinds ||
      a.wallet_ids != b.wallet_ids || a.inventory_ids != b.inventory_ids ||
      a.amount_limit != b.amount_limit || a.valid_from != b.valid_from ||
      a.valid_until != b.valid_until || a.may_delegate != b.may_delegate ||
      a.continuity_policy_id != b.continuity_policy_id ||
      a.scopes.size() != b.scopes.size())
    return false;
  for (std::size_t n = 0; n < a.scopes.size(); ++n)
    if (a.scopes[n].kind != b.scopes[n].kind ||
        a.scopes[n].id != b.scopes[n].id)
      return false;
  return true;
}
// 新命令绑定具名签发者、当前任职 epoch、候选修订与所有授权资源读版本。
static Outcome<AuthorityProof>
check_original_authority(const CommonStore &c, const OrganizationState &state,
                         const OrganizationCatalog &catalog,
                         const ActingAuthority &acting, const StableId &kind,
                         const ScopeRef &scope, Integer amount,
                         const Grant *selected, const WorldMoment &current,
                         const OrganizationAuthorityClock *clock) {
  const auto *org = state.organizations.find(acting.organization_id);
  const auto *office = state.offices.find(acting.office_id);
  if (amount < 0 || !org || org->status != OrganizationStatus::active ||
      !office || office->organization_id != org->id ||
      office->status != OfficeStatus::active ||
      office->epoch != acting.authority_epoch || !office->holder_actor_id ||
      *office->holder_actor_id != acting.actor_id ||
      !eligible_officer(c, acting.actor_id))
    return {false, {}, common_failure("OrganizationAuthorityDenied").error};
  for (const auto &member : state.members.rows())
    if (!active_membership_episode(state, acting.actor_id,
                                   acting.organization_id) &&
        member.second.actor_id == acting.actor_id &&
        member.second.organization_id == acting.organization_id &&
        member.second.response == MemberResponse::departed)
      return {false, {}, common_failure("OrganizationAuthorityDenied").error};
  if (catalog.authority_catalog_identity.content_digest.empty() ||
      catalog.authority_catalog_identity.content_digest !=
          c.state().metadata.content_digest)
    return {false,
            {},
            common_failure("AuthorityProofCatalogIdentityRequired").error};
  auto source_history =
      validate_grant_authority_history(c, state, catalog, clock);
  if (!source_history.ok)
    return {false, {}, source_history.error};
  auto term = read_current_office_term(
      c, state, catalog, catalog.authority_catalog_identity, office->id, clock);
  if (!term.ok)
    return {false, {}, term.error};
  for (const auto &id : office->grant_ids) {
    if (selected && selected->id != id)
      continue;
    auto original = read_current_grant(c, state, catalog, id);
    if (!original.ok) {
      if (original.error.kind != ErrorKind::rule)
        return {false, {}, original.error};
      continue;
    }
    const auto *grant = &original.value.terms;
    if (selected && !same_selected_grant(*selected, *grant))
      return {false,
              {},
              common_failure("AuthoritySelectedGrantMismatch", id).error};
    // A narrowed catalog cannot turn an unlisted authored grant into a runtime
    // grant.
    if (!selected && original.value.revision == 0 && !catalog.grants.find(id))
      continue;
    auto window = authority_window_contains(grant->valid_from,
                                            grant->valid_until, current);
    if (!window.ok)
      return {false, {}, window.error};
    if (grant->office_id != office->id ||
        !has(grant->allowed_command_kinds, kind) ||
        amount > grant->amount_limit || !window.value ||
        std::none_of(grant->scopes.begin(), grant->scopes.end(),
                     [&](const auto &s) { return scope_matches(s, scope); }))
      continue;
    AuthorityProof proof;
    proof.organization_id = org->id;
    proof.office_id = office->id;
    proof.actor_id = acting.actor_id;
    proof.command_kind = kind;
    proof.scope = scope;
    proof.candidate_revision = c.revision();
    proof.authority_epoch = office->epoch;
    proof.amount = amount;
    proof.source_identity = c.state().identity;
    proof.grant_id = id;
    proof.grant_revision = original.value.revision;
    proof.office_source = term.value.source;
    proof.grant_source =
        original.value.revision == 0
            ? GrantAuthoritySource{AuthoredGrantAuthoritySource{
                  catalog.authority_catalog_identity, id}}
            : GrantAuthoritySource{GrantRevisionAuthoritySource{
                  original.value.last_change_fact_id, id,
                  original.value.revision}};
    proof.observed_at = current;
    for (const auto &wallet : grant->wallet_ids) {
      const auto *w = c.wallet(wallet);
      if (!w || w->owner.kind != Owner::Kind::organization ||
          w->owner.id != org->id)
        return {false,
                {},
                common_failure("AuthorityResourceMismatch", wallet).error};
      proof.resource_versions.push_back(
          c.read_version({ResourceKey::Kind::wallet, wallet}));
    }
    for (const auto &inventory : grant->inventory_ids) {
      const auto *i = c.inventory(inventory);
      if (!i || i->owner.kind != Owner::Kind::organization ||
          i->owner.id != org->id)
        return {false,
                {},
                common_failure("AuthorityResourceMismatch", inventory).error};
      proof.resource_versions.push_back(
          c.read_version({ResourceKey::Kind::inventory, inventory}));
    }
    return {true, std::move(proof), {}};
  }
  return {false, {}, common_failure("OrganizationAuthorityDenied").error};
}
Outcome<AuthorityProof> check_authority_with_single_grant(
    const CommonStore &c, const OrganizationState &state,
    const OrganizationCatalog &catalog, const ActingAuthority &acting,
    const StableId &kind, const ScopeRef &scope, Integer amount,
    const Grant &selected, const OrganizationAuthorityClock *clock) {
  if (catalog.exact_fact_clock_enabled && !clock)
    return {
        false, {}, common_failure("OrganizationExactFactClockRequired").error};
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  return check_original_authority(c, state, catalog, acting, kind, scope,
                                  amount, &selected, now.value, clock);
}
Outcome<AuthorityProof>
check_authority(const CommonStore &c, const OrganizationState &state,
                const OrganizationCatalog &catalog,
                const ActingAuthority &acting, const StableId &kind,
                const ScopeRef &scope, Integer amount,
                const OrganizationAuthorityClock *clock) {
  auto now = original_now(c, catalog, clock);
  if (!now.ok)
    return {false, {}, now.error};
  return check_original_authority(c, state, catalog, acting, kind, scope,
                                  amount, nullptr, now.value, clock);
}
// 应用前重做权限和资源版本检查，不把旧 proof 当成永久授权。
Status validate_authority_proof(const CommonStore &c,
                                const OrganizationState &state,
                                const OrganizationCatalog &catalog,
                                const AuthorityProof &proof,
                                const OrganizationAuthorityClock *clock) {
  if (proof.schema_version != 2 || proof.grant_id.empty() ||
      proof.candidate_revision != c.revision())
    return common_failure("StaleAuthorityProof");
  auto original = read_current_grant(c, state, catalog, proof.grant_id);
  if (!original.ok)
    return {false, {}, original.error};
  auto current = check_authority_with_single_grant(
      c, state, catalog,
      {proof.organization_id, proof.office_id, proof.authority_epoch,
       proof.actor_id},
      proof.command_kind, proof.scope, proof.amount, original.value.terms,
      clock);
  if (!current.ok)
    return {false, {}, current.error};
  if (!same_authority_proof(current.value, proof))
    return common_failure("StaleAuthorityProof");
  return common_success();
}
Status validate_authority_proof(const CommonStore &c,
                                const OrganizationState &s,
                                const OrganizationCatalog &catalog,
                                const AuthorityProof &proof) {
  return validate_authority_proof(c, s, catalog, proof, nullptr);
}
// 职位失效先递增 epoch，合法副手存在时才继任，历史责任仍挂在原组织。
Status invalidate_office(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const StableId& id,Integer tick,const StableId& cause){
    if(catalog.authority_lifecycle_enabled)return common_failure("VerifiedOfficeTransitionRequired",id);
    auto* office=s.offices.find(id);if(!office)return common_failure("UnknownOffice",id);if(office->epoch==INT64_MAX)return common_failure("AuthorityEpochOverflow",id);const auto old_holder=office->holder_actor_id;office->holder_actor_id.reset();office->status=OfficeStatus::vacant;++office->epoch;
    if(office->deputy_actor_id&&eligible_officer(c,*office->deputy_actor_id)&&office->deputy_actor_id!=old_holder){bool departed=false;for(const auto& row:s.members.rows())if(row.second.actor_id==*office->deputy_actor_id&&row.second.organization_id==office->organization_id&&row.second.response==MemberResponse::departed)departed=true;if(!departed||active_membership_episode(s,*office->deputy_actor_id,office->organization_id)){office->holder_actor_id=office->deputy_actor_id;office->deputy_actor_id.reset();office->status=OfficeStatus::active;}}
    auto* org=s.organizations.find(office->organization_id);if(!org)return common_failure("UnknownOrganization",office->organization_id);// 单职位旧组织仍按原规则限权；多辖区的局部空缺不能关闭其他有效Office。
    if(org->status!=OrganizationStatus::dissolved){bool has_authority=false;for(const auto& office_id:org->office_ids){const auto* remaining=s.offices.find(office_id);if(remaining&&remaining->status==OfficeStatus::active&&remaining->holder_actor_id&&eligible_officer(c,*remaining->holder_actor_id)){has_authority=true;break;}}org->status=has_authority?OrganizationStatus::active:OrganizationStatus::limited;}
    OrganizationFact fact;fact.kind="organization_office_changed";fact.organization_id=office->organization_id;fact.office_id=id;fact.actor_id=office->holder_actor_id?*office->holder_actor_id:StableId{};fact.tick=tick;if(!cause.empty())fact.cause_fact_ids.push_back(cause);auto result=append_org_fact(c,s,std::move(fact),catalog,nullptr);return result.ok?common_success():Status{false,{},result.error};
}
// 欠薪先请求，再拒绝新职责，最后在真实承诺释放后离开；阈值全来自作者政策。
Outcome<OrganizationEffects> reconcile_organization(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead& access,Integer tick,const OrganizationOriginalRuntimeWriteContext* runtime){
    const auto* clock=runtime?&runtime->clock:nullptr;auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return {false,{},common_failure("OrganizationCommandClockMismatch").error};
    if(!catalog.enabled)return {true,{},{} };if(tick<0||tick==INT64_MAX)return {false,{},common_failure("InvalidOrganizationTick").error};local_economy_detail::PhaseSavepoint guard(c);if(!guard.token().ok)return {false,{},guard.token().error};auto next=s;OrganizationEffects effects;
    if(!catalog.authority_lifecycle_enabled)for(const auto& id:next.offices.order()){auto* office=next.offices.find(id);if(office->status==OfficeStatus::active&&(!office->holder_actor_id||!eligible_officer(c,*office->holder_actor_id))){auto prior=next.facts.size();auto invalid=invalidate_office(c,next,catalog,id,tick,{});if(!invalid.ok)return {false,{},invalid.error};for(std::size_t i=prior;i<next.facts.size();++i){effects.fact_ids.push_back(next.facts.rows()[i].first);effects.receipt_ids.push_back(next.facts.rows()[i].first);}}}
    for(auto& row:next.members.rows()){auto& member=row.second;if(catalog.named_membership_enabled||member.response==MemberResponse::departed||member.next_review_tick>tick){const bool accepts=member.response==MemberResponse::serving||member.response==MemberResponse::payment_requested;auto synchronized=synchronize_member_services(c,next,catalog,access,member,accepts||(member.response!=MemberResponse::departed&&has_personal_action(c,member.actor_id)),member.response==MemberResponse::departed,tick,clock);if(!synchronized.ok)return {false,{},synchronized.error};effects.fact_ids.insert(effects.fact_ids.end(),synchronized.value.fact_ids.begin(),synchronized.value.fact_ids.end());continue;}const auto* policy=catalog.member_policies.find(member.policy_id);if(!policy)return {false,{},common_failure("UnknownMemberPolicy",member.policy_id).error};auto owed=debt(c,next,economy,member);if(!owed.ok)return {false,{},owed.error};MemberResponse response=MemberResponse::serving;
        if(owed.value.amount>=policy->minimum_arrears&&owed.value.earliest_due>=0){Integer age=tick-owed.value.earliest_due;if(age>=policy->leave_after_ticks&&policy->may_leave)response=personally_committed(c,next,&access,member.actor_id)?MemberResponse::awaiting_release:MemberResponse::departed;else if(age>=policy->refuse_after_ticks)response=MemberResponse::refusing_new_duties;else if(age>=policy->request_after_ticks)response=MemberResponse::payment_requested;}
        auto changed=change_member(c,next,catalog,economy,access,member,response,tick,runtime);if(!changed.ok)return changed;effects.fact_ids.insert(effects.fact_ids.end(),changed.value.fact_ids.begin(),changed.value.fact_ids.end());effects.receipt_ids.insert(effects.receipt_ids.end(),changed.value.receipt_ids.begin(),changed.value.receipt_ids.end());member.next_review_tick=tick+1;
    }
    auto committed=guard.commit();if(!committed.ok)return {false,{},committed.error};s=std::move(next);return {true,std::move(effects),{}};
}
// 预览只检查本人已知的义务来源和合法退出条件，不分配 ID 或改动角色。
Status preview_organization_member_command(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead&,const OrganizationMemberCommand& command,Integer tick,const OrganizationAuthorityClock* clock){
    auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return common_failure("OrganizationCommandClockMismatch");
    if(tick<0||tick==INT64_MAX)return common_failure("InvalidOrganizationTick");const OrgMemberState* member=nullptr;for(const auto& row:s.members.rows())if(row.second.actor_id==command.actor_id&&row.second.organization_id==command.organization_id){member=&row.second;break;}const auto* actor=c.actor(command.actor_id);if(!member||!actor||!actor->alive)return common_failure("UnknownOrganizationMember");if(member->response==MemberResponse::departed)return common_failure("OrganizationMemberDeparted");const auto* policy=catalog.member_policies.find(member->policy_id);if(!policy)return common_failure("UnknownMemberPolicy");auto owed=debt(c,s,economy,*member);if(!owed.ok)return {false,{},owed.error};if(owed.value.amount<policy->minimum_arrears)return common_failure("NoUnpaidMemberObligation");if(!command.source_fact_id.empty()&&!has(owed.value.fact_ids,command.source_fact_id)&&member->last_fact_id!=command.source_fact_id)return common_failure("UnknownMemberResponseSource");if(command.kind==MemberCommandKind::lawful_leave&&!policy->may_leave)return common_failure("MemberDepartureNotAllowed");return common_success();
}
// 预览核实已同意群体、供养、未来劳力区间与真实训练容量；不生成批准事实。
Status preview_organization_recruit_command(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead&,const OrganizationRecruitCommand& command,Integer tick,const OrganizationAuthorityClock* clock,const OrganizationCalendarAdmissionRead* calendar){
    auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return common_failure("OrganizationCommandClockMismatch");
    const auto* terms=catalog.activity_terms.find(command.activity_terms_id);const auto* source=economy.cohorts.find(command.source_cohort_id);const auto* target=economy.cohorts.find(command.target_cohort_id);if(!terms||terms->kind!=OrgActivityKind::population_training||!source||!target||source==target||source->status!=PopulationStatus::civilian||target->status!=PopulationStatus::training||target->headcount!=0||target->assignment_id||source->site_id!=terms->training_site_id||target->site_id!=terms->training_site_id||source->household_group_id!=target->household_group_id||!has(terms->consenting_cohort_ids,source->id)||command.count<=0||command.labor_count<=0||command.labor_count>command.count||command.count>terms->training_capacity||command.count>source->headcount||command.labor_count>source->labor_capacity||source->labor_capacity-command.labor_count>source->headcount-command.count||tick!=now.value.half_day_tick)return common_failure("InvalidVoluntaryRecruitment");auto window=original_calendar_window(catalog,calendar,tick,terms->duration_ticks);if(!window.ok)return {false,{},window.error};const Integer due=window.value.until_tick;auto proof=check_authority(c,s,catalog,command.authority,"recruit_population",{ScopeRef::Kind::site,terms->training_site_id},command.count,clock);if(!proof.ok)return {false,{},proof.error};Integer occupied=0;for(const auto& row:s.activities.rows())if(row.second.terms_id==terms->id&&active_activity(row.second))for(const auto& p:row.second.participation)if(p.binding_kind==ParticipationKind::cohort_allocation){const auto* cohort=economy.cohorts.find(p.subject.id);if(!cohort||!add(occupied,cohort->headcount,occupied))return common_failure("InvalidTrainingCapacity");}if(occupied>terms->training_capacity-command.count)return common_failure("TrainingCapacityUnavailable");
    std::set<StableId> releases;Integer releasable=0;for(const auto& id:command.release_allocation_ids){const auto* allocation=economy.labor_allocations.find(id);if(!allocation||allocation->cohort_id!=source->id||!releases.insert(id).second||!add(releasable,allocation->workers,releasable))return common_failure("InvalidLaborRelease",id);}
    std::set<Integer> points{tick};for(const auto& row:economy.labor_allocations.rows())if(row.second.cohort_id==source->id&&row.second.through_tick>=tick)points.insert(std::max(tick,row.second.from_tick));for(auto point:points){Integer committed=0,released=0;for(const auto& row:economy.labor_allocations.rows())if(row.second.cohort_id==source->id&&row.second.from_tick<=point&&point<=row.second.through_tick){if(!add(committed,row.second.workers,committed))return common_failure("EconomyQuantityOverflow");if(releases.count(row.first)&&!add(released,row.second.workers,released))return common_failure("EconomyQuantityOverflow");}if(committed-released>source->labor_capacity-command.labor_count)return common_failure("PopulationLaborCommitted",source->id);}
    bool provisioned=false;for(const auto& row:economy.provision_bindings.rows())if(row.second.subject.kind==SubjectKind::cohort&&row.second.subject.id==target->id&&row.second.policy_owner==ProvisionOwner::macro&&row.second.from_tick<=tick&&row.second.through_tick>=(catalog.exact_fact_clock_enabled?due-1:due))provisioned=true;if(!provisioned)return common_failure("PopulationProvisionRequired",target->id);return common_success();
}
// 常规职事预览验证作者职责映射与真实行动参与者，不从空闲总量推断可以强占教师。
Status preview_organization_activity_command(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState&,const ActivityRead& access,const OrganizationActivityCommand& command,Integer tick,const DynamicResultRead* results,const OrganizationAuthorityClock* clock,const OrganizationCalendarAdmissionRead* calendar){
    auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return common_failure("OrganizationCommandClockMismatch");
    const auto* terms=catalog.activity_terms.find(command.terms_id);const auto* org=s.organizations.find(command.authority.organization_id);if(!terms||terms->kind==OrgActivityKind::population_training||!org||!org->headquarters_site||tick!=now.value.half_day_tick||command.participation.empty())return common_failure("InvalidOrganizationActivityCommand");Integer due=0;if(!add(tick,terms->duration_ticks,due)||terms->duration_ticks<=0)return common_failure("OrganizationTickOverflow");auto proof=check_authority(c,s,catalog,command.authority,"approve_org_activity",{ScopeRef::Kind::site,*org->headquarters_site},0,clock);if(!proof.ok)return {false,{},proof.error};std::set<StableId> participants;bool personal=false;for(const auto& p:command.participation){if(p.binding_kind==ParticipationKind::cohort_allocation)return common_failure("OrganizationCohortActivityRequiresOwner",p.binding_id,ErrorKind::unsupported_capability);const auto* actor=c.actor(p.subject.id);if(p.subject.kind!=SubjectKind::actor||!actor||!actor->alive||!actor->available||!participants.insert(p.subject.id).second)return common_failure("OrganizationExecutorUnavailable");bool has_duty=false;for(const auto& row:s.duties.rows())if(row.second.active&&row.second.actor_id==p.subject.id&&row.second.organization_id==org->id&&row.second.activity_terms_id==std::optional<StableId>(terms->id))has_duty=true;if(!has_duty)return common_failure("OrganizationDutyTermsMismatch");if(const auto* member=active_membership_episode(s,p.subject.id,org->id);member&&member->response!=MemberResponse::serving&&member->response!=MemberResponse::payment_requested)return common_failure("MemberDeclinesNewDuty");if(p.binding_kind==ParticipationKind::personal_action){personal=true;const auto* action=c.action(p.binding_id);if(action&&(!results||!results->supports_action_result(action->kind)))return common_failure("UnsupportedOrganizationActionResult",action->kind,ErrorKind::unsupported_capability);if(!action||(action->actor_id!=p.subject.id&&!has(action->participants,p.subject.id))||access.in_open_battle(p.subject.id))return common_failure("OrganizationExecutorUnavailable");for(const auto& activity:s.activities.rows())if(active_activity(activity.second))for(const auto& participation:activity.second.participation)if(participation.binding_kind==ParticipationKind::personal_action&&participation.binding_id==p.binding_id)return common_failure("OrganizationActionAlreadyBound");}}
    if(terms->kind==OrgActivityKind::contracted_work&&!personal)return common_failure("OrganizationActivityNeedsRealAction");auto admitted=original_calendar_window(catalog,calendar,tick,terms->duration_ticks,personal);if(!admitted.ok)return {false,{},admitted.error};for(const auto& id:command.commitment_ids)if(!c.action(id)&&!s.activities.find(id)&&!c.state().facts.find(id))return common_failure("UnknownDutyCommitment",id);return common_success();
}
// 主体自己的请求只能针对本人、当前组织及已经形成的欠薪来源。
Outcome<OrganizationEffects> apply_organization_member_command(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead& access,const OrganizationMemberCommand& command,Integer tick,const OrganizationOriginalRuntimeWriteContext* runtime){
    const auto* clock=runtime?&runtime->clock:nullptr;auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return {false,{},common_failure("OrganizationCommandClockMismatch").error};
    auto preview=preview_organization_member_command(c,s,catalog,economy,access,command,tick,clock);if(!preview.ok)return {false,{},preview.error};if(tick<0||tick==INT64_MAX)return {false,{},common_failure("InvalidOrganizationTick").error};OrgMemberState* member=nullptr;for(auto& row:s.members.rows())if(row.second.actor_id==command.actor_id&&row.second.organization_id==command.organization_id){member=&row.second;break;}const auto* actor=c.actor(command.actor_id);if(!member||!actor||!actor->alive)return {false,{},common_failure("UnknownOrganizationMember").error};auto owed=debt(c,s,economy,*member);if(!owed.ok)return {false,{},owed.error};if(!command.source_fact_id.empty()&&!has(owed.value.fact_ids,command.source_fact_id)&&member->last_fact_id!=command.source_fact_id)return {false,{},common_failure("UnknownMemberResponseSource").error};const auto* policy=catalog.member_policies.find(member->policy_id);if(!policy)return {false,{},common_failure("UnknownMemberPolicy").error};
    if(owed.value.amount<policy->minimum_arrears)return {false,{},common_failure("NoUnpaidMemberObligation").error};MemberResponse response=MemberResponse::payment_requested;if(command.kind==MemberCommandKind::decline_new_work)response=MemberResponse::refusing_new_duties;else if(command.kind==MemberCommandKind::lawful_leave)response=MemberResponse::departed;else if(command.kind==MemberCommandKind::wait_payment)response=member->response;
    return change_member(c,s,catalog,economy,access,*member,response,tick,runtime);
}
// 组织活动只接受真实可用执行者；具名亲自执行必须先已有行动，不伪造忙碌位。
Status approve_org_activity(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead& access,const AuthorityProof& proof,const OrgActivity& activity,const DynamicResultRead* results,const OrganizationAuthorityClock* clock,const OrganizationCalendarAdmissionRead* calendar){
    auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};
    auto valid=validate_authority_proof(c,s,catalog,proof,clock);if(!valid.ok)return valid;const auto* terms=catalog.activity_terms.find(activity.terms_id);bool personal=false;for(const auto& p:activity.participation)personal|=p.binding_kind==ParticipationKind::personal_action;if(!terms)return common_failure("UnknownOrganizationActivityTerms",activity.terms_id);auto window=original_calendar_window(catalog,calendar,now.value.half_day_tick,terms->duration_ticks,personal);if(!window.ok)return {false,{},window.error};if(!terms||terms->kind!=activity.kind||activity.id.empty()||s.activities.find(activity.id)||activity.principal.kind!=SubjectKind::organization||activity.principal.id!=proof.organization_id||activity.responsible_office_id!=proof.office_id||activity.started_tick!=window.value.from_tick||(catalog.exact_fact_clock_enabled&&(activity.phase_started_tick!=window.value.from_tick||activity.due_tick!=window.value.until_tick))||activity.due_tick<activity.started_tick||activity.execution.mode!=ExecutionMode::background||activity.execution.activity_id!=activity.id)return common_failure("InvalidOrganizationActivity",activity.id);
    if(!matching_executors(activity))return common_failure("OrganizationExecutorBindingMismatch",activity.id);
    std::set<StableId> physical;for(const auto& p:activity.participation){if(p.binding_kind==ParticipationKind::administrative_reference)continue;if(p.binding_kind==ParticipationKind::personal_action){const auto* actor=c.actor(p.subject.id);const auto* action=c.action(p.binding_id);if(action&&(!results||!results->supports_action_result(action->kind)))return common_failure("UnsupportedOrganizationActionResult",action->kind,ErrorKind::unsupported_capability);if(p.subject.kind!=SubjectKind::actor||!actor||!actor->alive||!actor->available||!action||(action->actor_id!=actor->id&&!has(action->participants,actor->id))||!physical.insert(actor->id).second||access.in_open_battle(actor->id))return common_failure("OrganizationExecutorUnavailable",p.subject.id);if(const auto* member=active_membership_episode(s,actor->id,proof.organization_id);member&&member->response!=MemberResponse::serving&&member->response!=MemberResponse::payment_requested)return common_failure("MemberDeclinesNewDuty",actor->id);}else{const auto* allocation=economy.labor_allocations.find(p.binding_id);if(p.subject.kind!=SubjectKind::cohort||!allocation||allocation->cohort_id!=p.subject.id||allocation->activity_or_production_id!=activity.id)return common_failure("InvalidOrganizationAllocation",p.binding_id);}}
    s.activities.insert(activity.id,activity);return common_success();
}
// 合法自愿征募真实扣转人口和劳力；训练供养目标必须事先登记，容量不足拒绝。
Outcome<OrganizationEffects> apply_organization_recruit_command(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,LocalEconomyState& economy,const ActivityRead& access,const OrganizationRecruitCommand& command,Integer tick,const OrganizationOriginalRuntimeWriteContext* runtime){
    const auto* clock=runtime?&runtime->clock:nullptr;auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return {false,{},common_failure("OrganizationCommandClockMismatch").error};
    if(catalog.exact_fact_clock_enabled&&(!runtime||!runtime->population))return {false,{},common_failure("PopulationTrainingRevisionContextRequired",{},ErrorKind::unsupported_capability).error};auto preview=preview_organization_recruit_command(c,s,catalog,economy,access,command,tick,clock,runtime?runtime->calendar:nullptr);if(!preview.ok)return {false,{},preview.error};const auto* terms=catalog.activity_terms.find(command.activity_terms_id);const auto* source=economy.cohorts.find(command.source_cohort_id);const auto* target=economy.cohorts.find(command.target_cohort_id);if(!terms||terms->kind!=OrgActivityKind::population_training||!source||!target||source->status!=PopulationStatus::civilian||target->status!=PopulationStatus::training||target->headcount!=0||target->assignment_id||source->site_id!=terms->training_site_id||target->site_id!=terms->training_site_id||!has(terms->consenting_cohort_ids,source->id)||command.count<=0||command.labor_count<=0||command.labor_count>command.count||command.count>terms->training_capacity||tick!=now.value.half_day_tick)return {false,{},common_failure("InvalidVoluntaryRecruitment").error};auto window=original_calendar_window(catalog,runtime?runtime->calendar:nullptr,tick,terms->duration_ticks);if(!window.ok)return {false,{},window.error};const Integer due=window.value.until_tick;
    auto proof=check_authority(c,s,catalog,command.authority,"recruit_population",{ScopeRef::Kind::site,terms->training_site_id},command.count,clock);if(!proof.ok)return {false,{},proof.error};Integer occupied=0;for(const auto& row:s.activities.rows())if(row.second.terms_id==terms->id&&active_activity(row.second))for(const auto& p:row.second.participation)if(p.binding_kind==ParticipationKind::cohort_allocation){const auto* cohort=economy.cohorts.find(p.subject.id);if(!cohort||!add(occupied,cohort->headcount,occupied))return {false,{},common_failure("InvalidTrainingCapacity").error};}if(occupied>terms->training_capacity-command.count)return {false,{},common_failure("TrainingCapacityUnavailable").error};
    std::optional<SingleGrantAuthorityRead> captured;if(catalog.exact_fact_clock_enabled){if(!runtime)return {false,{},common_failure("OrganizationOriginalRuntimeRequired").error};auto read=capture_original_issue(c,s,catalog,proof.value,clock);if(!read.ok)return {false,{},read.error};captured=std::move(read.value);}
    std::optional<PreparedProductionPopulationChange> production_change;
    if(catalog.exact_fact_clock_enabled){
        if(!runtime->production_population)return {false,{},common_failure("OrganizationProductionPopulationContextRequired",{},ErrorKind::unsupported_capability).error};
        auto& x=*runtime->production_population;auto same=production_population_candidate(x,c,s,catalog,economy,now.value);if(!same.ok)return {false,{},same.error};
        if(&x.original.population.population!=runtime->population||&x.original.training.runtime!=&runtime->runtime)return {false,{},common_failure("OrganizationProductionPopulationCandidateMismatch").error};
        auto prepared=prepare_production_population_change(x,{source->id,target->id});if(!prepared.ok)return {false,{},prepared.error};production_change.emplace(std::move(prepared.value));
    }
    // 中央命令候选拥有共同 savepoint；本域副本只在全部成功后替换。
    auto next=s;auto next_economy=economy;OrganizationEffects effects;OrganizationFact approved;approved.kind="population_training_approved";approved.organization_id=command.authority.organization_id;approved.actor_id=command.authority.actor_id;approved.office_id=command.authority.office_id;approved.tick=tick;approved.amount=command.count;auto approval=append_org_fact(c,next,std::move(approved),catalog,clock);if(!approval.ok)return {false,{},approval.error};auto activity_id=c.next_id("org_activity");if(!activity_id.ok)return {false,{},activity_id.error};auto allocation_id=c.next_id("labor");if(!allocation_id.ok)return {false,{},allocation_id.error};next_economy.cohorts.find(target->id)->assignment_id=activity_id.value;
    PopulationTransferRequest move;move.from_cohort_id=source->id;move.to_cohort_id=target->id;move.reason="voluntary_training";move.source_fact_id=approval.value;move.count=command.count;move.labor_eligible_count=command.labor_count;move.effective_tick=tick;move.destination_status=PopulationStatus::training;move.assignment_id=activity_id.value;move.release_allocation_ids=command.release_allocation_ids;PopulationTransferLaborCapture labor_capture;auto* capture=catalog.exact_fact_clock_enabled&&runtime->population->version==3?&labor_capture:nullptr;auto moved=transfer_population(c,next_economy,move,capture);if(!moved.ok)return {false,{},moved.error};if(catalog.exact_fact_clock_enabled){auto revision=record_population_transfer_revisions_in_candidate(c,next_economy,*runtime->population,moved.value,capture);if(!revision.ok)return {false,{},revision.error};}
    LaborAllocation allocation{allocation_id.value,target->id,activity_id.value,command.labor_count,window.value.from_tick,catalog.exact_fact_clock_enabled?due-1:due,activity_id.value};auto allocated=allocate_labor(c,next_economy,allocation);if(!allocated.ok)return {false,{},allocated.error};OrgActivity activity;activity.id=activity_id.value;activity.kind=OrgActivityKind::population_training;activity.principal={SubjectKind::organization,command.authority.organization_id};activity.responsible_office_id=command.authority.office_id;activity.executors={{SubjectKind::cohort,target->id}};activity.started_tick=window.value.from_tick;activity.phase_started_tick=window.value.from_tick;activity.due_tick=due;activity.phase="training";activity.terms_id=terms->id;activity.status=OrgActivityStatus::active;activity.participation={{{SubjectKind::cohort,target->id},"trainee",ParticipationKind::cohort_allocation,allocation.id}};activity.execution={activity.id,activity.phase,ExecutionMode::background,0,{},{}};auto accepted=approve_org_activity(c,next,catalog,next_economy,access,proof.value,activity,nullptr,clock,runtime?runtime->calendar:nullptr);if(!accepted.ok)return {false,{},accepted.error};next.facts.find(approval.value)->activity_id=activity.id;
    if(captured){auto bound=bind_original_issue(c,next,catalog,*runtime,*captured,approval.value);if(!bound.ok)return {false,{},bound.error};}
    s=std::move(next);economy=std::move(next_economy);
    // All real transfer/revision/allocation/approval/Basis rows are now visible to
    // this same candidate's readers. A downstream error rejects the whole candidate.
    if(production_change){auto changed=apply_production_population_change_in_candidate(*runtime->production_population,std::move(*production_change),{{{"local_economy","population_transfer",moved.value},1}});if(!changed.ok)return {false,{},changed.error};}
    effects.receipt_ids={approval.value,moved.value};effects.fact_ids=effects.receipt_ids;return {true,std::move(effects),{}};
}
// 常规职事以明确授权建立活动；亲自执行只绑定已经存在的真实行动。
Outcome<OrganizationEffects> apply_organization_activity_command(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,const LocalEconomyState& economy,const ActivityRead& access,const OrganizationActivityCommand& command,Integer tick,const DynamicResultRead* results,const OrganizationOriginalRuntimeWriteContext* runtime){
    const auto* clock=runtime?&runtime->clock:nullptr;auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled&&tick!=now.value.half_day_tick)return {false,{},common_failure("OrganizationCommandClockMismatch").error};
    auto preview=preview_organization_activity_command(c,s,catalog,economy,access,command,tick,results,clock,runtime?runtime->calendar:nullptr);if(!preview.ok)return {false,{},preview.error};const auto* terms=catalog.activity_terms.find(command.terms_id);if(!terms||terms->kind==OrgActivityKind::population_training||tick!=now.value.half_day_tick||command.participation.empty())return {false,{},common_failure("InvalidOrganizationActivityCommand").error};const auto* organization=s.organizations.find(command.authority.organization_id);if(!organization)return {false,{},common_failure("UnknownOrganization").error};if(!organization->headquarters_site)return {false,{},common_failure("OrganizationFixedHeadquartersRequired").error};Integer due=0;if(!add(tick,terms->duration_ticks,due))return {false,{},common_failure("OrganizationTickOverflow").error};auto proof=check_authority(c,s,catalog,command.authority,"approve_org_activity",{ScopeRef::Kind::site,*organization->headquarters_site},0,clock);if(!proof.ok)return {false,{},proof.error};
    bool personal=false;for(const auto& p:command.participation){if(p.binding_kind==ParticipationKind::cohort_allocation)return {false,{},common_failure("OrganizationCohortActivityRequiresOwner",p.binding_id,ErrorKind::unsupported_capability).error};personal|=p.binding_kind==ParticipationKind::personal_action;}
    if(terms->kind==OrgActivityKind::contracted_work&&!personal)return {false,{},common_failure("OrganizationActivityNeedsRealAction").error};auto window=original_calendar_window(catalog,runtime?runtime->calendar:nullptr,tick,terms->duration_ticks,personal);if(!window.ok)return {false,{},window.error};due=window.value.until_tick;
    std::optional<SingleGrantAuthorityRead> captured;if(catalog.exact_fact_clock_enabled){if(!runtime)return {false,{},common_failure("OrganizationOriginalRuntimeRequired").error};auto read=capture_original_issue(c,s,catalog,proof.value,clock);if(!read.ok)return {false,{},read.error};captured=std::move(read.value);}
    auto next=s;auto id=c.next_id("org_activity");if(!id.ok)return {false,{},id.error};OrgActivity activity;activity.id=id.value;activity.kind=terms->kind;activity.principal={SubjectKind::organization,organization->id};activity.responsible_office_id=command.authority.office_id;activity.participation=command.participation;for(const auto& p:command.participation)activity.executors.push_back(p.subject);activity.commitment_ids=command.commitment_ids;activity.started_tick=window.value.from_tick;activity.phase_started_tick=window.value.from_tick;activity.due_tick=due;activity.phase="active";activity.status=OrgActivityStatus::active;activity.terms_id=terms->id;activity.execution={activity.id,activity.phase,ExecutionMode::background,0,{},{}};auto valid=approve_org_activity(c,next,catalog,economy,access,proof.value,activity,results,clock,runtime?runtime->calendar:nullptr);if(!valid.ok)return {false,{},valid.error};OrganizationFact approved;approved.kind="organization_activity_approved";approved.organization_id=organization->id;approved.actor_id=command.authority.actor_id;approved.office_id=command.authority.office_id;approved.activity_id=id.value;approved.tick=tick;auto result=append_org_fact(c,next,std::move(approved),catalog,clock);if(!result.ok)return {false,{},result.error};if(captured){auto bound=bind_original_issue(c,next,catalog,*runtime,*captured,result.value);if(!bound.ok)return {false,{},bound.error};}s=std::move(next);return {true,{{result.value},{result.value}}, {}};
}
// 仅 capture 的既有活动可推进；个人行动只读取原域已完成事实。
Outcome<OrganizationEffects> step_organization_activities(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,LocalEconomyState& economy,const ActivityRead&,const DynamicResultRead& results,const DynamicStepInput& input,const OrganizationAuthorityClock* clock,const OrganizationCalendarAdmissionRead* calendar,const OrganizationTrainingCoverageRead* training,ProductionPopulationChangeContext* production_population){
    auto now=original_now(c,catalog,clock);if(!now.ok)return {false,{},now.error};if(catalog.exact_fact_clock_enabled){auto exact=compare_world_moments(now.value,{input.to_tick,{}});if(!exact.ok)return {false,{},exact.error};if(exact.value||input.from_tick==INT64_MAX||input.to_tick!=input.from_tick+1)return {false,{},common_failure("OrganizationCalendarBoundaryRequired").error};}
    if(!catalog.enabled)return {true,{},{} };
    std::optional<PreparedProductionPopulationChange> production_change;std::vector<MomentOwnerSource> production_changes;
    if(catalog.exact_fact_clock_enabled){
        std::vector<StableId> cohorts;
        for(const auto& id:input.existing_activity_ids){const auto* a=s.activities.find(id);if(!a||!active_activity(*a)||a->started_tick>input.from_tick||a->execution.mode!=ExecutionMode::background||a->kind!=OrgActivityKind::population_training||input.to_tick<a->due_tick)continue;for(const auto& p:a->participation)if(p.binding_kind==ParticipationKind::cohort_allocation&&std::find(cohorts.begin(),cohorts.end(),p.subject.id)==cohorts.end())cohorts.push_back(p.subject.id);}
        if(!cohorts.empty()){
            if(!production_population)return {false,{},common_failure("OrganizationProductionPopulationContextRequired",{},ErrorKind::unsupported_capability).error};
            auto same=production_population_candidate(*production_population,c,s,catalog,economy,now.value);if(!same.ok)return {false,{},same.error};
            auto prepared=prepare_production_population_change(*production_population,cohorts);if(!prepared.ok)return {false,{},prepared.error};production_change.emplace(std::move(prepared.value));
        }
    }
    local_economy_detail::PhaseSavepoint guard(c);if(!guard.token().ok)return {false,{},guard.token().error};auto next=s;auto next_economy=economy;OrganizationEffects effects;
    for(const auto& id:input.existing_activity_ids){auto* activity=next.activities.find(id);if(!activity||!active_activity(*activity)||activity->started_tick>input.from_tick||activity->execution.mode!=ExecutionMode::background)continue;const auto* terms=catalog.activity_terms.find(activity->terms_id);if(!terms)return {false,{},common_failure("UnknownOrganizationActivityTerms",id).error};if(!matching_executors(*activity))return {false,{},common_failure("OrganizationExecutorBindingMismatch",activity->id,ErrorKind::internal).error};bool ready=input.to_tick>=activity->due_tick;bool successful=true;StableId terminal_code;std::vector<StableId> causes;
        for(const auto& p:activity->participation)if(p.binding_kind==ParticipationKind::personal_action){if(const auto* action=c.action(p.binding_id)){if(!results.supports_action_result(action->kind))return {false,{},common_failure("UnsupportedOrganizationActionResult",action->kind,ErrorKind::unsupported_capability).error};ready=false;continue;}auto result=results.action_result(p.binding_id);if(!result)return {false,{},common_failure("MissingOrganizationActionResult",p.binding_id,ErrorKind::internal).error};if(result->action_id!=p.binding_id||!results.supports_action_result(result->action_kind)||result->source_fact_id.empty()||!c.state().facts.find(result->source_fact_id)||result->completed_tick<activity->started_tick||result->completed_tick>input.to_tick)return {false,{},common_failure("InvalidOrganizationActionResult",p.binding_id,ErrorKind::internal).error};auto bound=terminal_participant(c,*result,p.subject);if(!bound.ok){bound.error.kind=ErrorKind::internal;return {false,{},bound.error};}if(!has(causes,result->source_fact_id))causes.push_back(result->source_fact_id);if(!result->success){if(successful||terminal_code.empty())terminal_code=result->code;successful=false;}else if(successful&&terminal_code.empty())terminal_code=result->code;}
        if(!ready)continue;
        if(catalog.exact_fact_clock_enabled){
            bool personal=false;for(const auto& p:activity->participation)personal|=p.binding_kind==ParticipationKind::personal_action;
            if(!personal){
                if(!calendar)return {false,{},common_failure("OrganizationCalendarAdmissionReadRequired",id,ErrorKind::unsupported_capability).error};
                const OrganizationFact* approved=nullptr;for(const auto& row:next.facts.rows())if(row.second.activity_id==id&&(row.second.kind=="organization_activity_approved"||row.second.kind=="population_training_approved")){if(approved)return {false,{},common_failure("DuplicateOrganizationActivityApproval",id).error};approved=&row.second;}
                if(!approved)return {false,{},common_failure("OrganizationCalendarApprovalSourceRequired",id,ErrorKind::unsupported_capability).error};
                SourceRef source{"organization",approved->kind,approved->id};auto admitted=calendar->read_original_calendar_window(source,terms->duration_ticks);if(!admitted.ok)return {false,{},admitted.error};const auto& window=admitted.value.approved_window;
                if(activity->started_tick!=window.from_tick||activity->phase_started_tick!=window.from_tick||activity->due_tick!=window.until_tick||input.to_tick!=window.until_tick)return {false,{},common_failure("OrganizationCalendarActivityWindowMismatch",id).error};
                auto coverage=calendar->validate_completed_calendar_window(source,terms->duration_ticks);if(!coverage.ok)return {false,{},coverage.error};
                if(activity->kind==OrgActivityKind::population_training){if(!training)return {false,{},common_failure("OrganizationTrainingOriginalCoverageRequired",id,ErrorKind::unsupported_capability).error};coverage=training->validate_original_training_window(*activity,*terms,source,window.from_tick,window.until_tick);if(!coverage.ok)return {false,{},coverage.error};}
            }
        }
        if(activity->outcome_sequence==INT64_MAX)return {false,{},common_failure("OrganizationSequenceOverflow",id).error};OrganizationFact fact;fact.kind=!successful?"organization_activity_failed":activity->kind==OrgActivityKind::population_training?"population_training_completed":"organization_activity_completed";fact.code=terminal_code;fact.organization_id=activity->principal.id;fact.activity_id=id;fact.tick=input.to_tick;fact.cause_fact_ids=causes;
        if(activity->kind==OrgActivityKind::population_training){for(const auto& p:activity->participation)if(p.binding_kind==ParticipationKind::cohort_allocation){auto* cohort=next_economy.cohorts.find(p.subject.id);auto* allocation=next_economy.labor_allocations.find(p.binding_id);if(!cohort||!allocation||cohort->status!=PopulationStatus::training||cohort->assignment_id!=std::optional<StableId>(id)||allocation->activity_or_production_id!=id)return {false,{},common_failure("InvalidTrainingContinuity",id).error};cohort->status=PopulationStatus::trained_reserve;cohort->assignment_id.reset();if(!add(fact.amount,cohort->headcount,fact.amount))return {false,{},common_failure("EconomyQuantityOverflow",id).error};next_economy.labor_allocations.erase(p.binding_id);}}
        auto completed=append_org_fact(c,next,std::move(fact),catalog,clock);if(!completed.ok)return {false,{},completed.error};activity=next.activities.find(id);activity->status=successful?OrgActivityStatus::completed:OrgActivityStatus::failed;activity->phase=successful?"completed":"failed";activity->result_code=terminal_code;++activity->outcome_sequence;activity->result_fact_ids.push_back(completed.value);activity->execution.last_receipt_id=completed.value;activity->execution.phase=activity->phase;effects.fact_ids.push_back(completed.value);effects.receipt_ids.push_back(completed.value);if(catalog.exact_fact_clock_enabled&&activity->kind==OrgActivityKind::population_training&&successful)production_changes.push_back({{"organization","population_training_completed",completed.value},1});
    }auto committed=guard.commit();if(!committed.ok)return {false,{},committed.error};s=std::move(next);economy=std::move(next_economy);
    if(!production_changes.empty()){if(!production_change)return {false,{},common_failure("OrganizationProductionPopulationPreparedSourceRequired").error};auto changed=apply_production_population_change_in_candidate(*production_population,std::move(*production_change),production_changes);if(!changed.ok)return {false,{},changed.error};}
    return {true,std::move(effects),{}};
}
// Original entry keeps its signature and legacy behavior. Full selected training
// cannot silently omit its original production mutation consumer.
Outcome<OrganizationEffects> step_organization_activities(CommonStore& c,OrganizationState& s,const OrganizationCatalog& catalog,LocalEconomyState& economy,const ActivityRead& access,const DynamicResultRead& results,const DynamicStepInput& input,const OrganizationAuthorityClock* clock,const OrganizationCalendarAdmissionRead* calendar,const OrganizationTrainingCoverageRead* training){return step_organization_activities(c,s,catalog,economy,access,results,input,clock,calendar,training,nullptr);}

// 保留公共短签名，复用已有的授权校验，不复制或放宽授权规则。
// 调用者未提供精确读钟时，仍由原实现拒绝需要该来源的配置。
Outcome<AuthorityProof> check_authority(const CommonStore &common,
                                        const OrganizationState &state,
                                        const OrganizationCatalog &catalog,
                                        const ActingAuthority &acting,
                                        const StableId &command_kind,
                                        const ScopeRef &scope, Integer amount) {
  return check_authority(common, state, catalog, acting, command_kind, scope,
                         amount, nullptr);
}

// 公共成员预览仅转交原有只读校验，不生成事实或改写成员状态。
// 缺少精确读钟时保留原失败结果，不凭 tick 构造新的时间来源。
Status preview_organization_member_command(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationMemberCommand &command,
    Integer tick) {
  return preview_organization_member_command(common, state, catalog, economy,
                                             activity, command, tick, nullptr);
}

// 公共招募预览复用原有容量、同意和劳力校验，不补造训练结果。
// 未提供读钟和日历准入来源时，原实现的能力拒绝继续生效。
Status preview_organization_recruit_command(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationRecruitCommand &command,
    Integer tick) {
  return preview_organization_recruit_command(common, state, catalog, economy,
                                              activity, command, tick, nullptr,
                                              nullptr);
}

// 公共活动预览把真实结果读口原样交给已有实现，不代替原域判断完成。
// 缺少精确时钟或日历来源的失败原样返回，不走降级成功分支。
Status preview_organization_activity_command(
    const CommonStore &common, const OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationActivityCommand &command,
    Integer tick, const DynamicResultRead *results) {
  return preview_organization_activity_command(common, state, catalog, economy,
                                               activity, command, tick, results,
                                               nullptr, nullptr);
}

// 公共成员命令沿用唯一成员 writer，不新增响应、离职或服务变更规则。
// 没有运行来源上下文时，由原实现决定拒绝，不伪造权源或读版本。
Outcome<OrganizationEffects> apply_organization_member_command(
    CommonStore &common, OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationMemberCommand &command,
    Integer tick) {
  return apply_organization_member_command(common, state, catalog, economy,
                                           activity, command, tick, nullptr);
}

// 公共招募命令只转交已有招募 writer，人口和劳力仍由原规则处理。
// 不补造人口修订上下文，缺少真实来源时保留原失败与回滚边界。
Outcome<OrganizationEffects> apply_organization_recruit_command(
    CommonStore &common, OrganizationState &state,
    const OrganizationCatalog &catalog, LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationRecruitCommand &command,
    Integer tick) {
  return apply_organization_recruit_command(common, state, catalog, economy,
                                            activity, command, tick, nullptr);
}

// 公共活动命令复用已有活动 writer，并透传真实结果读口。
// 不构造缺失的运行来源，原校验失败不会被转换为批准或成功。
Outcome<OrganizationEffects> apply_organization_activity_command(
    CommonStore &common, OrganizationState &state,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const ActivityRead &activity, const OrganizationActivityCommand &command,
    Integer tick, const DynamicResultRead *results) {
  return apply_organization_activity_command(common, state, catalog, economy,
                                             activity, command, tick, results,
                                             nullptr);
}

// 公共推进入口复用完整原实现，不复制活动完成或训练结算逻辑。
// 精确模式所需读钟、日历、训练及人口来源缺失时仍明确失败。
Outcome<OrganizationEffects> step_organization_activities(
    CommonStore &common, OrganizationState &state,
    const OrganizationCatalog &catalog, LocalEconomyState &economy,
    const ActivityRead &activity, const DynamicResultRead &results,
    const DynamicStepInput &input) {
  return step_organization_activities(common, state, catalog, economy, activity,
                                      results, input, nullptr, nullptr, nullptr,
                                      nullptr);
}

} // 未实现局部/后台切换明确拒绝，活动不会形成另一份士兵数量或战力。
