// field_battle/1 细地形只读输入；文件读取属于装载边界，规则只消费已核验字节。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include <span>
namespace hanmo::resident {
struct BoundaryValue;
// 作者十进制尺度按有理数保留，规则计算不使用 binary64。
struct FieldScale {
  Integer num = 0, den = 1;
};
struct FieldTerrainMetadata {
  StableId map_id, height_sha256, rivers_sha256, land_sha256;
  FieldScale km_per_unit, m_per_px;
  Integer width_px = 0, height_px = 0;
};
// 三份像素字节由内容目录唯一拥有，所有读数均由元数据指定格式解码。
struct FieldTerrainData {
  FieldTerrainMetadata metadata;
  CheckpointVector<std::uint8_t> height, rivers, land;
};
struct FieldTerrainSample {
  Integer height_dm = 0, river_distance_mm = 0, river_half_width_mm = 0;
  std::uint8_t river_level = 0, cultivation = 0, forest = 0, rock = 0;
  bool water = false;
};
// 缺文件、格式、尺度或 SHA 不符均显式拒绝，不创建平地后备。
Outcome<FieldTerrainMetadata>
decode_field_terrain_metadata(const BoundaryValue &);
Outcome<FieldTerrainData> load_field_terrain_data(
    FieldTerrainMetadata, std::span<const std::uint8_t> height,
    std::span<const std::uint8_t> rivers, std::span<const std::uint8_t> land);
// 任意图外点按合同夹在图内；高度双线性权重为 1/1024，其余最近像素。
FieldTerrainSample sample_field_terrain(const FieldTerrainData &,
                                        PointI map_point_dm);
// 作者 map_xy 转地图分米；同一有理尺度只在最终位置向下取整。
PointI field_map_point_dm(const FieldTerrainMetadata &, PointI map_xy);
} // namespace hanmo::resident
