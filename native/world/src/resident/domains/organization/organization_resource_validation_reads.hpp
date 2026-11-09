// 原稳定态资源历史/条件性目录范围的唯一闭包；不授当前资源使用或支出权限。
#pragma once
#include "organization_validation.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct MilitaryAccess;
struct NativeOrganizationResourceValidationReads {
  OrganizationResourceHistoryRead resource_history;
  OrganizationProspectiveGrantRead prospective_grants;
};
// 返回的闭包仅借这两个实参。原 validate 的局部 ma 在同步校验后销毁；
// complete scope 必须持有其真实 MilitaryAccess 和 World，直到所有读取结束。
// 不在这里重新 acquire 世界工厂，也不复制军队/资产表或生成第二套范围名单。
NativeOrganizationResourceValidationReads
make_native_organization_resource_validation_reads(const WorldBundle &,
                                                   const MilitaryAccess &);
NativeOrganizationResourceValidationReads
make_native_organization_resource_validation_reads(const WorldBundle &,
                                                   MilitaryAccess &&) = delete;
NativeOrganizationResourceValidationReads
make_native_organization_resource_validation_reads(
    WorldBundle &&, const MilitaryAccess &) = delete;
} // namespace hanmo::resident
