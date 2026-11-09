#include "hanmo/resident/checkpoint_memory.hpp"
// 作者人物冷初始化；沿既有工厂的资产、年龄、外貌与行旅初值，不执行任何运行规则。
#include "native_initial_state.hpp"
#include "native_role_seed.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include "native_inventory_seed.hpp"
#include "native_actor_social_occupancy.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/travel/travel_model.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <picosha2.h>
#include <charconv>
#include <set>
namespace hanmo::resident {
namespace {
using namespace boundary;
using V=BoundaryValue;
// Original native initialization has no initial Party/Session producer. Both
// the actual seeder and the locked projection consume this same finite branch.
// If that original behavior gains such a source, this one visitor must change.
template<class Visitor>Status visit_native_authored_actor_rows(const V& data,const V& initial,Visitor visitor){
    const auto& rows=array(field(initial,"actors","initial"),"initial/actors");
    for(std::size_t n=0;n<rows.size();++n){
        const auto& row=rows[n];const auto id=get_string(row,"actor_id","initial/actors");
        if(id.empty())fail("InvalidInitialActorId","initial/actors");
        for(std::size_t prior=0;prior<n;++prior)
            if(get_string(rows[prior],"actor_id","initial/actors")==id)fail("DuplicateInitialActorId",id);
        const auto& definition=field(field(data,"actors","data"),id,"actors");
        auto applied=visitor(row,id,definition,ActorSocialOccupancy{});if(!applied.ok)return applied;
    }
    return common_success();
}
// The original role seeder and immutable catalog projection share the same
// authored rows. Reject duplicate IDs here: the old insert ignored duplicates
// while effective_catalog's actor override was last-wins.
template<class Visitor>Status visit_native_authored_role_rows(const V& initial,Visitor visitor){
    const auto& rows=array(field(initial,"roles","initial"),"initial/roles");
    for(std::size_t n=0;n<rows.size();++n){
        const auto& row=rows[n];
        const auto& id=get_string_ref(row,"role_id","initial/roles");
        for(std::size_t prior=0;prior<n;++prior)
            if(get_string_ref(rows[prior],"role_id","initial/roles")==id)
                fail("DuplicateInitialRoleId",id);
        const auto& actor=get_string_ref(row,"actor_id","initial/roles");
        const bool enabled=get_bool(row,"enabled","initial/roles");
        auto applied=visitor(id,actor,enabled);if(!applied.ok)return applied;
    }
    return common_success();
}
// 外貌稳定抽样与既有appearance-v1共用规范JSON输入，不消耗玩法随机流。
std::shared_ptr<const AppearanceProfile> initial_appearance(const V& definition,const StableId& id,Integer seed){
    auto result=checkpoint_make_shared<AppearanceProfile>();result->generation_seed="not-sampled";
    const auto* appearance=definition.find("appearance_definition");
    if(!appearance){result->generator_version="legacy-portrait-v1";result->attire_style="legacy";result->source="legacy";return result;}
    const auto kind=get_string(*appearance,"kind",id);if(get_string(*appearance,"generator_version",id)!="appearance-v1")unsupported("UnsupportedAppearanceGenerator",id);
    if(kind!="fixed"&&kind!="generated")fail("InvalidAppearanceDefinition",id);
    result->generator_version="appearance-v1";result->attire_style=get_string(*appearance,"attire_style",id);result->source=kind;
    result->base_features.present=true;if(kind=="generated")result->generation_seed=std::to_string(seed);
    auto& f=result->base_features;
    for(auto entry:{std::pair<const char*,StableId*>{"face_shape",&f.face_shape},{"eyes",&f.eyes},{"brows",&f.brows},{"nose",&f.nose},{"mouth",&f.mouth},{"hair",&f.hair},{"beard",&f.beard},{"skin_palette",&f.skin_palette},{"build",&f.build}}){
        if(kind=="fixed")*entry.second=get_string(field(*appearance,"base_features",id),entry.first,id);
        else{
            auto choices=strings(field(field(*appearance,"candidates",id),entry.first,id),id);std::sort(choices.begin(),choices.end());
            if(choices.empty()||std::adjacent_find(choices.begin(),choices.end())!=choices.end())fail("InvalidAppearanceDefinition",id);
            auto encoded=render_boundary_json(V{V::Array{V{StableId{"appearance-v1"}},V{result->generation_seed},V{id},V{StableId{entry.first}}}});
            if(!encoded.ok)throw DecodeFailure(encoded.error.original.code,encoded.error.original.path);
            const auto digest=picosha2::hash256_hex_string(encoded.value);std::uint32_t index=0;
            auto parsed=std::from_chars(digest.data(),digest.data()+8,index,16);if(parsed.ec!=std::errc{})fail("InvalidAppearanceSeed",id);
            *entry.second=choices[index%choices.size()];
        }
    }
    return result;
}
// 每件初货分配本候选唯一lot身份；失败候选的序号不会写回任何活世界。
void add_goods(CommonState& common,InventoryView& inventory,const V* source,const StableId& path){
    if(!source)return;
    for(const auto& good:array(*source,path)){
        LotView lot;lot.id="lot:"+std::to_string(common.next_instance_id++);lot.item_id=get_string(good,"item_id",path);lot.quantity=get_int(good,"quantity",path);lot.owner=inventory.owner;
        if(lot.quantity<=0)fail("InvalidInitialGoods",path);inventory.lots.push_back(std::move(lot));
    }
}
// 两个入口共用原库存/背包lot分配器；完整人物初始化仍在原位置执行，保持旧正常顺序。
void seed_authored_inventories(CommonState& common,const V& data){
    for(const auto& row:object(field(data,"inventories","data"),"inventories")){
        InventoryView inventory;inventory.id=row.first;inventory.owner=decode_owner(field(row.second,"owner",row.first),row.first);inventory.capacity=get_int(row.second,"capacity",row.first);
        add_goods(common,inventory,row.second.find("goods"),"inventories/"+row.first+"/goods");common.inventories.insert(row.first,std::move(inventory));
    }
}
InventoryView seed_actor_bag(CommonState& common,const V& row){
    const auto id=get_string(row,"actor_id","initial/actors");InventoryView bag;bag.id="bag:"+id;bag.owner.kind=Owner::Kind::actor;bag.owner.id=id;bag.capacity=20;
    add_goods(common,bag,row.find("initial_goods"),id+"/initial_goods");return bag;
}
void seed_organization_stores(CommonState& common,const V& data){
    for(const auto& row:object(field(data,"organizations","data"),"organizations")){
        InventoryView store;store.id="store:"+row.first;store.owner.kind=Owner::Kind::organization;store.owner.id=row.first;store.capacity=2147483647;
        const auto store_id=store.id;if(!common.inventories.insert(store_id,std::move(store)))fail("OrganizationStoreCollision",row.first);
    }
}
}
// 先有作者容器，再建人物专属袋和缺省私钱包；不复用庄园钱包充当人物钱包。
Status seed_native_actors(CommonState& common,const BoundaryValue& data,const BoundaryValue& initial,TravelState& travel,InteractionState& interaction,bool extended_skills,const ActorFoundationCatalog* foundation){try{
    using namespace boundary;
    for(const auto& row:object(field(data,"wallets","data"),"wallets")){
        WalletView wallet;wallet.id=row.first;wallet.owner=decode_owner(field(row.second,"owner",row.first),row.first);wallet.balance=get_int(row.second,"initial_balance",row.first);
        if(!common.wallets.insert(row.first,std::move(wallet)))fail("DuplicateWallet",row.first);
    }
    seed_authored_inventories(common,data);
    std::set<StableId> estate_wallets;if(const auto* estates=data.find("estates"))for(const auto& row:object(*estates,"estates"))estate_wallets.insert(get_string(row.second,"wallet_id",row.first));
    auto seeded=visit_native_authored_actor_rows(data,initial,[&](const V& row,const StableId& id,const V& definition,const ActorSocialOccupancy& occupancy)->Status{
        if(occupancy.interaction_id||occupancy.party_id)fail("NativeInitialOccupancyWriterRequired",id);
        ActorRecord actor;actor.id=id;actor.alive=get_bool(row,"alive",id);actor.available=get_bool(row,"available",id);if(!foundation)actor.hp=100;
        const auto place=get_string(row,"place_id",id);const auto& location=field(field(data,"places","data"),place,"places");actor.location=SettlementLocation{get_string(location,"settlement_id",place),place};
        actor.appearance=initial_appearance(definition,id,common.rng.find("world")->seed);if(foundation){auto authored=initialize_authored_actor_foundation_actor(actor,*foundation,id);if(!authored.ok)return authored;}else{const auto age=get_int(row,"age_years",id);if(age<0||age>120)fail("InvalidScenarioActorAge",id);actor.birth_tick=-age*720;actor.created_tick=0;}
        const auto& attributes=field(row,"attributes",id);auto& a=actor.attributes;
        a.administration=get_int(attributes,"administration",id);a.charisma=get_int(attributes,"charisma",id);a.intellect=get_int(attributes,"intellect",id);a.leadership=get_int(attributes,"leadership",id);a.martial=get_int(attributes,"martial",id);
        const auto& skills=field(row,"skills",id);auto& s=actor.skills;
        if(extended_skills)fields(skills,{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport","naval","espionage","fortification","agriculture","medicine","forging"},{},id+"/skills");
        else fields(skills,{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport"},{},id+"/skills");
        if(extended_skills){s.extended.emplace();auto& x=*s.extended;for(auto item:{std::pair<const char*,Integer*>{"naval",&x.naval},{"espionage",&x.espionage},{"fortification",&x.fortification},{"agriculture",&x.agriculture},{"medicine",&x.medicine},{"forging",&x.forging}})*item.second=get_int(skills,item.first,id);}
        s.archery=get_int(skills,"archery",id);s.cavalry=get_int(skills,"cavalry",id);s.commerce=get_int(skills,"commerce",id);s.debate=get_int(skills,"debate",id);s.infantry=get_int(skills,"infantry",id);s.martial=get_int(skills,"martial",id);s.negotiation=get_int(skills,"negotiation",id);s.rites=get_int(skills,"rites",id);s.tactics=get_int(skills,"tactics",id);s.transport=get_int(skills,"transport",id);
        const auto& combat=field(row,"combat",id);if(combat.find("weapon_id"))actor.combat.weapon_id=get_string(combat,"weapon_id",id);if(combat.find("armor_id"))actor.combat.armor_id=get_string(combat,"armor_id",id);
        if(const auto* learned=combat.find("learned"))for(const auto& skill:array(*learned,id))actor.combat.learned.insert(get_string(skill,"skill_id",id),get_int(skill,"mastery",id));
        Owner owner;owner.kind=Owner::Kind::actor;owner.id=id;
        auto wallets=common.wallets.order();std::sort(wallets.begin(),wallets.end());for(const auto& wallet_id:wallets)if(!estate_wallets.count(wallet_id)&&owner_equal(common.wallets.find(wallet_id)->owner,owner)){actor.wallet_id=wallet_id;break;}
        if(actor.wallet_id.empty()){actor.wallet_id="personal:"+id;WalletView wallet;wallet.id=actor.wallet_id;wallet.owner=owner;if(!common.wallets.insert(actor.wallet_id,std::move(wallet)))fail("ActorAssetsCollision",id);}
        actor.inventory_id="bag:"+id;auto bag=seed_actor_bag(common,row);
        if(!common.inventories.insert(actor.inventory_id,std::move(bag))||!common.actors.insert(id,std::move(actor)))fail("ActorAssetsCollision",id);
        common.request_sequences.insert(id,0);interaction.knowledge.push_back({id,{}});
        if(const auto* plan=definition.find("travel_plan")){TravelItinerary itinerary;itinerary.ready_tick=get_int(*plan,"start_tick",id);travel.itineraries.insert(id,std::move(itinerary));}
        return common_success();
    });if(!seeded.ok)return seeded;
    seed_organization_stores(common,data);
    auto roles=visit_native_authored_role_rows(initial,[&](const StableId& id,const StableId& actor,bool enabled)->Status{
        common.roles.insert(checkpoint_decode_boundary_string(id),
            {checkpoint_decode_boundary_string(actor),enabled,{}});
        return common_success();
    });if(!roles.ok)return roles;
    for(const auto& row:object(field(data,"flags","data"),"flags"))interaction.flags.push_back({row.first,boolean(row.second,"flags/"+row.first)});
    return common_success();
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status bind_native_authored_role_seed(CommonCatalog& catalog,const BoundaryValue& initial){try{
    return visit_native_authored_role_rows(initial,[&](const StableId& id,const StableId& actor,bool enabled)->Status{
        auto* role=catalog.roles.find(id);
        if(!role||role->initial_actor!=actor)boundary::fail("NativeAuthoredRoleSourceMismatch",id);
        role->initial_enabled=enabled;
        return common_success();
    });
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Outcome<ActorSocialOccupancyInitialCatalog> compile_native_actor_social_occupancy_seed(
    const BoundaryValue& data,const BoundaryValue& initial,const CatalogIdentity& identity,const StableId& scenario){try{
    ActorSocialOccupancyInitialCatalog out;out.identity=identity;out.scenario_id=scenario;
    auto visited=visit_native_authored_actor_rows(data,initial,[&](const V&,const StableId& id,const V&,const ActorSocialOccupancy& value)->Status{
        if(!out.authored_initial.insert(id,value))return common_failure("DuplicateSocialOccupancySeedActor",id);
        return common_success();
    });if(!visited.ok)return {false,{},std::move(visited.error)};
    return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 从同一effective Installed初态重建唯一库存分配序；没有当前World输入，也不消费人物/RNG。
Outcome<NativeInventorySeed> seed_native_inventory_assets(const BoundaryValue& data,const BoundaryValue& initial){try{
    CommonState common;seed_authored_inventories(common,data);
    for(const auto& row:boundary::array(boundary::field(initial,"actors","initial"),"initial/actors")){
        auto bag=seed_actor_bag(common,row);const auto id=bag.id;
        if(!common.inventories.insert(id,std::move(bag)))boundary::fail("ActorAssetsCollision",boundary::get_string(row,"actor_id","initial/actors"));
    }
    seed_organization_stores(common,data);return {true,{std::move(common.inventories),common.next_instance_id},{}};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
} // 本文件不安装世界、不检查GUI草稿，不保存作者JSON到人物记录。
