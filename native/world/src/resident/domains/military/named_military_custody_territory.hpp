// 看管释放只借原辖区控制；未选接管时证明原始投影未变，不建立空接管世界。
#pragma once
#include "resident/domains/politics/politics_territory_contract.hpp"
namespace hanmo::resident {
class CommonStore;
class CurrentMomentRead;
class PoliticalTakeoverTerritoryAccess;
struct PoliticalTakeoverContext;
struct NamedMilitaryCustodyRelease;
struct WorldBundle;
// 值只保存借用；旧 concrete reader 隐式接入保持原调用语义。
class NamedMilitaryCustodyTerritorySources {
  const PoliticalTakeoverTerritoryAccess *legacy_ = nullptr;
  const WorldBundle *world_ = nullptr;
  const CurrentMomentRead *clock_ = nullptr;
  const PoliticalTakeoverContext *takeover_ = nullptr;
  Status validate_native_roots() const;
public:
  NamedMilitaryCustodyTerritorySources(const PoliticalTakeoverTerritoryAccess &);
  NamedMilitaryCustodyTerritorySources(const WorldBundle &, const CurrentMomentRead &,
                                      const PoliticalTakeoverContext *);
  // 仅核拥有根身份，不在构造期间获取 Complete lease 或读取业务正文。
  bool bound_to(const WorldBundle &) const noexcept;
  Outcome<const StableId *> military_settlement_jurisdiction(const StableId &) const;
  Outcome<PoliticalTerritoryControl> governance_jurisdiction_control_current(const StableId &) const;
  Outcome<PoliticalTerritoryControl> military_jurisdiction_control_before_custody_release(
      const NamedMilitaryCustodyRelease &, const StableId &) const;
};
} // namespace hanmo::resident
