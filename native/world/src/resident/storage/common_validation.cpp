// 共同profile校验直接检查typed资产/人物/占用；不把GD种子上的ok当独立验证。
#include "common_store.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <set>
#include <charconv>
namespace hanmo::resident {
namespace {
// 原Schema保类型及实例上界；native共同缓存只接收可表示的成功回执，不补推历史或请求序号。
Status validate_cached_receipt_shells(const CommonState& state){
    for(const auto& row:state.results.rows()){
        const auto& result=row.second;
        if(!result||!result->ok||result->kind.empty()||result->tick<0||result->tick>state.identity.tick||result->revision<1||result->revision>state.identity.revision)return common_failure("InvalidCachedResult","results/"+row.first);
        const auto& id=result->result_id;if(id.rfind("result:",0)!=0)return common_failure("InvalidCachedResult","results/"+row.first);auto suffix=id.substr(7);Integer number=0;auto decoded=std::from_chars(suffix.data(),suffix.data()+suffix.size(),number);if(decoded.ec!=std::errc{}||decoded.ptr!=suffix.data()+suffix.size()||number<1||std::to_string(number)!=suffix)return common_failure("InvalidCachedResult","results/"+row.first);if(number>=state.next_instance_id)return common_failure("InstanceCounterCollision",id);
    }
    return common_success();
}
} // 缺失旧缓存仍按原AlreadyApplied处理；不新增cache key/history强一致性要求。
Status CommonStore::validate_common()const{
    auto whole=[&](Integer n,Integer low=0,Integer high=INT64_MAX){const auto limit=state_.metadata.ruleset_id==dynamic_ruleset_id?high:std::min<Integer>(high,2147483647);return n>=low&&n<=limit;};
    if(!catalog_)return common_failure("NoCatalog");
    if(!whole(tick())||!whole(revision())||!whole(state_.next_instance_id,1))return common_failure("InvalidCounter");
    if(!actor(state_.metadata.player_actor_id))return common_failure("InvalidPlayerActor");
    std::set<StableId> active(catalog_->actor_ids.begin(),catalog_->actor_ids.end());for(auto& id:catalog_->actor_ids)if(!actor(id))return common_failure("MissingWorldEntities",id);
    if(state_.roles.size()!=catalog_->roles.size())return common_failure("MissingWorldEntities","roles");
    for(auto& row:state_.actors.rows()){
        auto& a=row.second;auto* seq=state_.request_sequences.find(row.first);
        if(a.id!=row.first||!whole(a.hp,0,100)||!whole(a.fatigue,0,100)||!whole(a.merit)||!seq||!whole(*seq))return common_failure("InvalidActorState",row.first);
        if(!wallet(a.wallet_id)||!inventory(a.inventory_id))return common_failure("ActorAssetsMissing",row.first);
        for(auto n:{a.attributes.administration,a.attributes.charisma,a.attributes.intellect,a.attributes.leadership,a.attributes.martial})if(!whole(n,1,110))return common_failure("InvalidAttribute",row.first);
        for(auto n:{a.skills.archery,a.skills.cavalry,a.skills.commerce,a.skills.debate,a.skills.infantry,a.skills.martial,a.skills.negotiation,a.skills.rites,a.skills.tactics,a.skills.transport})if(!whole(n,0,1200))return common_failure("InvalidSkill",row.first);
        if(a.skills.extended.has_value()!=catalog_->extended_skills)return common_failure("ActorSkillProfileMismatch",row.first);
        if(a.skills.extended){const auto& x=*a.skills.extended;for(auto n:{x.naval,x.espionage,x.fortification,x.agriculture,x.medicine,x.forging})if(!whole(n,0,1200))return common_failure("InvalidExtendedSkill",row.first);}
        if(auto* p=std::get_if<SettlementLocation>(&a.location)){auto* place=catalog_->places.find(p->place_id);if(!place||place->settlement_id!=p->settlement_id)return common_failure("InvalidPlace",row.first);}
        if(const auto* carried=std::get_if<CarriedLocation>(&a.location)){if(!catalog_->carrier_location_enabled)return common_failure("CarrierLocationProfileRequired",row.first,ErrorKind::unsupported_capability);if(carried->carrier_id.empty()||(carried->carrier_kind!=CarriedLocation::CarrierKind::transport_unit&&carried->carrier_kind!=CarriedLocation::CarrierKind::vessel))return common_failure("InvalidActorCarriedLocation",row.first);if(!carried_location_validation_)return common_failure("CarriedLocationSourceReaderRequired",row.first,ErrorKind::unsupported_capability);auto valid=carried_location_validation_->validate_actor_carried_location(row.first,*carried);if(!valid.ok)return valid;}
        if(a.injury<0||a.injury>3)return common_failure("InvalidInjury",row.first);
        if(!a.appearance)return common_failure("InvalidActorAppearance",row.first);
        auto* initial=catalog_->actor_initial_birth.find(row.first);
        if(!initial){
            if(!family_identity_)return common_failure("DynamicIdentityCapabilityRequired",row.first,ErrorKind::unsupported_capability);
            auto valid=family_identity_->validate_dynamic_actor(*this,a);if(!valid.ok)return valid;
        }else{if(!active.count(row.first))return common_failure("InactiveStaticActor",row.first);if(a.prototype_id&&*a.prototype_id!=row.first)return common_failure("BaseActorPrototypeMismatch",row.first);if(*initial&&(!a.birth_tick||*a.birth_tick!=**initial||!a.created_tick||*a.created_tick!=0))return common_failure("BaseActorAgeMismatch",row.first);if((a.created_tick&&*a.created_tick!=0)||(a.birth_tick&&(*a.birth_tick< -2147483647||*a.birth_tick>0)))return common_failure("InvalidBaseActorDate",row.first);}
        auto& ap=*a.appearance;if(ap.format_version!=1)return common_failure("InvalidAppearanceProfile",row.first);if(ap.source=="legacy"){if(ap.generator_version!="legacy-portrait-v1"||ap.generation_seed!="not-sampled"||ap.base_features.present||ap.attire_style!="legacy"||!ap.parent_sources.empty())return common_failure("InvalidLegacyAppearance",row.first);}else {if(ap.generator_version!="appearance-v1")return common_failure("UnsupportedAppearanceGenerator",row.first);if(ap.source=="inherited"||!ap.parent_sources.empty())return common_failure("AppearanceInheritanceCapabilityRequired",row.first,ErrorKind::unsupported_capability);auto token=[](const StableId& t){if(t.empty()||t.size()>64)return false;for(std::size_t i=0;i<t.size();++i){auto ch=t[i];if(!(ch>='a'&&ch<='z')&&(i==0||!((ch>='0'&&ch<='9')||ch=='-'||ch=='.'||ch=='_')))return false;}return true;};if((ap.source!="fixed"&&ap.source!="custom"&&ap.source!="generated")||!ap.base_features.present||!token(ap.attire_style))return common_failure("InvalidAppearanceProfile",row.first);for(auto& t:{ap.base_features.face_shape,ap.base_features.eyes,ap.base_features.brows,ap.base_features.nose,ap.base_features.mouth,ap.base_features.hair,ap.base_features.beard,ap.base_features.skin_palette,ap.base_features.build})if(!token(t))return common_failure("InvalidAppearanceProfile",row.first);if((ap.source=="fixed"||ap.source=="custom")&&ap.generation_seed!="not-sampled")return common_failure("InvalidAppearanceSeed",row.first);if(ap.source=="generated"&&(ap.generation_seed.empty()||ap.generation_seed.size()>256||ap.generation_seed.front()==' '||ap.generation_seed.back()==' '||std::any_of(ap.generation_seed.begin(),ap.generation_seed.end(),[](unsigned char c){return c<32||c==127;})))return common_failure("InvalidAppearanceSeed",row.first);}
        auto& combat=a.combat;if(!combat.weapon_id.empty()){auto* item=catalog_->items.find(combat.weapon_id);if(!item||!item->weapon)return common_failure("UnknownEquippedWeapon",row.first);}if(combat.armor_id&&!combat.armor_id->empty()){auto* item=catalog_->items.find(*combat.armor_id);if(!item||!item->armor)return common_failure("NotAnArmor",row.first);}for(auto& learned:combat.learned.rows())if(!catalog_->personal_skill_kinds.find(learned.first)||!whole(learned.second,0,100))return common_failure("InvalidLearnedSkill",row.first);if(!combat.heart_id.empty()){auto* skill=catalog_->personal_skill_kinds.find(combat.heart_id);if(!combat.learned.find(combat.heart_id)||!skill||*skill!="heart")return common_failure("InvalidHeartConfiguration",row.first);}
    }
    auto valid_owner=[&](const Owner& o){if(o.extra)return false;if(o.kind==Owner::Kind::actor)return actor(o.id)!=nullptr;if(o.kind==Owner::Kind::organization)return state_.metadata.ruleset_id==dynamic_ruleset_id?organization_owners_&&organization_owners_->organization_exists(o.id):std::find(catalog_->organization_ids.begin(),catalog_->organization_ids.end(),o.id)!=catalog_->organization_ids.end();if(o.kind==Owner::Kind::task)return task_owners_&&task_owners_->task_exists(o.id);if(o.kind==Owner::Kind::household)return state_.metadata.ruleset_id==dynamic_ruleset_id&&household_owners_&&household_owners_->household_exists(o.id);return false;};
    for(auto& row:state_.roles.rows())if(!catalog_->roles.find(row.first)||!actor(row.second.actor_id))return common_failure("InvalidRole",row.first);
    for(auto& row:state_.wallets.rows()){
        auto& w=row.second;if(w.id!=row.first||!whole(w.balance)||!valid_owner(w.owner))return common_failure("InvalidWallet",row.first);
        Integer total=0;std::set<ReservationRef> seen;for(auto& r:w.reserved){if(!reservation_exists(r.action_id)||!whole(r.amount)||!seen.insert(r.action_id).second)return common_failure("InvalidMoneyReservation",row.first);if(total>INT64_MAX-r.amount)return common_failure("ProjectionOverflow",row.first);total+=r.amount;}
        if(total>w.balance)return common_failure("OverReservedMoney",row.first);
    }
    for(auto& row:catalog_->required_wallets.rows())if(!wallet(row.first))return common_failure("RequiredWalletMissing",row.first);
    for(auto& row:catalog_->required_inventories.rows())if(!inventory(row.first))return common_failure("RequiredInventoryMissing",row.first);
    for(auto& id:catalog_->organization_ids){auto* i=inventory("store:"+id);if(!i||i->owner.kind!=Owner::Kind::organization||i->owner.id!=id)return common_failure("RequiredReceiverStoreMissing",id);}
    std::set<StableId> lots;
    for(auto& row:state_.inventories.rows()){
        auto& inv=row.second;if(inv.id!=row.first||!whole(inv.capacity,1)||!whole(inv.revision)||!valid_owner(inv.owner))return common_failure("InvalidInventory",row.first);
        for(auto& lot:inv.lots){
            if(lot.item_instance_id&&lot.lot_properties_ref)return common_failure("LotReferenceConflict",lot.id);
            if((lot.item_instance_id&&!catalog_->item_instances_enabled)||(lot.lot_properties_ref&&!catalog_->lot_properties_enabled))return common_failure("LotReferenceCapabilityRequired",lot.id,ErrorKind::unsupported_capability);
            if(lot.item_instance_id||lot.lot_properties_ref){if(!selected_lot_semantics_)return common_failure("LotSemanticReaderRequired",lot.id,ErrorKind::unsupported_capability);auto valid=selected_lot_semantics_->validate_live_lot(*this,inv.id,lot);if(!valid.ok)return valid;}
            if(!catalog_->items.find(lot.item_id)||!whole(lot.quantity,1)||!valid_owner(lot.owner)||lot.extra||!lots.insert(lot.id).second)return common_failure("InvalidLot",lot.id);if((lot.reserved_by.empty()&&lot.reserved_by.kind!=ReservationRef::Kind::action)||(!lot.reserved_by.empty()&&!reservation_exists(lot.reserved_by)))return common_failure("DanglingReservation",lot.id);}
        auto weight=inventory_weight(row.first);if(!weight.ok)return {false,{},weight.error};if(weight.value>inv.capacity)return common_failure("OverCapacity",row.first);
    }
    std::set<StableId> actor_wallets,actor_inventories;for(auto& row:state_.actors.rows()){if(!actor_wallets.insert(row.second.wallet_id).second||!actor_inventories.insert(row.second.inventory_id).second)return common_failure("SharedIdentityAssets",row.first);Owner expected{Owner::Kind::actor,row.first,{}};if(!owner_equal(wallet(row.second.wallet_id)->owner,expected)||!owner_equal(inventory(row.second.inventory_id)->owner,expected))return common_failure("ActorAssetOwnership",row.first);}
    for(auto name:{"world","event","duel","battle","minigame"})if(!state_.rng.find(name))return common_failure("InvalidRandomState",name);
    auto replay=validate_cached_receipt_shells(state_);if(!replay.ok)return replay;
    for(auto& row:state_.actions.rows()){
        auto& a=row.second;if(a.id!=row.first||!actor(a.actor_id)||!whole(a.remaining_ticks,action_time_validation_?0:1)||!whole(a.start_tick)||(std::find(a.participants.begin(),a.participants.end(),a.actor_id)==a.participants.end()&&!(a.kind=="study_skill"&&a.participants.empty())))return common_failure("InvalidAction",row.first);
        if(action_time_validation_){auto time=action_time_validation_->validate_action_time(a);if(!time.ok)return time;}
        std::set<StableId> seen;for(auto& id:a.participants){auto* person=actor(id);if(!person||person->action_id!=a.id||!seen.insert(id).second)return common_failure("InvalidOccupation",id);}
    }
    for(auto& row:state_.actors.rows()){auto& a=row.second;if(!a.action_id.empty()){auto* act=action(a.action_id);if(!act||std::find(act->participants.begin(),act->participants.end(),a.id)==act->participants.end())return common_failure("DanglingOccupation",a.id);}}
    std::set<StableId> joined;
    for(auto& row:state_.parties.rows()){
        auto& p=row.second;if(p.id!=row.first||!whole(p.created_tick,0,tick()))return common_failure("InvalidPartyTime",row.first);
        if(p.members.size()<2||p.members.size()>16||p.members[0]!=p.leader_id||p.terms.size()!=p.members.size()-1)return common_failure("InvalidPartyMembers",row.first);
        auto* leader=actor(p.leader_id);if(!leader)return common_failure("UnknownPartyLeader",row.first);if(std::holds_alternative<FollowingLocation>(leader->location)||std::holds_alternative<InArmyLocation>(leader->location))return common_failure("NestedParty",row.first);
        for(auto& id:p.members){auto* a=actor(id);if(!a||!joined.insert(id).second)return common_failure("DuplicatePartyMember",id);if(membership_&&membership_->active_army_members(id))return common_failure("ArmyPartyOverlap",id);if(id==p.leader_id)continue;auto* loc=std::get_if<FollowingLocation>(&a->location);if(!loc||loc->party_id!=p.id)return common_failure("PartyLocationMismatch",id);auto* term=p.terms.find(id);auto* role=term?catalog_->roles.find(term->role_id):nullptr;if(!role||std::find(role->services.begin(),role->services.end(),"join_party")==role->services.end()||term->cost_per_tick!=role->companion_cost_per_tick)return common_failure("InvalidCompanionTerms",id);}
    }
    for(auto& row:state_.actors.rows())if(std::holds_alternative<FollowingLocation>(row.second.location)&&!joined.count(row.first))return common_failure("DanglingParty",row.first);
    return common_success();
}
} // 地形点有效性、领域payload、跨域历史引用由唯一领域validator继续检查。
