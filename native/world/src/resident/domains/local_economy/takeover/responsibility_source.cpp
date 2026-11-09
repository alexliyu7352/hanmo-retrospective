// 只引用原目录/事实；现公库已换Owner不改变历史责任主体。
#include "responsibility_history.hpp"
#include "../authored_obligation_read.hpp"
#include "resident/domains/military/siege_model.hpp"
#include "resident/domains/military/occupation_model.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/relief/public_relief_rules.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include "resident/domains/estate/employment/estate_employment.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
namespace hanmo::resident {
namespace {
using Result=Outcome<EconomicResponsibilitySource>;
Result fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
bool fact_is(const CommonStore& c,const StableId& id,const char* kind,const char* domain="local_economy"){
    const auto* fact=c.state().facts.find(id);return fact&&fact->id==id&&fact->kind==kind&&fact->domain==domain;
}
Status append_fact_source(const CommonStore& c,EconomicResponsibilitySource& out,const StableId& id,const char* kind,const char* domain="local_economy"){
    if(!fact_is(c,id,kind,domain))return common_failure("InvalidEconomicResponsibilityFact",id);
    if(std::find(out.source_fact_ids.begin(),out.source_fact_ids.end(),id)==out.source_fact_ids.end())out.source_fact_ids.push_back(id);
    return common_success();
}
bool subject_exists(const CommonStore& c,const LocalEconomyState& s,const SubjectRef& subject){
    if(subject.id.empty())return false;
    switch(subject.kind){
    case SubjectKind::actor:return c.actor(subject.id)!=nullptr; // 死亡不删除已发生责任。
    case SubjectKind::cohort:return s.cohorts.find(subject.id)!=nullptr; // 0人原容器仍保留。
    case SubjectKind::organization:return true; // 原机构存在性由完整组织validator证明，不要求现在active。
    default:return false;
    }
}
void container(EconomicResponsibilitySource& out,const StableId& id,bool inventory,EconomicContainerRole role,const StableId& item={}){
    if(!id.empty())out.containers.push_back({id,inventory,role,item});
}
Status production(const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const StableId& id,EconomicResponsibilitySource& out){
    const auto* unit=state.production_units.find(id);const auto* original=catalog.production_units.find(id);
    if(!unit||!original||unit->id!=id||original->id!=id||unit->site_id!=original->site_id||unit->input_inventory_ids!=original->input_inventory_ids||unit->output_inventory_id!=original->output_inventory_id)return common_failure("InvalidEconomicProductionSource",id);
    out.site_id=original->site_id;out.original_operating_owner=original->owner;
    out.authored_sources.push_back({"production_units",id,{}});
    for(const auto& input:original->input_inventory_ids)container(out,input,true,EconomicContainerRole::production_input);
    container(out,original->output_inventory_id,true,EconomicContainerRole::production_output);
    if(catalog.production_management){
        const auto* plan=catalog.production_management->plans.find(id);
        if(plan){
            if(plan->unit_id!=id)return common_failure("InvalidEconomicProductionPlan",id);
            out.authored_sources.push_back({"production_management.plans",id,{}});
            std::set<StableId> seen;
            for(const auto& use:plan->asset_uses){
                const auto* asset=catalog.production_management->assets.find(use.asset_id);
                if(!asset||asset->id!=use.asset_id||asset->site_id!=original->site_id||!seen.insert(asset->id).second)return common_failure("InvalidEconomicProductionAsset",use.asset_id);
                out.production_assets.push_back({asset->id,asset->source_key,asset->owner});
            }
        }
    }
    return common_success();
}
// 原目录义务与运行行共用这份来源校验；不比较可随真实时间推进的到期游标或已付字段。
Status authored_obligation_matches(const RecurringObligation &authored,
                                   const RecurringObligation &value,
                                   const StableId &id) {
  if (authored.id != id || !same_subject(authored.principal, value.principal) ||
      !same_subject(authored.beneficiary, value.beneficiary) ||
      authored.kind != value.kind ||
      authored.source_wallet_or_inventory != value.source_wallet_or_inventory ||
      authored.destination != value.destination ||
      authored.authority_ref != value.authority_ref)
    return common_failure("InvalidEconomicObligationSource", id);
  return common_success();
}
Status obligation(const CommonStore &common, const LocalEconomyState &state,
                  const LocalEconomyCatalog &catalog, const StableId &id,
                  EconomicResponsibilitySource &out) {
  const auto *value = state.obligations.find(id);
  if (!value || value->id != id)
    return common_failure("UnknownEconomicObligation", id);
  if (const auto *authored = catalog.obligations.find(id)) {
    auto valid = authored_obligation_matches(*authored, *value, id);
    if (!valid.ok)
      return valid;
    out.authored_sources.push_back({"obligations", id, {}});
  } else {
    const auto *binding =
        state.membership_wage_bindings.find(value->authority_ref);
    if (!binding || binding->id != value->authority_ref ||
        binding->obligation_id != id || value->kind != ObligationKind::wage ||
        value->principal.kind != SubjectKind::organization ||
        value->principal.id != binding->organization_id ||
        value->beneficiary.kind != SubjectKind::actor ||
        value->beneficiary.id != binding->actor_id ||
        value->source_wallet_or_inventory != binding->payer_wallet_id ||
        value->destination != binding->payee_wallet_id)
      return common_failure("InvalidEconomicMembershipObligation", id);
    auto fact = append_fact_source(common, out, binding->decision_id,
                                   "membership_decided", "organization");
    if (!fact.ok)
      return fact;
    out.authored_sources.push_back(
        {"organization.membership_terms", binding->terms_id, {}});
  }
  if (!subject_exists(common, state, value->principal) ||
      !subject_exists(common, state, value->beneficiary))
    return common_failure("InvalidEconomicResponsibilitySubject", id);
  out.obligation_id = id;
  out.original_principal = value->principal;
  out.original_beneficiaries = {value->beneficiary};
  // 群体日后可能迁移，不能把当前驻地填成原义务的发生地点。
  if (value->beneficiary.kind == SubjectKind::cohort)
    if (const auto *original = catalog.cohorts.find(value->beneficiary.id))
      out.site_id = original->site_id;
  const bool ration = value->kind == ObligationKind::ration;
  container(out, value->source_wallet_or_inventory, ration,
            EconomicContainerRole::payment,
            ration ? value->item_id : StableId{});
  container(out, value->destination, ration, EconomicContainerRole::receipt,
            ration ? value->item_id : StableId{});
  return common_success();
}
} // namespace
// S6 原初始责任只可引用真实已安装 authored
// obligation；此口不经过或伪造占领清单。
Outcome<const RecurringObligation *> read_economic_authored_obligation(
    const CommonStore &common, const LocalEconomyState &state,
    const LocalEconomyCatalog &catalog, const SourceRef &ref,
    const StableId &organization_id) {
  auto rejected =
      [&](const StableId &code) -> Outcome<const RecurringObligation *> {
    return {false, nullptr, common_failure(code, ref.id).error};
  };
  if (!catalog.enabled)
    return rejected("EconomicResponsibilityDisabled");
  if (ref.domain != "local_economy" || ref.kind != "authored_obligation" ||
      ref.id.empty() || organization_id.empty())
    return rejected("EconomicAuthoredObligationReferenceRequired");
  const auto *authored = catalog.obligations.find(ref.id);
  const auto *value = state.obligations.find(ref.id);
  if (!authored)
    return rejected("EconomicAuthoredObligationRequired");
  if (!value || value->id != ref.id)
    return rejected("UnknownEconomicObligation");
  auto valid = authored_obligation_matches(*authored, *value, ref.id);
  if (!valid.ok)
    return {false, nullptr, valid.error};
  const auto is_organization = [&](const SubjectRef &subject) {
    return subject.kind == SubjectKind::organization &&
           subject.id == organization_id;
  };
  if (!is_organization(authored->principal) &&
      !is_organization(authored->beneficiary))
    return rejected("EconomicAuthoredObligationOrganizationMismatch");
  if (!subject_exists(common, state, value->principal) ||
      !subject_exists(common, state, value->beneficiary))
    return rejected("InvalidEconomicResponsibilitySubject");
  const bool ration = value->kind == ObligationKind::ration;
  if (value->source_wallet_or_inventory.empty() ||
      value->authority_ref.empty() ||
      (ration ? !common.inventory(value->source_wallet_or_inventory)
              : !common.wallet(value->source_wallet_or_inventory)) ||
      (!value->destination.empty() &&
       (ration ? !common.inventory(value->destination)
               : !common.wallet(value->destination))) ||
      (ration && !common.item_definition(value->item_id)))
    return rejected("EconomicAuthoredObligationAssetsMismatch");
  return {true, authored, {}};
}
static Result read_source(const CommonStore &common,
                          const LocalEconomyState &economy,
                          const LocalEconomyCatalog &catalog,
                          const FiscalSettlementState *fiscal,
                          const FiscalSettlementCatalog *fiscal_catalog,
                          const PublicReliefState *relief,
                          const EconomicPublicAccountSelection &selection,
                          const EconomicResponsibilityRef &ref,
                          bool historical_configuration) {
  if (!catalog.enabled)
    return fail("EconomicResponsibilityDisabled", ref.id);
  if (ref.id.empty() ||
      (ref.kind == EconomicResponsibilityKind::production_asset_use
           ? ref.unit_id.empty()
           : !ref.unit_id.empty()))
    return fail("InvalidEconomicResponsibilityReference", ref.id);
  if (selection.original_owner.kind != Owner::Kind::organization ||
      selection.original_owner.id.empty() ||
      (selection.wallet_ids.empty() && selection.inventory_ids.empty()))
    return fail("InvalidEconomicPublicAccountOwner", ref.id);
  if (bool(fiscal) != bool(fiscal_catalog))
    return fail("EconomicFiscalSourceUnavailable", ref.id);
  std::set<StableId> wallets, inventories;
  for (const auto &id : selection.wallet_ids)
    if (!common.wallet(id) || !wallets.insert(id).second)
      return fail("InvalidEconomicPublicWalletSelection", id);
  for (const auto &id : selection.inventory_ids)
    if (!common.inventory(id) || !inventories.insert(id).second)
      return fail("InvalidEconomicPublicInventorySelection", id);
  EconomicResponsibilitySource out;
  out.reference = ref;
  out.source_identity = common.state().identity;
  Status checked = common_success();
  switch (ref.kind) {
  case EconomicResponsibilityKind::production_unit:
    checked = production(economy, catalog, ref.id, out);
    break;
  case EconomicResponsibilityKind::production_asset_use: {
    checked = production(economy, catalog, ref.unit_id, out);
    if (!checked.ok)
      break;
    auto asset =
        std::find_if(out.production_assets.begin(), out.production_assets.end(),
                     [&](const auto &a) { return a.id == ref.id; });
    if (asset == out.production_assets.end())
      return fail("UnknownEconomicProductionAssetUse", ref.id);
    auto selected = *asset;
    out.production_assets = {std::move(selected)};
    out.authored_sources.push_back(
        {"production_management.assets", ref.id, ref.unit_id});
    break;
  }
  case EconomicResponsibilityKind::pending_configuration: {
    const auto *management = economy.production_management
                                 ? &*economy.production_management
                                 : nullptr;
    const auto *record =
        management ? management->configurations.find(ref.id) : nullptr;
    const auto *pending =
        record ? management->pending_by_unit.find(record->unit_id) : nullptr;
    if (!record || record->id != ref.id ||
        (!historical_configuration && (!pending || *pending != ref.id)))
      return fail("UnknownEconomicPendingConfiguration", ref.id);
    checked = production(economy, catalog, record->unit_id, out);
    if (checked.ok)
      checked = append_fact_source(common, out, ref.id,
                                   "production_configuration_requested");
    break;
  }
  case EconomicResponsibilityKind::obligation:
    checked = obligation(common, economy, catalog, ref.id, out);
    break;
  case EconomicResponsibilityKind::arrear: {
    const auto *debt = economy.arrears.find(ref.id);
    if (!debt || debt->id != ref.id)
      return fail("UnknownEconomicArrear", ref.id);
    checked = obligation(common, economy, catalog, debt->obligation_id, out);
    if (!checked.ok)
      break;
    const auto *due = economy.obligations.find(debt->obligation_id);
    if (due->kind == ObligationKind::ration)
      return fail("InvalidEconomicArrearSource", ref.id);
    checked = append_fact_source(common, out, ref.id,
                                 due->kind == ObligationKind::wage
                                     ? "wage_due"
                                     : "contract_payment_due");
    break;
  }
  case EconomicResponsibilityKind::fiscal_assessment: {
    if (!fiscal || !fiscal_catalog)
      return fail("EconomicFiscalSourceUnavailable", ref.id);
    if (fiscal->version != 1 || fiscal_catalog->version != 1)
      return fail("UnsupportedEconomicFiscalVersion", ref.id);
    const auto *assessment = fiscal->assessments.find(ref.id);
    const auto *period =
        assessment ? fiscal->periods.find(assessment->period_id) : nullptr;
    const auto *district =
        period ? fiscal_catalog->districts.find(period->district_id) : nullptr;
    const auto *source = district && assessment
                             ? district->sources.find(assessment->source_id)
                             : nullptr;
    if (!assessment || assessment->id != ref.id || !period || !period->policy ||
        !source || source->id != assessment->source_id ||
        source->wallet_id != assessment->wallet_id ||
        source->inventory_id != assessment->inventory_id ||
        source->grain_item_id != assessment->grain_item_id ||
        std::count(period->source_assessment_ids.begin(),
                   period->source_assessment_ids.end(), ref.id) != 1 ||
        !subject_exists(common, economy, source->taxpayer))
      return fail("InvalidEconomicFiscalResponsibility", ref.id);
    out.original_principal = source->taxpayer;
    out.original_beneficiaries = {
        {SubjectKind::organization, period->policy->organization_id}};
    out.site_id = period->policy->site_id;
    out.period_id = period->id;
    out.authored_sources.push_back(
        {"fiscal_settlement.sources", source->id, district->id});
    container(out, assessment->wallet_id, false,
              EconomicContainerRole::payment);
    container(out, assessment->inventory_id, true,
              EconomicContainerRole::payment, assessment->grain_item_id);
    container(out, period->policy->public_wallet_id, false,
              EconomicContainerRole::receipt);
    container(out, period->policy->public_inventory_id, true,
              EconomicContainerRole::receipt, assessment->grain_item_id);
    checked = append_fact_source(common, out, period->policy->source_fact_id,
                                 "political_fiscal_policy_changed", "politics");
    if (checked.ok)
      checked = append_fact_source(common, out, period->registration_fact_id,
                                   "fiscal_source_registered");
    if (checked.ok)
      checked =
          append_fact_source(common, out, period->id, "fiscal_period_assessed");
    if (checked.ok)
      checked =
          append_fact_source(common, out, ref.id, "fiscal_source_assessed");
    break;
  }
  case EconomicResponsibilityKind::relief_contract: {
    if (!relief)
      return fail("EconomicReliefSourceUnavailable", ref.id);
    if (relief->version != 1)
      return fail("UnsupportedEconomicReliefVersion", ref.id);
    // 唯一外部责任ID用contract_id；commitment本表仍以terms_id为键。
    const PublicReliefCommitment *commitment = nullptr;
    for (const auto &row : relief->commitments.rows())
      if (row.second.terms.contract_id == ref.id) {
        if (commitment)
          return fail("DuplicateEconomicReliefContract", ref.id);
        commitment = &row.second;
      }
    if (!commitment)
      return fail("UnknownEconomicReliefContract", ref.id);
    const auto &terms = commitment->terms;
    if (terms.recipients.size() != commitment->transfers.size())
      return fail("InvalidEconomicReliefRecipients", ref.id);
    out.original_principal =
        SubjectRef{SubjectKind::organization, terms.organization_id};
    out.site_id = terms.site_id;
    out.task_id = terms.task_id;
    out.contract_id = terms.contract_id;
    out.authored_sources.push_back(
        {"public_relief.policies", commitment->policy_id, {}});
    container(out, terms.public_wallet_id, false,
              EconomicContainerRole::payment);
    container(out, terms.public_inventory_id, true,
              EconomicContainerRole::payment, terms.item_id);
    for (std::size_t i = 0; i < commitment->transfers.size(); ++i) {
      const auto &recipient = terms.recipients[i];
      const auto &transfer = commitment->transfers[i];
      const auto *cohort = economy.cohorts.find(recipient.cohort_id);
      const auto *household =
          economy.households.find(recipient.household_group_id);
      if (!cohort || !household ||
          cohort->household_group_id != household->id ||
          transfer.cohort_id != recipient.cohort_id ||
          transfer.household_group_id != recipient.household_group_id ||
          transfer.wallet_id != household->wallet_id ||
          transfer.inventory_id != household->inventory_id ||
          transfer.cash != recipient.cash || transfer.grain != recipient.grain)
        return fail("InvalidEconomicReliefRecipientSource", ref.id);
      out.original_beneficiaries.push_back(
          {SubjectKind::cohort, recipient.cohort_id});
      container(out, transfer.wallet_id, false, EconomicContainerRole::receipt);
      container(out, transfer.inventory_id, true,
                EconomicContainerRole::receipt, terms.item_id);
    }
    checked = append_fact_source(common, out, terms.authority_source_fact_id,
                                 "governance_authority_issued", "politics");
    if (checked.ok)
      checked = append_fact_source(common, out, terms.acceptance_fact_id,
                                   "governance_task_accepted", "tasks");
    if (checked.ok)
      checked =
          append_fact_source(common, out, terms.id, "public_relief_reserved");
    break;
  }
  case EconomicResponsibilityKind::supply_source: {
    const auto *source = catalog.supply_sources.find(ref.id);
    if (!source || source->source_key != ref.id)
      return fail("UnknownEconomicSupplySource", ref.id);
    out.site_id = source->site_id;
    out.authored_sources.push_back({"supply_sources", ref.id, {}});
    container(out, source->inventory_id, true,
              EconomicContainerRole::production_output, source->item_id);
    const LocalSupplyRegistration *registered = nullptr;
    for (const auto &row : economy.supply_registrations.rows())
      if (row.second.source_key == ref.id) {
        if (registered)
          return fail("DuplicateEconomicSupplyRegistration", ref.id);
        registered = &row.second;
      }
    if (!registered || registered->organization_id != source->organization_id ||
        registered->site_id != source->site_id ||
        registered->inventory_id != source->inventory_id ||
        registered->item_id != source->item_id)
      return fail("InvalidEconomicSupplyRegistration", ref.id);
    checked =
        append_fact_source(common, out, registered->id, "local_supply_source");
    break;
  }
  default:
    return fail("UnsupportedEconomicResponsibilityKind", ref.id);
  }
  if (!checked.ok)
    return {false, {}, checked.error};
  bool selected = false;
  for (const auto &resource : out.containers) {
    if (resource.inventory) {
      if (!common.inventory(resource.id))
        return fail("UnknownEconomicResponsibilityInventory", resource.id);
      selected |= inventories.count(resource.id) != 0;
    } else {
      if (!common.wallet(resource.id))
        return fail("UnknownEconomicResponsibilityWallet", resource.id);
      selected |= wallets.count(resource.id) != 0;
    }
  }
  if (!selected)
    return fail("EconomicResponsibilityOutsideSelection", ref.id);
  return {true, std::move(out), {}};
}
Result read_economic_responsibility_source(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,
    const EconomicPublicAccountSelection& selection,const EconomicResponsibilityRef& ref){
    return read_source(common,economy,catalog,fiscal,fiscal_catalog,relief,selection,ref,false);
}
Result read_economic_retained_responsibility_source(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,
    const EconomicPublicAccountSelection& selection,const EconomicResponsibilityRef& ref){
    return read_source(common,economy,catalog,fiscal,fiscal_catalog,relief,selection,ref,true);
}
namespace {
struct HistoryBoundary {
    EconomicPublicAccountSelection selection;
    Integer sequence=0;
    std::size_t fact_position=0;
    std::map<StableId,std::size_t> facts;
};
// 实例序只用于已验证的原发生链；不能由序号推断某次occupation当时已applied。
std::optional<Integer> instance_sequence(const CommonStore& common,const StableId& id,const StableId& prefix={}){
    const auto colon=id.rfind(':');if(colon==StableId::npos||colon==0||(!prefix.empty()&&id.substr(0,colon)!=prefix))return {};
    const auto digits=id.substr(colon+1);Integer result=0;
    const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||result<1||result>=common.state().next_instance_id||std::to_string(result)!=digits)return {};
    return result;
}
Outcome<HistoryBoundary> history_boundary(const CommonStore& common,const MilitaryOccupationFact& source,const DynamicStepInput* phase){
    Integer evaluation_tick=common.tick();if(phase){if(evaluation_tick<0||evaluation_tick==INT64_MAX||phase->from_tick!=evaluation_tick||phase->to_tick!=evaluation_tick+1)return {false,{},common_failure("EconomicTakeoverPhaseBoundaryMismatch",source.id).error};evaluation_tick=phase->to_tick;}
    const auto reject=[&](const StableId& code)->Outcome<HistoryBoundary>{return {false,{},common_failure(code,source.id).error};};
    const auto serial=instance_sequence(common,source.id,"military");const auto* header=common.state().facts.find(source.id);
    if(!serial||!header||header->id!=source.id||header->domain!="military"||header->kind!="military_occupation"||
       source.phase!="completed"||source.tick<0||source.tick>evaluation_tick||!source.assets_applied.value_or(false)||!source.asset_manifest||
       source.from_organization_id.empty()||source.to_organization_id.empty()||source.from_organization_id==source.to_organization_id)
        return reject("InvalidEconomicOccupationHistory");
    HistoryBoundary out;out.sequence=*serial;out.selection.original_owner={Owner::Kind::organization,source.from_organization_id};
    out.selection.wallet_ids=source.public_wallet_ids;out.selection.inventory_ids=source.public_inventory_ids;
    const Owner next{Owner::Kind::organization,source.to_organization_id};std::set<StableId> wallets,inventories;
    for(const auto& row:source.asset_manifest->wallets)
        if(row.first.empty()||!wallets.insert(row.first).second||!common.wallet(row.first)||
           !owner_equal(row.second.from_owner,out.selection.original_owner)||!owner_equal(row.second.to_owner,next))return reject("InvalidEconomicOccupationManifest");
    for(const auto& row:source.asset_manifest->inventories)
        if(row.first.empty()||!inventories.insert(row.first).second||!common.inventory(row.first)||
           !owner_equal(row.second.from_owner,out.selection.original_owner)||!owner_equal(row.second.to_owner,next))return reject("InvalidEconomicOccupationManifest");
    if(wallets.size()!=source.public_wallet_ids.size()||inventories.size()!=source.public_inventory_ids.size()||
       wallets!=std::set<StableId>(source.public_wallet_ids.begin(),source.public_wallet_ids.end())||
       inventories!=std::set<StableId>(source.public_inventory_ids.begin(),source.public_inventory_ids.end()))return reject("InvalidEconomicOccupationManifest");
    for(std::size_t n=0;n<common.state().facts.rows().size();++n){const auto& row=common.state().facts.rows()[n];
        if(row.first!=row.second.id||!out.facts.emplace(row.first,n).second)return reject("InvalidEconomicHistoryFactOrder");}
    out.fact_position=out.facts.at(source.id);return {true,std::move(out),{}};
}
Outcome<bool> fact_before(const CommonStore& common,const HistoryBoundary& boundary,const StableId& id,const StableId& kind={},const StableId& domain="local_economy"){
    const auto* fact=common.state().facts.find(id);const auto sequence=instance_sequence(common,id);const auto found=boundary.facts.find(id);
    if(!fact||fact->id!=id||(!kind.empty()&&(fact->kind!=kind||fact->domain!=domain))||!sequence||found==boundary.facts.end())
        return {false,{},common_failure("EconomicResponsibilityHistorySourceMissing",id).error};
    if((*sequence<boundary.sequence)!=(found->second<boundary.fact_position))return {false,{},common_failure("EconomicResponsibilityHistoryOrderMismatch",id).error};
    return {true,*sequence<boundary.sequence,{}};
}
Outcome<bool> responsibility_existed(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const PublicReliefState* relief,const HistoryBoundary& boundary,const EconomicResponsibilityRef& ref){
    const auto reject=[&](const StableId& code)->Outcome<bool>{return {false,{},common_failure(code,ref.id).error};};
    if(ref.id.empty()||(ref.kind==EconomicResponsibilityKind::production_asset_use?ref.unit_id.empty():!ref.unit_id.empty()))return reject("InvalidEconomicResponsibilityReference");
    switch(ref.kind){
    case EconomicResponsibilityKind::production_unit:
    case EconomicResponsibilityKind::production_asset_use:
        // 当前只支持原作者生产单元；动态新单位必须先提供真实创建源。
        if(!catalog.production_units.find(ref.kind==EconomicResponsibilityKind::production_unit?ref.id:ref.unit_id))return reject("EconomicProductionHistoryUnavailable");
        return {true,true,{}};
    case EconomicResponsibilityKind::pending_configuration:{
        if(!economy.production_management)return reject("EconomicProductionHistoryUnavailable");
        const auto& management=*economy.production_management;const auto* record=management.configurations.find(ref.id);
        if(!record||record->id!=ref.id)return reject("EconomicConfigurationHistoryUnavailable");
        auto requested=fact_before(common,boundary,record->id,"production_configuration_requested");if(!requested.ok)return requested;
        bool resolved=false,before=false;
        for(const auto& row:management.results.rows())if(row.second.configuration_id==record->id){
            const auto& result=row.second;if(resolved||row.first!=result.id||(result.kind!="applied"&&result.kind!="cancelled"&&result.kind!="superseded"))return reject("InvalidEconomicConfigurationHistory");
            auto ended=fact_before(common,boundary,result.id,"production_configuration_"+result.kind);if(!ended.ok)return ended;
            if(boundary.facts.at(record->id)>=boundary.facts.at(result.id))return reject("InvalidEconomicConfigurationHistory");
            resolved=true;before=ended.value;
        }
        return {true,requested.value&&!before,{}};
    }
    case EconomicResponsibilityKind::obligation:{
        if(catalog.obligations.find(ref.id))return {true,true,{}};
        const auto* due=economy.obligations.find(ref.id);const auto* binding=due?economy.membership_wage_bindings.find(due->authority_ref):nullptr;
        const auto created=instance_sequence(common,ref.id,"membership_obligation");
        const auto bound=binding?instance_sequence(common,binding->id,"membership_wage"):std::optional<Integer>{};
        if(!due||!binding||binding->obligation_id!=ref.id||!created||!bound||*bound>=*created)return reject("EconomicObligationHistoryUnavailable");
        auto decision=fact_before(common,boundary,binding->decision_id,"membership_decided","organization");if(!decision.ok)return decision;
        const auto accepted=instance_sequence(common,binding->decision_id);
        if(!accepted||*accepted>=*bound||(*created<boundary.sequence&&!decision.value))return reject("EconomicResponsibilityHistoryOrderMismatch");
        return {true,*created<boundary.sequence,{}};
    }
    case EconomicResponsibilityKind::arrear:{
        const auto* debt=economy.arrears.find(ref.id);const auto* due=debt?economy.obligations.find(debt->obligation_id):nullptr;
        if(!debt||!due||due->kind==ObligationKind::ration)return reject("EconomicArrearHistoryUnavailable");
        return fact_before(common,boundary,ref.id,due->kind==ObligationKind::wage?"wage_due":"contract_payment_due");
    }
    case EconomicResponsibilityKind::fiscal_assessment:
        if(!fiscal||!fiscal->assessments.find(ref.id))return reject("EconomicFiscalHistoryUnavailable");
        return fact_before(common,boundary,ref.id,"fiscal_source_assessed");
    case EconomicResponsibilityKind::relief_contract:{
        const PublicReliefCommitment* commitment=nullptr;
        if(relief)for(const auto& row:relief->commitments.rows())if(row.second.terms.contract_id==ref.id){if(commitment)return reject("DuplicateEconomicReliefContract");commitment=&row.second;}
        if(!commitment)return reject("EconomicReliefHistoryUnavailable");
        return fact_before(common,boundary,commitment->terms.id,"public_relief_reserved");
    }
    case EconomicResponsibilityKind::supply_source:{
        const LocalSupplyRegistration* source=nullptr;
        for(const auto& row:economy.supply_registrations.rows())if(row.second.source_key==ref.id){if(source)return reject("DuplicateEconomicSupplyRegistration");source=&row.second;}
        if(!source||!catalog.supply_sources.find(ref.id))return reject("EconomicSupplyHistoryUnavailable");
        return fact_before(common,boundary,source->id,"local_supply_source");
    }
    default:return reject("UnsupportedEconomicResponsibilityKind");
    }
}
Result read_history(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,
    const HistoryBoundary& boundary,const EconomicResponsibilityRef& ref){
    auto exists=responsibility_existed(common,economy,catalog,fiscal,relief,boundary,ref);if(!exists.ok)return {false,{},exists.error};
    if(!exists.value)return fail("EconomicResponsibilityNotPresentAtOccupation",ref.id);
    auto result=read_source(common,economy,catalog,fiscal,fiscal_catalog,relief,boundary.selection,ref,true);if(!result.ok)return result;
    for(const auto& id:result.value.source_fact_ids){auto before=fact_before(common,boundary,id);if(!before.ok)return {false,{},before.error};
        if(!before.value)return fail("EconomicResponsibilityHistorySourceAfterOccupation",id);}
    return result;
}
// 全域validator是前置；这里还反查原事实头，防止整个来源记录被删除后枚举漏项。
Status retained_history(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const PublicReliefState* relief){
    for(const auto& row:catalog.production_units.rows())if(!economy.production_units.find(row.first))return common_failure("EconomicProductionHistoryUnavailable",row.first);
    for(const auto& row:catalog.obligations.rows())if(!economy.obligations.find(row.first))return common_failure("EconomicObligationHistoryUnavailable",row.first);
    for(const auto& row:economy.membership_wage_bindings.rows())if(!economy.obligations.find(row.second.obligation_id))return common_failure("EconomicObligationHistoryUnavailable",row.first);
    for(const auto& row:common.state().facts.rows()){
        const auto& fact=row.second;if(fact.domain!="local_economy")continue;
        if(fact.kind=="production_configuration_requested"&&(!economy.production_management||!economy.production_management->configurations.find(fact.id)))return common_failure("EconomicConfigurationHistoryUnavailable",fact.id);
        if((fact.kind=="production_configuration_applied"||fact.kind=="production_configuration_cancelled"||fact.kind=="production_configuration_superseded")&&(!economy.production_management||!economy.production_management->results.find(fact.id)))return common_failure("EconomicConfigurationHistoryUnavailable",fact.id);
        if((fact.kind=="wage_due"||fact.kind=="contract_payment_due")&&!economy.arrears.find(fact.id))return common_failure("EconomicArrearHistoryUnavailable",fact.id);
        if(fact.kind=="fiscal_source_assessed"&&(!fiscal||!fiscal->assessments.find(fact.id)))return common_failure("EconomicFiscalHistoryUnavailable",fact.id);
        if(fact.kind=="public_relief_reserved"&&(!relief||!relief->commitments.find(fact.id)))return common_failure("EconomicReliefHistoryUnavailable",fact.id);
        if(fact.kind=="local_supply_source"&&!economy.supply_registrations.find(fact.id))return common_failure("EconomicSupplyHistoryUnavailable",fact.id);
    }
    return common_success();
}
}
// 旧冷读符号维持严格当前tick；只有原阶段显式传入from/to才读取本步已完成占领。
Status validate_economic_occupation_history_boundary(const CommonStore& c,const MilitaryOccupationFact& occupation,const DynamicStepInput* phase){auto boundary=history_boundary(c,occupation,phase);return boundary.ok?common_success():Status{false,{},boundary.error};}
Result read_economic_public_account_responsibility_history(const CommonStore& c,const LocalEconomyState& e,const LocalEconomyCatalog& cat,const FiscalSettlementState* f,const FiscalSettlementCatalog* fc,const PublicReliefState* r,const MilitaryOccupationFact& occupation,const EconomicResponsibilityRef& ref){return read_economic_public_account_responsibility_history(c,e,cat,f,fc,r,occupation,ref,nullptr);}
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(const CommonStore& c,const LocalEconomyState& e,const LocalEconomyCatalog& cat,const FiscalSettlementState* f,const FiscalSettlementCatalog* fc,const PublicReliefState* r,const MilitaryOccupationFact& occupation){return read_economic_public_account_responsibility_keys(c,e,cat,f,fc,r,occupation,nullptr);}
Result read_economic_public_account_responsibility_history(const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,
    const MilitaryOccupationFact& occupation,const EconomicResponsibilityRef& ref,const DynamicStepInput* phase){
    auto boundary=history_boundary(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};
    return read_history(common,economy,catalog,fiscal,fiscal_catalog,relief,boundary.value,ref);
}
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,const MilitaryOccupationFact& occupation,const DynamicStepInput* phase){return read_economic_public_account_responsibility_keys(common,economy,catalog,fiscal,fiscal_catalog,relief,occupation,phase,nullptr);}
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,const MilitaryOccupationFact& occupation,const DynamicStepInput* phase,const EstateEmploymentContext* employment){return read_economic_public_account_responsibility_keys(common,economy,catalog,fiscal,fiscal_catalog,relief,occupation,phase,employment,nullptr);}
Outcome<std::vector<EconomicResponsibilityRef>> read_economic_public_account_responsibility_keys(
    const CommonStore& common,const LocalEconomyState& economy,const LocalEconomyCatalog& catalog,
    const FiscalSettlementState* fiscal,const FiscalSettlementCatalog* fiscal_catalog,const PublicReliefState* relief,const MilitaryOccupationFact& occupation,const DynamicStepInput* phase,const EstateEmploymentContext* employment,const PrivateShopResponsibilitySourceContext* shops){
    if(shops){if(&shops->common!=&common||&shops->economy!=&economy||&shops->economy_catalog!=&catalog||shops->phase!=phase)return {false,{},common_failure("EconomicPrivateShopHistoryContextMismatch").error};auto valid=validate_private_shop_public_responsibility_sources(common,economy,catalog,shops->state,shops->catalog,phase);if(!valid.ok)return {false,{},valid.error};}

    if(employment){if(&employment->common!=&common||&employment->economy!=&economy||&employment->economy_catalog!=&catalog||employment->phase!=phase)return {false,{},common_failure("EconomicEmploymentHistoryContextMismatch").error};auto valid=employment->common_catalog.family_birth_identity_enabled?validate_estate_employment_original_sources(*employment):validate_estate_employment(*employment);if(!valid.ok)return {false,{},valid.error};}
    using Keys=Outcome<std::vector<EconomicResponsibilityRef>>;
    auto boundary=history_boundary(common,occupation,phase);if(!boundary.ok)return {false,{},boundary.error};
    if(!catalog.enabled||bool(fiscal)!=bool(fiscal_catalog))return {false,{},common_failure("EconomicHistoryProfileMismatch",occupation.id).error};
    auto retained=retained_history(common,economy,catalog,fiscal,relief);if(!retained.ok)return {false,{},retained.error};
    if(boundary.value.selection.wallet_ids.empty()&&boundary.value.selection.inventory_ids.empty())return {true,{}, {}};
    std::vector<EconomicResponsibilityRef> keys;std::set<std::pair<Integer,std::pair<StableId,StableId>>> seen;
    std::set<StableId> pending_units;
    const auto include=[&](const EconomicResponsibilityRef& ref)->Status{
        auto source=read_history(common,economy,catalog,fiscal,fiscal_catalog,relief,boundary.value,ref);
        if(!source.ok){const auto& code=source.error.original.code;
            if(code=="EconomicResponsibilityOutsideSelection"||code=="EconomicResponsibilityNotPresentAtOccupation")return common_success();
            return {false,{},source.error};}
        if(!seen.emplace(static_cast<Integer>(ref.kind),std::make_pair(ref.unit_id,ref.id)).second)return common_failure("DuplicateEconomicHistoricalResponsibility",ref.id);
        if(ref.kind==EconomicResponsibilityKind::pending_configuration){const auto* config=economy.production_management->configurations.find(ref.id);
            if(!pending_units.insert(config->unit_id).second)return common_failure("AmbiguousEconomicHistoricalConfiguration",ref.id);}
        keys.push_back(ref);return common_success();
    };
    for(const auto& row:economy.production_units.rows()){
        auto result=include({EconomicResponsibilityKind::production_unit,row.first,{}});if(!result.ok)return Keys{false,{},result.error};
        const auto* plan=catalog.production_management?catalog.production_management->plans.find(row.first):nullptr;
        if(plan)for(const auto& asset:plan->asset_uses){result=include({EconomicResponsibilityKind::production_asset_use,asset.asset_id,row.first});if(!result.ok)return Keys{false,{},result.error};}
    }
    if(economy.production_management)for(const auto& row:economy.production_management->configurations.rows()){
        auto result=include({EconomicResponsibilityKind::pending_configuration,row.first,{}});if(!result.ok)return Keys{false,{},result.error};}
    // 私人雇用不进入公库继受。仅排除两端均仍为原真人私钱包、且本次真实清单均未选中的具名私约。
    // 任何公库被选中仍走原严格来源拒绝；完整私约在显式LE/雇用validator独立核验。
    const auto private_employment_outside=[&](const RecurringObligation& wage){
        if(!employment||wage.principal.kind!=SubjectKind::actor||wage.beneficiary.kind!=SubjectKind::actor||wage.kind!=ObligationKind::wage||wage.authority_ref.rfind("estate_employment:",0)!=0)return false;
        const auto* head=common.state().facts.find(wage.authority_ref);const auto* payer=common.wallet(wage.source_wallet_or_inventory);const auto* payee=common.wallet(wage.destination);
        return head&&head->id==wage.authority_ref&&head->kind=="estate_employment_offered"&&head->domain=="estate_employment"&&payer&&payee&&owner_equal(payer->owner,{Owner::Kind::actor,wage.principal.id,{}})&&owner_equal(payee->owner,{Owner::Kind::actor,wage.beneficiary.id,{}})&&std::find(boundary.value.selection.wallet_ids.begin(),boundary.value.selection.wallet_ids.end(),payer->id)==boundary.value.selection.wallet_ids.end()&&std::find(boundary.value.selection.wallet_ids.begin(),boundary.value.selection.wallet_ids.end(),payee->id)==boundary.value.selection.wallet_ids.end();
    };
    for(const auto& row:economy.obligations.rows()){
        if(private_employment_outside(row.second))continue;
        if(shops){auto outside=private_shop_obligation_outside_public_selection(*shops,row.second,boundary.value.selection);if(!outside.ok)return Keys{false,{},outside.error};if(outside.value)continue;}
        auto result=include({EconomicResponsibilityKind::obligation,row.first,{}});if(!result.ok)return Keys{false,{},result.error};}
    for(const auto& row:economy.arrears.rows()){
        const auto* wage=economy.obligations.find(row.second.obligation_id);if(wage&&private_employment_outside(*wage))continue;
        if(wage&&shops){auto outside=private_shop_obligation_outside_public_selection(*shops,*wage,boundary.value.selection);if(!outside.ok)return Keys{false,{},outside.error};if(outside.value)continue;}
        auto result=include({EconomicResponsibilityKind::arrear,row.first,{}});if(!result.ok)return Keys{false,{},result.error};}
    if(fiscal)for(const auto& row:fiscal->assessments.rows()){
        auto result=include({EconomicResponsibilityKind::fiscal_assessment,row.first,{}});if(!result.ok)return Keys{false,{},result.error};}
    if(relief)for(const auto& row:relief->commitments.rows()){
        auto result=include({EconomicResponsibilityKind::relief_contract,row.second.terms.contract_id,{}});if(!result.ok)return Keys{false,{},result.error};}
    for(const auto& row:catalog.supply_sources.rows()){
        auto result=include({EconomicResponsibilityKind::supply_source,row.first,{}});if(!result.ok)return Keys{false,{},result.error};}
    return {true,std::move(keys),{}};
}
}
