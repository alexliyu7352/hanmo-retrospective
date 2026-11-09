// 原静态据点归属的独立叶门；占领源、映射歧义和今日字段伪改都必须拒绝。
#include "resident/domains/military/field_battle/field_exit_policy.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include "resident/domains/military/occupation_applied_source.hpp"
#include "resident/storage/common_store.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 本门只断言真实读取结果，不提供替代世界reader。
void check(bool ok, const char *reason) { if (!ok) throw std::runtime_error(reason); }
// 反例同时核原失败码，避免任何早错被误算为所需source边界。
template<class T> void check_error(const Outcome<T> &result, const char *code,
                                   const char *reason) {
  check(!result.ok && result.error.original.code == code, reason);
}

// 复用原资源组件的数据形状；没有另造正式游戏配置。
struct OwnershipHistoryFixture {
    std::shared_ptr<CommonCatalog> common=std::make_shared<CommonCatalog>();
    MilitaryState state;MilitaryCatalog catalog;MilitaryAccess access;
    CommonState initial;
    OwnershipHistoryFixture(){
        common->organization_ids={"old","new"};common->items.insert("grain",{"grain",1,1,false,false});
        Owner old{Owner::Kind::organization,"old",{}},next{Owner::Kind::organization,"new",{}},private_owner{Owner::Kind::actor,"private",{}};
        common->required_wallets.insert("public-wallet",{old,0});common->required_inventories.insert("public-store",{old,100});
        initial.identity.world_id[0]=1;initial.identity.tick=10;initial.metadata.player_actor_id="commander";
        ActorRecord commander;commander.id="commander";commander.alive=true;commander.available=true;commander.hp=100;commander.location=SettlementLocation{"target","gate"};initial.actors.insert(commander.id,commander);
        initial.wallets.insert("public-wallet",{"public-wallet",old,200,{}});
        InventoryView inventory;inventory.id="public-store";inventory.owner=old;inventory.capacity=100;inventory.revision=7;
        inventory.lots.push_back({"public-lot","grain","",5,old,{}});inventory.lots.push_back({"private-lot","grain","private-reservation",3,private_owner,{}});initial.inventories.insert(inventory.id,inventory);
        catalog.item_ids={"grain"};StrongholdDefinition h;h.id="fort";h.garrison_id="camp";h.settlement_id="target";h.public_inventory_ids={"public-store"};h.public_wallet_ids={"public-wallet"};catalog.strongholds.push_back(h);
        GarrisonDefinition camp;camp.id="camp";camp.organization_id="old";camp.inventory_id="public-store";camp.wallet_id="public-wallet";catalog.garrisons.push_back(camp);
        MilitaryOrderDefinition order;order.id="occupy-order";order.target_stronghold_id="fort";order.objective_kind="occupy";catalog.orders.push_back(order);
        OfficeDefinition office;office.id="old-office";office.garrison_id="camp";catalog.offices.push_back(office);state.offices.push_back({"old-office","old-holder",true,"appointment",{}});
        state.garrisons.push_back({"camp",{}, {},0,{}});state.strongholds.push_back({"fort","old","","siege",{}});
        ArmyRecord army;army.id="army";army.organization_id="new";army.commander_actor_id="commander";army.order_id="campaign";army.unit_ids={"unit"};state.armies.push_back(army);
        CampaignRecord campaign;campaign.id="campaign";campaign.army_id="army";campaign.template_id="occupy-order";state.campaigns.push_back(campaign);
        MilitaryUnitRecord unit;unit.id="unit";unit.army_id="army";unit.organization_id="new";unit.fit={"soldier"};state.military_units.push_back(unit);
        SiegeRecord siege;siege.id="siege";siege.stronghold_id="fort";siege.campaign_id="campaign";siege.phase="result";siege.armies={{"army",0}};siege.result=SiegeResult{"occupied",0,10,{},{},{}};state.sieges.push_back(siege);
        MilitaryOccupationFact f;f.id="occupation";f.tick=10;f.siege_id="siege";f.campaign_id="campaign";f.army_id="army";f.stronghold_id="fort";f.garrison_id="camp";f.from_organization_id="old";f.to_organization_id="new";f.public_inventory_ids={"public-store"};f.public_wallet_ids={"public-wallet"};state.occupation_facts.push_back(f);access.common_catalog=common.get();
    }
};
// 沿test_siege_world_resources的原最小资产夹具实际apply，再用Common头前后cut读归属。
// 这是原军事owner组件；不冒充完整Field接触，也不构造政治接管或新占领writer。
void check_applied_settlement_ownership_history() {
  OwnershipHistoryFixture f;
  CommonStore common(f.initial, f.common);
  f.access.fact_exists = [&](const StableId &id) {
    return common.state().facts.find(id) != nullptr;
  };
  auto append = [&](const char *prefix, const char *kind) {
    auto id = common.next_id(prefix);
    check(id.ok, "allocate real history component source");
    check(common.append_fact({id.value, kind, "military"}).ok,
          "append real history component head");
    return id.value;
  };
  const auto before = append("field_battle_contact", "field_battle_contact");
  const auto occupation = append("military", "military_occupation");
  f.state.occupation_facts.front().id = occupation;
  auto applied = apply_military_occupation_with_source(
      common, f.state, f.catalog, f.access, common.state().identity, occupation);
  check(applied.ok && applied.value.source.has_value(),
        "original military owner actually applies assets and receipt");
  const auto after = append("field_battle_contact", "field_battle_contact");
  const auto at = normalize_world_moment(10, {});
  check(at.ok, "actual original occupation tick");
  auto read = [&](const StableId &cut) {
    return read_military_garrison_owner_before_actual(
        common, f.state, f.catalog, f.access, "camp",
        SourceRefBuildView{"military", "field_battle_contact", cut},
        at.value, at.value, 1);
  };
  auto earlier = read(before), later = read(after);
  check(earlier.ok && earlier.value.organization_id() == "old" &&
        !earlier.value.actual_occupation,
        "same-tick later applied ownership cannot rewrite earlier real cut");
  check(later.ok && later.value.organization_id() == "new" &&
        later.value.actual_occupation == &f.state.occupation_facts.front(),
        "post-apply real cut uses original applied receipt");
  check(common.wallet("public-wallet")->balance == 200 &&
        common.wallet("public-wallet")->owner.id == "new" &&
        common.inventory("public-store")->lots[1].owner.id == "private" &&
        common.inventory("public-store")->lots[1].quantity == 3,
        "original apply keeps money and private goods while changing public owner");
  check_error(read_field_initial_settlement_controller(
      common, f.state, f.catalog, "target"),
      "FieldSettlementOccupationOriginalOwnerRequired",
      "finite initial helper never substitutes for occupied history");
  f.state.occupation_facts.front().previous_occupation_id = "missing";
  check_error(read(after), "OccupationReceiptRequired",
              "broken actual ownership predecessor rejected");
  f.state.occupation_facts.front().previous_occupation_id.clear();
  common.state().facts.find(occupation)->domain = "foreign";
  check_error(read(after), "CommonActualFactHeaderRequired",
              "foreign occupation head cannot grant historical owner");
}
}
// 既有Field target调用此入口；不新增可执行程序，不执行World热门或改作者fixture。
void check_field_initial_settlement_controller_source() {
    auto catalog = std::make_shared<CommonCatalog>();
    CommonState state;
    CommonStore common(state, catalog);
    MilitaryCatalog military_catalog;
    GarrisonDefinition camp; camp.id = "camp"; camp.organization_id = "original";
    military_catalog.garrisons.push_back(camp);
    StrongholdDefinition definition; definition.id = "hold";
    definition.garrison_id = camp.id; definition.settlement_id = "town";
    military_catalog.strongholds.push_back(definition);
    MilitaryState military;
    StrongholdRecord hold; hold.id = definition.id; hold.owner_organization_id = "original";
    military.strongholds.push_back(hold);
    auto actual = read_field_initial_settlement_controller(common, military, military_catalog, "town");
    check(actual.ok && actual.value == "original", "original garrison controller");
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "missing").ok,
          "no inferred controller without settlement source");
    military.strongholds.front().owner_organization_id = "forged";
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "mutable owner cannot replace original");
    military.strongholds.front().owner_organization_id = "original";
    military_catalog.strongholds.push_back(definition);
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "ambiguous source rejected");
    military_catalog.strongholds.pop_back();
    military.strongholds.front().occupation_id = "retained:1";
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "retained occupation binding cannot use initial ownership");
    military.strongholds.front().occupation_id.clear();
    MilitaryOccupationFact occupation; occupation.id = "occupation:body";
    military.occupation_facts.push_back(occupation);
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "occupation body cannot use initial ownership");
    military.occupation_facts.clear();
    military_catalog.garrisons.push_back(camp);
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "ambiguous original garrison rejected");
    military_catalog.garrisons.pop_back();
    military.strongholds.clear();
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "original initialized stronghold required");
    military.strongholds.push_back(hold);
    // 同一原owner读者的cut与时刻边界；这里只验原读口，不冒充完整Field contact正文。
    auto cut_id = common.next_id("field_battle_contact");
    check(cut_id.ok, "allocate actual boundary head");
    check(common.append_fact({cut_id.value, "field_battle_contact", "military"}).ok,
          "append actual boundary head");
    const SourceRefBuildView boundary{"military", "field_battle_contact", cut_id.value};
    MilitaryAccess access; access.common_catalog = catalog.get();
    const auto zero = normalize_world_moment(0, {});
    const auto one = normalize_world_moment(1, {});
    check(zero.ok && one.ok, "exact original test moments");
    auto before = read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, boundary,
        zero.value, zero.value, 1);
    check(before.ok && before.value.organization_id() == "original",
          "actual Common cut reads original ownership");
    auto current = read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, std::nullopt,
        zero.value, zero.value, 1);
    check(current.ok && current.value.organization_id() == "original",
          "current actual moment reads same original ownership");
    check_error(read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id,
        SourceRefBuildView{"military", "field_battle_contact", "missing"},
        zero.value, zero.value, 1), "CommonActualFactHeaderRequired", "missing real historical cut rejected");
    check_error(read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, boundary,
        one.value, zero.value, 1), "MilitaryOwnershipActualBoundaryRequired", "future historical moment rejected");
    check_error(read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, std::nullopt,
        zero.value, one.value, 1), "MilitaryOwnershipActualBoundaryRequired", "old moment without real cut rejected");
    check_error(read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, boundary,
        zero.value, zero.value, 99), "UnsupportedMilitaryOccupationSchema", "unsupported source schema rejected");
    military.strongholds.front().owner_organization_id = "forged";
    check_error(read_military_garrison_owner_before_actual(
        common, military, military_catalog, access, camp.id, boundary,
        zero.value, zero.value, 1), "OccupationSourceMissing", "mutable owner cannot supply historical source");
    military.strongholds.front().owner_organization_id = "original";
    CommonState action_residue;
    ActionCoreView action; action.id = "occupation:action"; action.kind = "military_occupation";
    action_residue.actions.insert(action.id, action);
    CommonStore action_common(std::move(action_residue), catalog);
    check(!read_field_initial_settlement_controller(action_common, military, military_catalog, "town").ok,
          "occupation action residue rejected");
    CommonState history_residue;
    history_residue.history.push_back({0, "military_occupation", "actor", "result:1", {}});
    CommonStore history_common(std::move(history_residue), catalog);
    check(!read_field_initial_settlement_controller(history_common, military, military_catalog, "town").ok,
          "occupation history residue rejected");
    CommonState result_residue;
    auto receipt = std::make_shared<CommitResult>(); receipt->kind = "military_occupation";
    result_residue.results.insert("request", receipt);
    CommonStore result_common(std::move(result_residue), catalog);
    check(!read_field_initial_settlement_controller(result_common, military, military_catalog, "town").ok,
          "occupation result residue rejected");
    auto original_id = common.next_id("occupation");
    check(original_id.ok, "real Common source identity");
    auto added = common.append_fact({original_id.value, "military_occupation", "military"});
    check(added.ok, "real negative source head");
    check(!read_field_initial_settlement_controller(common, military, military_catalog, "town").ok,
          "orphan occupation head cannot grant initial control");
    check_applied_settlement_ownership_history();
}
} // namespace hanmo::resident
