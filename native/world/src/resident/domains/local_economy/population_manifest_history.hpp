// 原 S5 人口/实际接受劳力与 Common entry
// 证明的历史读面；不保存第二份人数或库存。
#pragma once
#include "population_transport.hpp"
#include "resident/domains/travel/group_transport_registration.hpp"
#include "resident/domains/travel/transport_manifest_history.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
Outcome<std::uint64_t>
read_population_transport_domain_revision(const LocalEconomyState &,
                                          const PopulationTransportState &,
                                          const DomainReadKey &);
Outcome<Integer>
read_population_manifest_revision(const LocalEconomyState &,
                                  const PopulationTransportState &,
                                  const SourceRef &);
Outcome<TransportPopulationSource> read_population_transport_manifest_history(
    const CommonStore &, const LocalEconomyState &,
    const PopulationTransportState &, const TransportRegistrationState &,
    const PopulationTransportSourceRead &, const SourceRef &population_ref,
    Integer population_revision, const WorldMoment &actual_entry_at);
Status validate_population_transport_manifest_history(
    const CommonStore &, const LocalEconomyState &,
    const PopulationTransportState &, const TransportRegistrationState &,
    const PopulationTransportSourceRead &, const TransportPopulationManifest &,
    const WorldMoment &actual_entry_at);
class NativeEconomicTransportManifestHistoryRead final
    : public TransportManifestHistorySourceRead {
public:
  NativeEconomicTransportManifestHistoryRead(
      const CommonStore &common, const CommonCatalog &catalog,
      const LocalEconomyState &economy,
      const PopulationTransportState &population,
      const TransportRegistrationState &registrations,
      const PopulationTransportSourceRead &sources,
      const InventoryEntryOriginalSourceRead &entries,
      const InventoryEntryItemSources *items,
      const MilitaryTransportManifestHistoryRead *military = nullptr)
      : common_(common), catalog_(catalog), economy_(&economy),
        population_(&population), registrations_(registrations),
        sources_(&sources), entries_(entries), items_(items),
        military_(military) {}
  // 显式纯军编组：原军事 composition 拒民口，库存证据仍走同一原验证。
  // 不构空人口业务对象；实际出现民口引用必须拒绝，不授未选商业能力。
  NativeEconomicTransportManifestHistoryRead(
      const CommonStore &common, const CommonCatalog &catalog,
      const TransportRegistrationState &registrations,
      const InventoryEntryOriginalSourceRead &entries,
      const InventoryEntryItemSources *items,
      const MilitaryTransportManifestHistoryRead &military)
      : common_(common), catalog_(catalog), economy_(nullptr),
        population_(nullptr), registrations_(registrations), sources_(nullptr),
        entries_(entries), items_(items), military_(&military) {}
  bool bound_to(
      const CommonStore &common,
      const TransportRegistrationState &registrations) const noexcept override {
    return &common == &common_ && &registrations == &registrations_;
  }
  Outcome<Integer>
  population_manifest_revision(const SourceRef &) const override;
  Outcome<TransportPopulationSource>
  read_population_manifest_history(const SourceRef &, Integer,
                                   const WorldMoment &) const override;
  Status validate_inventory_manifest_history(
      const TransportInventoryManifest &) const override;
  Status
  validate_population_manifest_history(const TransportPopulationManifest &,
                                       const WorldMoment &) const override;
  Status validate_military_manifest_history(
      const TransportEntryManifestSource &) const override;
  Outcome<TransportManifestDerivedTotals> read_military_manifest_derived(
      const TransportEntryManifestSource &) const override;

private:
  const CommonStore &common_;
  const CommonCatalog &catalog_;
  const LocalEconomyState *economy_;
  const PopulationTransportState *population_;
  const TransportRegistrationState &registrations_;
  const PopulationTransportSourceRead *sources_;
  const InventoryEntryOriginalSourceRead &entries_;
  const InventoryEntryItemSources *items_;
  const MilitaryTransportManifestHistoryRead *military_;
};
} // namespace hanmo::resident
