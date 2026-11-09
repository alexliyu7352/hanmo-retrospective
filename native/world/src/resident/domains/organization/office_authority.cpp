// 原任期事实是唯一历史；本叶只读，不创建平行Office/Grant状态或知情记录。
#include "office_authority.hpp"
#include "grant_authority.hpp"
#include "organization_office_history.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
bool same_office_source(const OfficeAuthoritySource &a,
                        const OfficeAuthoritySource &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *initial = std::get_if<AuthoredOfficeAuthoritySource>(&a)) {
    const auto &other = std::get<AuthoredOfficeAuthoritySource>(b);
    return initial->office_id == other.office_id &&
           initial->initial_epoch == other.initial_epoch &&
           same_catalog(initial->catalog_identity, other.catalog_identity);
  }
  return std::get<OfficeChangedAuthoritySource>(a).fact_id ==
         std::get<OfficeChangedAuthoritySource>(b).fact_id;
}
bool same_grant_source(const GrantAuthoritySource &a,
                       const GrantAuthoritySource &b) {
  if (a.index() != b.index())
    return false;
  if (const auto *initial = std::get_if<AuthoredGrantAuthoritySource>(&a)) {
    const auto &other = std::get<AuthoredGrantAuthoritySource>(b);
    return initial->grant_id == other.grant_id &&
           same_catalog(initial->catalog_identity, other.catalog_identity);
  }
  const auto &x = std::get<GrantRevisionAuthoritySource>(a);
  const auto &y = std::get<GrantRevisionAuthoritySource>(b);
  return x.fact_id == y.fact_id && x.grant_id == y.grant_id &&
         x.revision == y.revision;
}
bool same_selected_grant(const Grant &a, const Grant &b) {
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
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool same_scope(const ScopeRef& a,const ScopeRef& b){return a.kind==b.kind&&a.id==b.id;}
bool contains(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
// 当前/历史读都重放同一原事实序；先验证完整链末态，再投影目标边界。
Outcome<OfficeTermRead> read_term(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const StableId& id,const WorldMoment& at,const StableId& boundary,const OrganizationAuthorityClock* clock){
    const auto fail=[&](const StableId& code)->Outcome<OfficeTermRead>{return {false,{},common_failure(code,id).error};};
    auto now=read_organization_authority_now(c,clock);if(!now.ok)return {false,{},now.error};auto future=compare_world_moments(at,now.value);if(!future.ok)return {false,{},future.error};
    const auto* initial=catalog.offices.find(id);const auto* current=s.offices.find(id);if(!initial||!current||initial->organization_id!=current->organization_id||identity.content_digest!=c.state().metadata.content_digest||future.value>0)return fail("InvalidOfficeTermSource");
    const auto& order=c.state().facts.order();const auto limit=boundary.empty()?order.end():std::find(order.begin(),order.end(),boundary);if(!boundary.empty()&&(limit==order.end()||!c.state().facts.find(boundary)))return fail("OfficeTermBoundaryRequired");if(boundary.empty()&&future.value!=0)return fail("OfficeTermCurrentTimeMismatch");
    OfficeTermRead out;out.source_identity=c.state().identity;out.organization_id=initial->organization_id;out.office_id=id;out.epoch=initial->epoch;out.holder_actor_id=initial->holder_actor_id;out.status=initial->status;out.source=AuthoredOfficeAuthoritySource{identity,id,initial->epoch};out.effective_at.kind=ActorFoundationAt::Kind::legacy_tick;out.effective_at.legacy_tick=0;
    Integer epoch=initial->epoch;WorldMoment previous{};auto holder=initial->holder_actor_id;
    for(auto it=order.begin();it!=order.end();++it){const auto* f=s.facts.find(*it);if(!f||f->kind!="organization_office_changed"||f->office_id!=id)continue;const auto* head=c.state().facts.find(*it);
        const auto occurred=f->office_change_payload?f->office_change_payload->effective_moment:WorldMoment{f->tick,{}};auto before=compare_world_moments(previous,occurred);auto occurred_now=compare_world_moments(occurred,now.value);auto target=compare_world_moments(occurred,at);
        if(!before.ok)return {false,{},before.error};if(!occurred_now.ok)return {false,{},occurred_now.error};if(!target.ok)return {false,{},target.error};
        if(f->id!=*it||!head||head->kind!=f->kind||head->domain!="organization"||f->organization_id!=initial->organization_id||before.value>0||occurred_now.value>0||epoch==std::numeric_limits<Integer>::max()||(!f->actor_id.empty()&&!c.actor(f->actor_id)))return fail("InvalidOfficeTermHistory");
        for(const auto& source:f->cause_fact_ids){const auto source_at=std::find(order.begin(),order.end(),source);if(source_at==order.end()||source_at>=it)return fail("InvalidOfficeTermCauseOrder");}
        auto shape=validate_office_change_payload_shape(c,catalog,*f,epoch,holder);if(!shape.ok)return {false,{},shape.error};++epoch;holder=f->actor_id.empty()?std::nullopt:std::optional<StableId>{f->actor_id};previous=occurred;
        if(it<limit){if(target.value>0)return fail("OfficeTermHistoricalTimeMismatch");out.epoch=epoch;out.holder_actor_id=holder;out.status=holder?OfficeStatus::active:OfficeStatus::vacant;out.source=OfficeChangedAuthoritySource{f->id};if(f->office_change_payload){out.effective_at.kind=ActorFoundationAt::Kind::exact_moment;out.effective_at.exact_moment=occurred;}else {out.effective_at.kind=ActorFoundationAt::Kind::legacy_tick;out.effective_at.legacy_tick=f->tick;}}else if(target.value<0)return fail("OfficeTermHistoricalTimeMismatch");
    }
    for(const auto& row:s.facts.rows())if(row.second.kind=="organization_office_changed"&&row.second.office_id==id&&std::count(order.begin(),order.end(),row.first)!=1)return fail("OrphanOfficeTermFact");
    if(epoch!=current->epoch||holder!=current->holder_actor_id||(holder&&current->status!=OfficeStatus::active)||(!holder&&current->status==OfficeStatus::active))return fail("OfficeTermCurrentMismatch");return {true,std::move(out),{}};
}
} // namespace
// Binding survives a later observation; request equality also checks the exact
// current ticket.
bool same_authority_grant_binding(const AuthorityProof &a,
                                  const AuthorityProof &b) {
  return a.schema_version == 2 && b.schema_version == 2 &&
         a.organization_id == b.organization_id && a.office_id == b.office_id &&
         a.actor_id == b.actor_id && a.command_kind == b.command_kind &&
         same_scope(a.scope, b.scope) &&
         a.authority_epoch == b.authority_epoch && a.amount == b.amount &&
         a.source_identity.world_id == b.source_identity.world_id &&
         a.grant_id == b.grant_id && a.grant_revision == b.grant_revision &&
         same_office_source(a.office_source, b.office_source) &&
         same_grant_source(a.grant_source, b.grant_source);
}
bool same_authority_proof(const AuthorityProof &a, const AuthorityProof &b) {
  if (!same_authority_grant_binding(a, b) ||
      !same_identity(a.source_identity, b.source_identity) ||
      a.candidate_revision != b.candidate_revision ||
      a.resource_versions.size() != b.resource_versions.size())
    return false;
  auto moment = compare_world_moments(a.observed_at, b.observed_at);
  if (!moment.ok || moment.value != 0)
    return false;
  for (std::size_t i = 0; i < a.resource_versions.size(); ++i) {
    const auto &x = a.resource_versions[i];
    const auto &y = b.resource_versions[i];
    if (x.resource.kind != y.resource.kind || x.resource.id != y.resource.id ||
        x.version != y.version)
      return false;
  }
  return true;
}
// 资源存在/项目目录来自Common，量纲实际签发内容稍后由AuthorityIssueSourceRead反核。
Status validate_authority_amount_components(const CommonStore& c,const std::vector<AuthorityAmountComponent>& rows,const std::vector<ResourceKey>& resources,Integer limit){
    if(rows.empty()||rows.size()>128||resources.empty()||resources.size()>128||limit<0)return common_failure("InvalidAuthorityAmountComponents");
    for(std::size_t i=0;i<rows.size();++i){const auto* money=std::get_if<MoneyAuthorityAmount>(&rows[i]);const auto& unit=money?money->currency_id:std::get<ItemAuthorityAmount>(rows[i]).item_id;const auto& ids=money?money->wallet_ids:std::get<ItemAuthorityAmount>(rows[i]).inventory_ids;const auto amount=money?money->amount:std::get<ItemAuthorityAmount>(rows[i]).amount;const auto kind=money?ResourceKey::Kind::wallet:ResourceKey::Kind::inventory;
        if(unit.empty()||amount<0||amount>limit||ids.empty()||ids.size()>128||(money?unit!="qian":!c.item_definition(unit)))return common_failure("AuthorityAmountUnitDenied",unit);
        for(std::size_t j=0;j<i;++j)if(rows[j].index()==rows[i].index()&&(money?std::get<MoneyAuthorityAmount>(rows[j]).currency_id:std::get<ItemAuthorityAmount>(rows[j]).item_id)==unit)return common_failure("DuplicateAuthorityAmountUnit",unit);
        for(std::size_t n=0;n<ids.size();++n){const auto& id=ids[n];if(id.empty()||std::find(ids.begin(),ids.begin()+n,id)!=ids.begin()+n||std::none_of(resources.begin(),resources.end(),[&](const auto& r){return r.kind==kind&&r.id==id;})||(money?!c.wallet(id):!c.inventory(id)))return common_failure("AuthorityAmountResourceMismatch",id);}
    }
    for(const auto& resource:resources){bool covered=false;for(const auto& row:rows){if(const auto* money=std::get_if<MoneyAuthorityAmount>(&row)){if(resource.kind==ResourceKey::Kind::wallet&&contains(money->wallet_ids,resource.id))covered=true;}else if(resource.kind==ResourceKey::Kind::inventory&&contains(std::get<ItemAuthorityAmount>(row).inventory_ids,resource.id))covered=true;}if(!covered)return common_failure("AuthorityAmountResourceMissing",resource.id);}
    return common_success();
}
bool same_authority_amount_components(const std::vector<AuthorityAmountComponent>& a,const std::vector<AuthorityAmountComponent>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i){if(a[i].index()!=b[i].index())return false;if(const auto* x=std::get_if<MoneyAuthorityAmount>(&a[i])){const auto& y=std::get<MoneyAuthorityAmount>(b[i]);if(x->currency_id!=y.currency_id||x->wallet_ids!=y.wallet_ids||x->amount!=y.amount)return false;}else {const auto& item_a=std::get<ItemAuthorityAmount>(a[i]);const auto& item_b=std::get<ItemAuthorityAmount>(b[i]);if(item_a.item_id!=item_b.item_id||item_a.inventory_ids!=item_b.inventory_ids||item_a.amount!=item_b.amount)return false;}}return true;}
Outcome<bool> authority_window_contains(Integer from,const std::optional<Integer>& until,const WorldMoment& at){if(from<0||(until&&*until<from))return {false,false,common_failure("InvalidAuthorityWindow").error};auto lower=compare_world_moments(at,WorldMoment{from,{}});if(!lower.ok)return {false,false,lower.error};if(lower.value<0)return {true,false,{}};if(until){auto upper=compare_world_moments(at,WorldMoment{*until,{}});if(!upper.ok)return {false,false,upper.error};if(upper.value>0)return {true,false,{}};}return {true,true,{}};}
Outcome<WorldMoment> read_organization_authority_now(const CommonStore& c,const OrganizationAuthorityClock* clock){auto out=clock?clock->read_organization_authority_now():Outcome<WorldMoment>{true,WorldMoment{c.tick(),{}},{}};if(!out.ok)return out;auto valid=encode_world_moment(out.value);if(!valid.ok)return {false,{},valid.error};if(out.value.half_day_tick<c.tick())return {false,{},common_failure("OrganizationAuthorityClockBeforeWorld").error};return out;}
Outcome<OfficeTermRead> read_current_office_term(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const StableId& office,const OrganizationAuthorityClock* clock){auto now=read_organization_authority_now(c,clock);if(!now.ok)return {false,{},now.error};return read_term(c,s,catalog,identity,office,now.value,{},clock);}
Outcome<OfficeTermRead> read_office_term_before_fact(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const StableId& office,Integer tick,const StableId& boundary,const OrganizationAuthorityClock* clock){if(boundary.empty())return {false,{},common_failure("OfficeTermBoundaryRequired").error};return read_term(c,s,catalog,identity,office,WorldMoment{tick,{}},boundary,clock);}
Outcome<OfficeTermRead> read_office_term_before_fact_at(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const StableId& office,const WorldMoment& at,const StableId& boundary,const OrganizationAuthorityClock& clock){if(boundary.empty())return {false,{},common_failure("OfficeTermBoundaryRequired").error};return read_term(c,s,catalog,identity,office,at,boundary,&clock);}
namespace {
Status request_shape(const CommonStore& c,const OrganizationCatalog& catalog,const SingleGrantAuthorityRequest& r){
 if(r.command_kind.empty()||r.amount<0||r.resources.size()>128)return common_failure("InvalidSingleGrantAuthorityRequest");
 for(std::size_t i=0;i<r.resources.size();++i){const auto& resource=r.resources[i];if(resource.id.empty()||(resource.kind!=ResourceKey::Kind::wallet&&resource.kind!=ResourceKey::Kind::inventory))return common_failure("InvalidAuthorityResourceSelection");for(std::size_t j=0;j<i;++j)if(r.resources[j].kind==resource.kind&&r.resources[j].id==resource.id)return common_failure("InvalidAuthorityResourceSelection");}
 if(!r.amount_components.empty()){if(!catalog.authority_lifecycle_enabled||r.amount!=0)return common_failure("AuthorityAmountProfileMismatch");return validate_authority_amount_components(c,r.amount_components,r.resources,std::numeric_limits<Integer>::max());}return common_success();
}
bool covers_request(const CommonStore& c,const Grant& g,const SingleGrantAuthorityRequest& r){
 if(g.office_id!=r.acting.office_id||!contains(g.allowed_command_kinds,r.command_kind)||r.amount>g.amount_limit||(r.requires_delegation&&!g.may_delegate)||std::none_of(g.scopes.begin(),g.scopes.end(),[&](const auto& scope){return same_scope(scope,r.scope);}))return false;
 for(const auto& resource:r.resources)if(!contains(resource.kind==ResourceKey::Kind::wallet?g.wallet_ids:g.inventory_ids,resource.id))return false;
 return r.amount_components.empty()||validate_authority_amount_components(c,r.amount_components,r.resources,g.amount_limit).ok;
}
}
// Single Grant and Proof /2 share the same original sources and revision.
Outcome<SingleGrantAuthorityRead> check_current_grant_authority_requirements(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const CatalogIdentity &identity,
    const SingleGrantAuthorityRequest &request,
    std::span<const SingleGrantAuthorityRequest> additional,
    const OrganizationAuthorityClock *clock) {
  if (!same_catalog(identity, catalog.authority_catalog_identity))
    return {false,
            {},
            common_failure("AuthorityProofCatalogIdentityMismatch").error};
  auto valid = request_shape(c, catalog, request);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &extra : additional) {
    valid = request_shape(c, catalog, extra);
    if (!valid.ok)
      return {false, {}, valid.error};
    if (extra.acting.organization_id != request.acting.organization_id ||
        extra.acting.office_id != request.acting.office_id ||
        extra.acting.actor_id != request.acting.actor_id ||
        extra.acting.authority_epoch != request.acting.authority_epoch)
      return {false,
              {},
              common_failure("SingleGrantRequirementsIssuerMismatch").error};
  }
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  auto term = read_current_office_term(c, s, catalog, identity,
                                       request.acting.office_id, clock);
  if (!term.ok)
    return {false, {}, term.error};
  const auto *office = s.offices.find(request.acting.office_id);
  if (!office)
    return {false,
            {},
            common_failure("UnknownOffice", request.acting.office_id).error};
  for (const auto &id : office->grant_ids) {
    auto runtime = read_current_grant(c, s, catalog, id);
    if (!runtime.ok) {
      if (runtime.error.kind != ErrorKind::rule)
        return {false, {}, runtime.error};
      continue;
    }
    const auto &grant = runtime.value.terms;
    auto window = authority_window_contains(grant.valid_from, grant.valid_until,
                                            now.value);
    if (!window.ok)
      return {false, {}, window.error};
    if (!window.value || !covers_request(c, grant, request))
      continue;
    bool all = true;
    for (const auto &extra : additional)
      if (!covers_request(c, grant, extra)) {
        all = false;
        break;
      }
    if (!all)
      continue;
    auto proof = check_authority_with_single_grant(
        c, s, catalog, request.acting, request.command_kind, request.scope,
        request.amount, grant, clock);
    if (!proof.ok)
      return {false, {}, proof.error};
    for (const auto &extra : additional) {
      auto actual = check_authority_with_single_grant(
          c, s, catalog, extra.acting, extra.command_kind, extra.scope,
          extra.amount, grant, clock);
      if (!actual.ok)
        return {false, {}, actual.error};
    }
    SingleGrantAuthorityRead out;
    out.source_identity = c.state().identity;
    out.authority = std::move(proof.value);
    out.office_source = term.value.source;
    out.grant_source =
        runtime.value.revision == 0
            ? GrantAuthoritySource{AuthoredGrantAuthoritySource{identity, id}}
            : GrantAuthoritySource{GrantRevisionAuthoritySource{
                  runtime.value.last_change_fact_id, id,
                  runtime.value.revision}};
    out.grant_revision = runtime.value.revision;
    out.effective_grant = std::move(runtime.value.terms);
    out.amount_components = request.amount_components;
    return {true, std::move(out), {}};
  }
  return {false, {}, common_failure("SingleGrantAuthorityDenied").error};
}
Outcome<SingleGrantAuthorityRead> check_current_grant_authority(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const CatalogIdentity& identity,const SingleGrantAuthorityRequest& request,const OrganizationAuthorityClock* clock){return check_current_grant_authority_requirements(c,s,catalog,identity,request,{},clock);}
Status validate_current_grant_authority_requirements(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const CatalogIdentity &identity,
    const SingleGrantAuthorityRequest &request,
    std::span<const SingleGrantAuthorityRequest> additional,
    const SingleGrantAuthorityRead &accepted,
    const OrganizationAuthorityClock *clock) {
  if (!same_authority_amount_components(request.amount_components,
                                        accepted.amount_components) ||
      !same_identity(c.state().identity, accepted.source_identity))
    return common_failure("StaleSingleGrantAuthority");
  auto now = check_current_grant_authority_requirements(
      c, s, catalog, identity, request, additional, clock);
  if (!now.ok)
    return {false, {}, now.error};
  if (!same_authority_proof(now.value.authority, accepted.authority) ||
      !same_office_source(now.value.office_source, accepted.office_source) ||
      !same_grant_source(now.value.grant_source, accepted.grant_source) ||
      !same_selected_grant(now.value.effective_grant,
                           accepted.effective_grant) ||
      now.value.grant_revision != accepted.grant_revision ||
      now.value.authority.authority_epoch !=
          accepted.authority.authority_epoch ||
      now.value.authority.resource_versions.size() !=
          accepted.authority.resource_versions.size())
    return common_failure("StaleSingleGrantAuthority");
  for (std::size_t i = 0; i < accepted.authority.resource_versions.size();
       ++i) {
    const auto &a = accepted.authority.resource_versions[i];
    const auto &b = now.value.authority.resource_versions[i];
    if (a.resource.kind != b.resource.kind || a.resource.id != b.resource.id ||
        a.version != b.version)
      return common_failure("StaleSingleGrantAuthority");
  }
  return common_success();
}
Status validate_single_grant_authority(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const CatalogIdentity &identity,
    const SingleGrantAuthorityRequest &request,
    const SingleGrantAuthorityRead &accepted,
    const OrganizationAuthorityClock *clock) {
  if (!same_authority_amount_components(request.amount_components,
                                        accepted.amount_components) ||
      !same_identity(c.state().identity, accepted.source_identity))
    return common_failure("StaleSingleGrantAuthority");
  auto now =
      check_current_grant_authority(c, s, catalog, identity, request, clock);
  if (!now.ok)
    return {false, {}, now.error};
  if (!same_authority_proof(now.value.authority, accepted.authority) ||
      !same_office_source(now.value.office_source, accepted.office_source) ||
      !same_grant_source(now.value.grant_source, accepted.grant_source) ||
      !same_selected_grant(now.value.effective_grant,
                           accepted.effective_grant) ||
      now.value.grant_revision != accepted.grant_revision ||
      now.value.authority.authority_epoch !=
          accepted.authority.authority_epoch ||
      now.value.authority.resource_versions.size() !=
          accepted.authority.resource_versions.size())
    return common_failure("StaleSingleGrantAuthority");
  for (std::size_t i = 0; i < accepted.authority.resource_versions.size();
       ++i) {
    const auto &a = accepted.authority.resource_versions[i];
    const auto &b = now.value.authority.resource_versions[i];
    if (a.resource.kind != b.resource.kind || a.resource.id != b.resource.id ||
        a.version != b.version)
      return common_failure("StaleSingleGrantAuthority");
  }
  return common_success();
}
} // namespace hanmo::resident
