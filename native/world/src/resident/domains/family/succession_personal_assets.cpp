#include "resident/storage/money_transfer_recipe.hpp"
#include "succession_personal_assets.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
constexpr Integer maximum=std::numeric_limits<Integer>::max();
// 仅本人直接私产进入这份条款，不使用家庭、公职或组织代理权限。
bool owns(const Owner& owner,const StableId& actor){return !owner.extra&&owner.kind==Owner::Kind::actor&&owner.id==actor;}
// 报价不得跨world、冷恢复代次或同世界提交重用。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return std::tie(a.world_id,a.execution_epoch,a.revision,a.tick)==std::tie(b.world_id,b.execution_epoch,b.revision,b.tick);}
}
// 字段顺序是持久化元数据；业务条款只比较实际人物、容器、金额与批次。
bool succession_personal_terms_equal(const SuccessionPersonalTerms& a,const SuccessionPersonalTerms& b){
    if(std::tie(a.predecessor_actor_id,a.successor_actor_id,a.source_wallet_id,a.destination_wallet_id,a.source_inventory_id,a.destination_inventory_id,a.money)!=std::tie(b.predecessor_actor_id,b.successor_actor_id,b.source_wallet_id,b.destination_wallet_id,b.source_inventory_id,b.destination_inventory_id,b.money)||a.lots.size()!=b.lots.size())return false;
    for(std::size_t i=0;i<a.lots.size();++i)if(std::tie(a.lots[i].lot_id,a.lots[i].item_id,a.lots[i].quantity)!=std::tie(b.lots[i].lot_id,b.lots[i].item_id,b.lots[i].quantity))return false;
    return true;
}
// 不检查当前批次仍在原处，交割后的长期事实也必须保留这些原始来源。
Status validate_succession_personal_terms(const SuccessionPersonalTerms& terms){
    for(const auto* id:{&terms.predecessor_actor_id,&terms.successor_actor_id,&terms.source_wallet_id,&terms.destination_wallet_id,&terms.source_inventory_id,&terms.destination_inventory_id})
        if(id->empty()||id->size()>256)return common_failure("InvalidSuccessionPersonalTerms");
    if(terms.predecessor_actor_id==terms.successor_actor_id||terms.source_wallet_id==terms.destination_wallet_id||terms.source_inventory_id==terms.destination_inventory_id||terms.money<0||terms.lots.size()>4096)return common_failure("InvalidSuccessionPersonalTerms");
    std::set<StableId> lots;
    for(const auto& lot:terms.lots)if(lot.lot_id.empty()||lot.lot_id.size()>256||lot.item_id.empty()||lot.item_id.size()>256||lot.quantity<=0||!lots.insert(lot.lot_id).second)return common_failure("InvalidSuccessionPersonalLot",lot.lot_id);
    return common_success();
}
// 报价只选可支配钱与本人未预留批次；未选、受托及预留财物留在原处。
Outcome<SuccessionPersonalQuote> preview_succession_personal_assets(const CommonStore& common,const StableId& predecessor,const StableId& successor,const SuccessionPersonalSelection& selection){
    auto fail=[](const StableId& code,const StableId& path={}){return Outcome<SuccessionPersonalQuote>{false,{},common_failure(code,path).error};};
    if(predecessor.empty()||predecessor!=common.state().metadata.player_actor_id)return fail("NotPlayerActor",predecessor);
    if(predecessor==successor)return fail("InvalidSuccessionParties");
    const auto* source=common.actor(predecessor);const auto* target=common.actor(successor);if(!source||!target)return fail("UnknownActor",!source?predecessor:successor);
    if(selection.money<0||selection.goods.size()>4096)return fail("InvalidSuccessionPersonalSelection");
    const auto* wallet=common.wallet(source->wallet_id);const auto* receiving=common.wallet(target->wallet_id);
    const auto* bag=common.inventory(source->inventory_id);const auto* destination=common.inventory(target->inventory_id);
    if(!wallet||!receiving||!bag||!destination||wallet->id==receiving->id||bag->id==destination->id||!owns(wallet->owner,predecessor)||!owns(bag->owner,predecessor)||!owns(receiving->owner,successor)||!owns(destination->owner,successor))return fail("SuccessionPersonalOwnershipMismatch");
    const auto spendable=common.spendable(wallet->id);if(!spendable.ok)return {false,{},spendable.error};if(selection.money>spendable.value)return fail("InsufficientFunds",wallet->id);
    if(receiving->balance<0||receiving->balance>maximum-selection.money)return fail("MoneyOverflow",receiving->id);
    const auto weight=common.inventory_weight(destination->id);if(!weight.ok)return {false,{},weight.error};
    SuccessionPersonalQuote quote;quote.source_identity=common.state().identity;quote.selection=selection;
    auto& terms=quote.terms;terms.predecessor_actor_id=predecessor;terms.successor_actor_id=successor;terms.source_wallet_id=wallet->id;terms.destination_wallet_id=receiving->id;terms.source_inventory_id=bag->id;terms.destination_inventory_id=destination->id;terms.money=selection.money;
    quote.source_spendable=spendable.value;quote.retained_spendable=spendable.value-selection.money;quote.destination_weight=weight.value;quote.source_balance=wallet->balance;quote.destination_balance=receiving->balance;quote.source_inventory_revision=bag->revision;quote.destination_inventory_revision=destination->revision;
    std::set<StableId> items;
    // 与原TransferLots完全同序选本人无预留批次，条款保实际lot来源，不让品类总数掩盖他人货物。
    for(const auto& requested:selection.goods){
        const auto* item=common.item_definition(requested.item_id);
        if(requested.item_id.empty()||requested.quantity<=0||!item||item->weight<0||!items.insert(requested.item_id).second)return fail("InvalidSuccessionItemSelection",requested.item_id);
        if(item->weight&&requested.quantity>maximum/item->weight)return fail("WeightOverflow",requested.item_id);
        const auto added=requested.quantity*item->weight;if(quote.transferred_weight>maximum-added)return fail("WeightOverflow",requested.item_id);quote.transferred_weight+=added;
        Integer remaining=requested.quantity;
        for(const auto& lot:bag->lots){
            if(lot.item_id!=requested.item_id||!owns(lot.owner,predecessor)||lot.reserved_by!=ReservationRef{})continue;
            if(lot.extra||lot.quantity<=0||lot.id.empty())return fail("InvalidSuccessionLot",lot.id);
            const auto used=std::min(remaining,lot.quantity);terms.lots.push_back({lot.id,lot.item_id,used});remaining-=used;if(!remaining)break;
        }
        if(remaining)return fail("InsufficientGoods",requested.item_id);
        if(terms.lots.size()>4096)return fail("SuccessionLotLimit");
    }
    if(weight.value<0||weight.value>destination->capacity||quote.transferred_weight>destination->capacity-weight.value)return fail("InsufficientCapacity",destination->id);
    const auto valid=validate_succession_personal_terms(terms);if(!valid.ok)return {false,{},valid.error};
    // TransferLots每品类修改一次原容器、每原批分配一个新批；先拒绝边界溢出，不能进入有符号加法溢出。
    if(bag->revision<0||destination->revision<0||bag->revision>maximum-static_cast<Integer>(selection.goods.size())||destination->revision>maximum-static_cast<Integer>(terms.lots.size()))return fail("ProjectionOverflow");
    if(!terms.lots.empty()&&(common.state().next_instance_id<1||common.state().next_instance_id>maximum-static_cast<Integer>(terms.lots.size())))return fail("InstanceIdOverflow");
    return {true,std::move(quote),{}};
}
// 重新验证完整报价后返回一份共同原子recipe；不在此改变玩家或家事事实。
Outcome<ResourceRecipe> succession_personal_asset_recipe(const CommonStore& common,const SuccessionPersonalQuote& quote){
    if(!same_identity(quote.source_identity,common.state().identity))return {false,{},common_failure("StaleSuccessionPersonalQuote").error};
    const auto current=preview_succession_personal_assets(common,quote.terms.predecessor_actor_id,quote.terms.successor_actor_id,quote.selection);if(!current.ok)return {false,{},current.error};const auto& fresh=current.value;
    if(!succession_personal_terms_equal(quote.terms,fresh.terms)||std::tie(quote.source_spendable,quote.retained_spendable,quote.destination_weight,quote.transferred_weight,quote.source_balance,quote.destination_balance,quote.source_inventory_revision,quote.destination_inventory_revision)!=std::tie(fresh.source_spendable,fresh.retained_spendable,fresh.destination_weight,fresh.transferred_weight,fresh.source_balance,fresh.destination_balance,fresh.source_inventory_revision,fresh.destination_inventory_revision))return {false,{},common_failure("StaleSuccessionPersonalQuote").error};
    ResourceRecipe recipe;const auto& t=fresh.terms;
    if(t.money){auto money=resource_recipe::append_money_transfer(recipe,t.source_wallet_id,t.destination_wallet_id,t.money,resource_recipe::MoneyTransferPolicy::spendable_debit_original_credit);if(!money.ok)return {false,{},money.error};}
    const Owner before{Owner::Kind::actor,t.predecessor_actor_id,{}},after{Owner::Kind::actor,t.successor_actor_id,{}};
    for(const auto& goods:fresh.selection.goods)recipe.operations.push_back(TransferLots{t.source_inventory_id,t.destination_inventory_id,goods.item_id,goods.quantity,{before},{},after});
    for(const auto& id:{t.source_wallet_id,t.destination_wallet_id})recipe.expected_versions.push_back(common.read_version({ResourceKey::Kind::wallet,id}));
    for(const auto& id:{t.source_inventory_id,t.destination_inventory_id})recipe.expected_versions.push_back(common.read_version({ResourceKey::Kind::inventory,id}));
    return {true,std::move(recipe),{}};
}
}
