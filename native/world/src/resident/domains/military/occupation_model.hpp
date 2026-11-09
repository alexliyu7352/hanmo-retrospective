// 占领永久账只保存原公库清单、私人寄存快照与看管转移；资产仍归共同资源表唯一持有。
#pragma once
#include "battle_world_model.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct OccupationPublicLot { StableId id,item_id;Integer quantity=0;FieldOrder field_order; };
struct OccupationWalletManifest { Owner from_owner,to_owner;Integer balance=0;FieldOrder field_order; };
struct OccupationInventoryManifest { Owner from_owner,to_owner;std::vector<OccupationPublicLot> public_lots;std::vector<LotView> protected_lots;FieldOrder field_order; };
struct OccupationAssetManifest { std::vector<std::pair<StableId,OccupationWalletManifest>> wallets;std::vector<std::pair<StableId,OccupationInventoryManifest>> inventories;FieldOrder field_order; };
struct OccupationCustodyTransfer { StableId person_id,from_army_id,to_army_id;FieldOrder field_order; };
enum class MilitaryOccupationProducerKind {calendar_siege_results,strategy,integration_transfer};
struct MilitaryOccupationTime2 {WorldMoment actual_occupation_at;MilitaryOccupationProducerKind producer_kind=MilitaryOccupationProducerKind::calendar_siege_results;SourceRef trigger_source;};
struct MilitaryOccupationFact { StableId id,phase="completed";Integer tick=0;StableId siege_id,campaign_id,army_id,stronghold_id,garrison_id,from_organization_id,to_organization_id,previous_occupation_id;std::vector<StableId> public_inventory_ids,public_wallet_ids;std::optional<bool> assets_applied;std::optional<OccupationAssetManifest> asset_manifest;std::optional<std::vector<OccupationCustodyTransfer>> custody_transfers;std::optional<std::vector<StableId>> disabled_office_ids;std::optional<std::vector<MilitaryActorChange>> actor_changes;FieldOrder field_order;std::optional<MilitaryOccupationTime2> exact_time; };
struct MilitaryReliefFact { StableId id,phase="completed";Integer tick=0;StableId siege_id,campaign_id,army_id,stronghold_id;FieldOrder field_order; };
struct SiegeMaterialFact { StableId id;Integer tick=0;StableId source_id;SiegeAssetEffect effect;FieldOrder effect_field_order,field_order; };
struct MilitaryOwnership { StableId organization_id,occupation_id,stronghold_id;std::vector<StableId> chain; };
struct MilitaryPublicOwnership { Owner owner;StableId occupation_id; };
struct OccupationEffect { bool already_completed=false;std::vector<MilitaryActorChange> actor_changes;std::vector<StableId> disabled_office_ids; };
}
