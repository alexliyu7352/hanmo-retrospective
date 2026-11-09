// FH §5.5 的局部承运对象联合；实际船与运输编组各归唯一原 owner。
// 这里只保存接受对象的身份，不代表承运同意、真实装载或新增公共载体分支。
#pragma once
#include "resident/domains/military/naval_vessel_ref.hpp"
#include "resident/domains/travel/spatial_carrier_model.hpp"

namespace hanmo::resident {
// 原 S1 编组及接受当刻真实 composition revision；不是冻结当前编组副本。
// 后继合法变化和本次 scope 是否仍可承运，必须由真实 S1 reader 独立证明。
struct CareGroundTransportCarrier {
  TransportUnitRef transport_unit_ref;
  Integer expected_composition_revision = 0;
};

// 只导入原 Naval 单船键，不使用 Fleet 或 MilitaryLocalVesselRef 冒实船。
// 此值不包含船体、世代、能力、载荷、产权或患者同意的第二份状态。
struct CareActualVesselCarrier {
  VesselRef vessel_ref;
};

// 原局部 wire 仅 tag0 GroundTransportUnit / tag1 ActualVessel。
// 公共 SpatialCarrierRef 保原五支，不在这里扩展或转换为通用载体。
using CareTransportCarrier =
    std::variant<CareGroundTransportCarrier, CareActualVesselCarrier>;
} // namespace hanmo::resident
