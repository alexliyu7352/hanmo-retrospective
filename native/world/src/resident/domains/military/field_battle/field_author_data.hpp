// field_battle/1 作者资源装载边界；与唯一 prepared.data 共用内容身份。
#pragma once
#include "field_generation.hpp"
#include <filesystem>
namespace hanmo::resident {
struct TravelCatalog;
// 冷装载借用的资源读口；身份沿原 CatalogIdentity，不形成第二套内容版本。
class FieldBattleResourceRead {
public:
  virtual ~FieldBattleResourceRead() = default;
  virtual const CatalogIdentity &identity() const noexcept = 0;
  virtual Outcome<CheckpointVector<std::uint8_t>>
  read(const StableId &relative_author_path) const = 0;
};
// 宿主明确授予目录；不读取 CWD，不接受存档路径，也不允许符号链接越过该根。
class DirectoryFieldBattleResources final : public FieldBattleResourceRead {
public:
  DirectoryFieldBattleResources(CatalogIdentity, std::filesystem::path root);
  const CatalogIdentity &identity() const noexcept override;
  Outcome<CheckpointVector<std::uint8_t>>
  read(const StableId &relative_author_path) const override;

private:
  CatalogIdentity identity_;
  std::filesystem::path root_;
};
// 独立拥有真实字节；provider 和临时 JSON 销毁后仍可安全采样，绝不存入世界银行。
struct FieldBattleCatalog {
  CatalogIdentity identity;
  FieldBattleParameters parameters;
  CheckpointMap<StableId, FieldTerrainData> maps;
  // 只保留原作者种类；位置与道路折线始终沿 TravelCatalog 读取。
  CheckpointMap<StableId, StableId> settlement_kinds;
  // 职名来自本次同内容锁的原 name_key/texts，不读取运行 UI 语言。
  CheckpointMap<StableId, StableId> office_titles;
};
// 准入身份必须逐项一致；只向 provider 请求作者目录下的四个固定相对文件。
Outcome<FieldTerrainData>
load_field_battle_terrain(const BoundaryValue &data, const StableId &map_id,
                          const CatalogIdentity &,
                          const FieldBattleResourceRead &);
// 只装载本次作者数据明确标记 fine_terrain 的地图；不为无资源地图补平地。
Outcome<FieldBattleCatalog> prepare_field_battle_catalog(
    const BoundaryValue &data, const StableId &scenario_id,
    const CatalogIdentity &, const FieldBattleResourceRead &);
// 沿原 TravelCatalog 路线出口组装临时几何，只补原
// settlement.kind，绝不生成假据点。
Outcome<FieldRoadGeometry>
read_field_road_geometry(const BoundaryValue &data, const TravelCatalog &,
                         const FieldTerrainMetadata &,
                         const StableId &route_id);
// 从同一道路两端的原地图归属选择实际细地形；绝不选 maps.begin() 或猜图。
Outcome<const FieldTerrainData *>
read_field_road_terrain(const TravelCatalog &, const FieldBattleCatalog &,
                        const StableId &route_id);
// 热生成与冷恢复只消费 owned 作者种类和原 TravelCatalog，不保留 Boundary JSON。
Outcome<FieldRoadGeometry>
read_field_road_geometry(const TravelCatalog &, const FieldBattleCatalog &,
                         const FieldTerrainMetadata &,
                         const StableId &route_id);
// 内容装载阶段仅读取不可变作者几何；与目录入口共用原折线叶，不构造世界表。
Outcome<FieldRoadGeometry>
read_field_road_geometry(const BoundaryValue &data,
                         const FieldTerrainMetadata &,
                         const StableId &route_id);
// 参数必须由作者选定块逐字段提供，不靠隐藏运行时默认值通过另一配置。
Outcome<FieldBattleParameters>
decode_field_battle_parameters(const BoundaryValue &selected);
} // namespace hanmo::resident
