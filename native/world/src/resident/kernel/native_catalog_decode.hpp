// Original catalog producer with a durable owner for connected ordinary copies.
#pragma once
#include "resident/checkpoint/checkpoint_capacity.hpp"
#include "resident/checkpoint/checkpoint_retention.hpp"
#include <string_view>
#include <optional>
#include <vector>
namespace hanmo::resident {
struct BoundaryValue;struct PreparedCatalog;class FieldBattleResourceRead;
// The existing native prepare lazy read, shared by both original catalog
// factories. It borrows their immutable input and the active original Boundary
// decode budget; it neither validates capabilities nor creates another ledger.
class NativeCatalogCapabilities {
public:
    explicit NativeCatalogCapabilities(const BoundaryValue& data)noexcept:data_(data){}
    NativeCatalogCapabilities(const NativeCatalogCapabilities&)=delete;
    const std::vector<StableId>& operator()();
private:
    const BoundaryValue& data_;
    std::optional<std::vector<StableId>> value_;
};
// Original admitted module list only. Capacity is reserved before its actual
// allocation; these helpers neither register modules nor alter their selection.
void reserve_native_catalog_modules(CatalogIdentity&,std::size_t count);
void append_native_catalog_module(CatalogIdentity&,std::string_view name,std::string_view version);
// Fresh identity copy for the original manual catalog decoders. The same
// active decode owner keeps its ordinary reservation through adoption; this
// neither changes an identity nor turns the incomplete catalog into ExactSet.
CatalogIdentity copy_native_catalog_identity_for_decode(const CatalogIdentity&);
class NativeCatalogDecodeValue {
public:
    NativeCatalogDecodeValue()noexcept=default;
    NativeCatalogDecodeValue(const NativeCatalogDecodeValue&)=delete;
    NativeCatalogDecodeValue& operator=(const NativeCatalogDecodeValue&)=delete;
    NativeCatalogDecodeValue(NativeCatalogDecodeValue&&)noexcept=default;
    NativeCatalogDecodeValue& operator=(NativeCatalogDecodeValue&&)noexcept;
    const PreparedCatalog* get()const noexcept{return catalog_.get();}
    Status retain(CheckpointRetentionCensus&)const;
private:
    friend CheckpointJobResult<NativeCatalogDecodeValue> decode_native_catalog_owned(const BoundaryValue&,const StableId&,const CatalogIdentity&,const CapacityProfile&,CheckpointJobBudget&,std::shared_ptr<CheckpointMemoryResource>,const FieldBattleResourceRead*);
    CheckpointJobReservation ordinary_;
    std::shared_ptr<const PreparedCatalog> catalog_;
};
// Engineering installation entry. The overload with a job uses that same
// job's actual Boundary budget; the other creates the original engineering job.
// Both return the original immutable catalog with its connected ordinary token.
// Business errors keep the original Outcome; technical failures retain their
// owning FailureException. This does not pay later native seed initialization.
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog_for_install(const BoundaryValue&,const StableId& scenario,const CatalogIdentity& expected,CheckpointJobBudget&,const FieldBattleResourceRead* resources=nullptr);
Outcome<std::shared_ptr<const PreparedCatalog>> prepare_native_catalog_for_install(const BoundaryValue&,const StableId& scenario,const CatalogIdentity& expected,const FieldBattleResourceRead* resources=nullptr);
// This is original semantic preparation plus actual connected-copy ownership.
// It cannot attest full input-budget closure or issue an ExactSet.
CheckpointJobResult<NativeCatalogDecodeValue> decode_native_catalog_owned(const BoundaryValue&,const StableId& scenario,const CatalogIdentity& expected,const CapacityProfile&,CheckpointJobBudget&,std::shared_ptr<CheckpointMemoryResource>,const FieldBattleResourceRead* resources=nullptr);
}
