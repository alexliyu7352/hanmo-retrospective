// 旁支认可可引用的永久原来源投影；它本身不构成认可、知情或控制许可。
#pragma once
#include "biological_control_origin.hpp"
#include "collateral_family_origin_values.hpp"
#include "collateral_family_source.hpp"
namespace hanmo::resident {
// 当前读取必须沿同候选原目录/完整出生谱系，不接受调用者自己传入的祖先或关系标签。
Outcome<CollateralFamilyOrigin>
make_collateral_family_origin(const BiologicalLegacyBasisContext &,
                              const StableId &first, const StableId &second);
// 消费方先核自己的具名实际事实及payload.tick；这里重读相同事实前的两条权威路径。
// 不借此通用投影授予任意事实新的业务意义；仍须双方真实认可来源和各自资格。
Status validate_collateral_family_origin_before_fact(
    const BiologicalLegacyBasisContext &, const CollateralFamilyOrigin &,
    Integer tick, const StableId &actual_fact);
bool collateral_family_origin_equal(const CollateralFamilyOrigin &,
                                    const CollateralFamilyOrigin &);
Outcome<CollateralFamilyOrigin>
decode_collateral_family_origin(const BoundaryValue &);
BoundaryValue encode_collateral_family_origin(const CollateralFamilyOrigin &);
Status visit_collateral_family_origin_checkpoint(CodecVisitor &,
                                                 CollateralFamilyOrigin &);
using CollateralFamilyRead =
    std::function<Outcome<std::optional<CollateralFamilySource>>(
        const StableId &, const StableId &)>;
using CollateralFamilyBeforeRead =
    std::function<Outcome<std::optional<CollateralFamilySource>>(
        const StableId &, const StableId &, Integer, const StableId &)>;
} // namespace hanmo::resident
