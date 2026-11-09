// P1 作者前置与原 Calendar 的同 World 拥有组合；外层仍须保活跨域来源。
#pragma once
#include "political_native_lease.hpp"
#include "political_p1_prerequisites.hpp"
#include "resident/domains/organization/organization_resource_validation_reads.hpp"
namespace hanmo::resident {
// Common/Org/Runtime/Calendar borrow the actual World. The optional concrete
// budget reader is retained by the same outer owner and checked against every
// original root by P1; a generic caller-supplied budget provider is not accepted.
struct NativePoliticalP1SourceInputs {
  const ReadStamp &stamp;
  const EconomicFacilityInstallationReadContext *installation = nullptr;
  const PoliticalFiscalTime2Context *fiscal = nullptr;
  const PoliticalTakeoverContext *takeover = nullptr;
  const GovernanceProgramReadContext *governance = nullptr;
  const MilitaryFormationSourceRead *formations = nullptr;
  const MilitaryReturnSourceRead *returns = nullptr;
  const NativeMilitaryTrainingPeriodRead *periods = nullptr;
  const PoliticalBudgetRead *budgets = nullptr;
};
struct NativePoliticalP1ReadInputs {
  NativePoliticalP1SourceInputs original;
  const ActionTimeOriginalSourceRead &action_time;
  const LifeAgeSources &ages;
  const VerifiedDeathRead &deaths;
  const SpatialCarrierSources &spatial;
  const NativeOrganizationResourceValidationReads &organization_resources;
  const PublicMarketStaffingContext *staffing = nullptr;
  const PopulationMilitaryReadContext *population_military = nullptr;
  const PoliticalTerritoryRead *territories = nullptr;
  const PoliticalTreatyExactReadBinding *treaty_exact = nullptr;
};
struct NativePoliticalP1WriteInputs {
  NativePoliticalP1ReadInputs read;
  const MembershipReleaseRead &membership_releases;
  ProductionPopulationChangeContext &production_population;
  PublicMarketStaffingEffectsWrite *staffing_effects = nullptr;
  PoliticalTreatyExactWriteBinding *treaty_exact = nullptr;
};
// 早期原 body 初始化只取前置来源；此函数只验 bindings，不能冒完整 source
// ready。 初始化写入后须按新读票重建下方 lease，不能继续借旧 stamp。
Outcome<std::unique_ptr<NativePoliticalP1Prerequisites>>
make_native_political_p1_prerequisites(const WorldBundle &, const WorldMoment &,
                                       const NativePoliticalP1SourceInputs &);
// 仅原 body 安装后调用；内部必验 selected authored/Office 来源，再接原完整
// lease。 返回体持有所有本域 context，借入的跨域 reader/stamp 仍由中央
// retaining scope 保活。
Outcome<std::unique_ptr<PoliticalExactReadLease>>
make_native_political_p1_read_lease(const WorldBundle &, const WorldMoment &,
                                    NativePoliticalP1ReadInputs);
Outcome<std::unique_ptr<PoliticalExactWriteLease>>
make_native_political_p1_write_lease(WorldBundle &, const WorldMoment &,
                                     NativePoliticalP1WriteInputs);
} // namespace hanmo::resident
