// Care 等外来 Location 字段显式借同一冷读预算；旧 Common 入口不变。
#pragma once
#include "common_codec.hpp"
namespace hanmo::resident::boundary {
Location decode_location(const BoundaryValue &, std::string_view,
                         ScopedCheckpointJsonDecode &,
                         bool carrier_location_enabled = false);
}
