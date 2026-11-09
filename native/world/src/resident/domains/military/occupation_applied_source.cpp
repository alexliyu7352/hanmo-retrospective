#include "resident/domains/military/military_model.hpp"
// 仅在原 apply 确实完成转权后交出清单；幂等重放不再次通知政治或经济领域。
#include "occupation_applied_source.hpp"
#include "occupation_codec.hpp"
#include "resident/model/common_operations.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 来源错误保持原候选失败，不补造占领凭据或更改时钟来通过校验。
Outcome<OccupationPhaseApplication> fail(const StableId& code){return {false,{},common_failure(code).error};}
template<class T> const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& row:rows)if(row.id==id)return &row;return nullptr;}
bool same_ids(std::vector<StableId> a,std::vector<StableId> b){std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b&&std::adjacent_find(a.begin(),a.end())==a.end();}
// 当前候选逐容器验证实际转权；原apply已逐预约核证并比较临时快照，保护批次仍逐字匹配。
bool applied_manifest(const CommonRead& common,const MilitaryOccupationFact& fact){
    if(!fact.assets_applied.value_or(false)||!fact.asset_manifest)return false;
    const Owner old{Owner::Kind::organization,fact.from_organization_id,{}},next{Owner::Kind::organization,fact.to_organization_id,{}};
    std::vector<StableId> wallets,inventories;
    for(const auto& [id,row]:fact.asset_manifest->wallets){
        wallets.push_back(id);const auto* current=common.wallet(id);
        if(!current||!owner_equal(row.from_owner,old)||!owner_equal(row.to_owner,next)||!owner_equal(current->owner,next)||current->balance!=row.balance)return false;
    }
    for(const auto& [id,row]:fact.asset_manifest->inventories){
        inventories.push_back(id);const auto* current=common.inventory(id);
        if(!current||!owner_equal(row.from_owner,old)||!owner_equal(row.to_owner,next)||!owner_equal(current->owner,next)||current->lots.size()!=row.public_lots.size()+row.protected_lots.size())return false;
        std::set<StableId> seen;
        for(const auto& lot:row.public_lots){const auto* actual=find(current->lots,lot.id);if(!seen.insert(lot.id).second||!actual||actual->item_id!=lot.item_id||actual->quantity!=lot.quantity||!owner_equal(actual->owner,next))return false;}
        for(const auto& lot:row.protected_lots){const auto* actual=find(current->lots,lot.id);if(!seen.insert(lot.id).second||!actual||owner_equal(lot.owner,old)||!compare_boundary(occupation_codec::encode(*actual),occupation_codec::encode(lot)).exact())return false;}
    }
    return same_ids(wallets,fact.public_wallet_ids)&&same_ids(inventories,fact.public_inventory_ids);
}
}
// 源身份保留 phase 的原 clock；生效 tick 可以是本次半日终点，但不把来源包装成历史通读。
Outcome<OccupationPhaseApplication> apply_military_occupation_with_source(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryAccess& access,const WorldIdentity& world,const StableId& id,const OccupationReservationRead& reservations){
    if(world.tick!=common.tick()||world.revision!=common.revision()||world.execution_epoch<0||std::none_of(world.world_id.begin(),world.world_id.end(),[](auto byte){return byte!=0;}))return fail("OccupationCandidateIdentityMismatch");
    const auto* before=find(military.occupation_facts,id);
    if(!before||!access.fact_exists||!access.fact_exists(id))return fail("OccupationReceiptSourceMissing");
    if(common.tick()==std::numeric_limits<Integer>::max())return fail("OccupationCandidateClockOverflow");
    if(!before->assets_applied.value_or(false)&&(before->tick<common.tick()||before->tick>common.tick()+1))return fail("OccupationCandidateTickMismatch");
    auto applied=apply_military_occupation(common,military,catalog,access,id,reservations);
    if(!applied.ok)return {false,{},applied.error};
    OccupationPhaseApplication out;out.effect=std::move(applied.value);
    if(out.effect.already_completed)return {true,std::move(out),{}};
    const auto* after=find(military.occupation_facts,id);
    if(!after||common.tick()!=world.tick||common.revision()!=world.revision||!applied_manifest(common,*after))return fail("OccupationAppliedManifestMismatch");
    OccupationAppliedSource source;source.world=world;source.occupation_id=id;source.siege_id=after->siege_id;source.campaign_id=after->campaign_id;source.army_id=after->army_id;source.stronghold_id=after->stronghold_id;source.garrison_id=after->garrison_id;source.from_organization_id=after->from_organization_id;source.to_organization_id=after->to_organization_id;source.previous_occupation_id=after->previous_occupation_id;source.source_tick=world.tick;source.effective_tick=after->tick;source.assets=*after->asset_manifest;out.source=std::move(source);
    return {true,std::move(out),{}};
}
// 旧调用无逐预约证明，仍由原拒绝门阻止已受领责任被隐式转走。
Outcome<OccupationPhaseApplication> apply_military_occupation_with_source(CommonWrite& common,MilitaryState& military,const MilitaryCatalog& catalog,const MilitaryAccess& access,const WorldIdentity& world,const StableId& id){return apply_military_occupation_with_source(common,military,catalog,access,world,id,{});}

}
