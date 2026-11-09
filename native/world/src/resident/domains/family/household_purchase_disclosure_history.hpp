// 原采购告知者的真实前缀：不新增受托权、身体账或 cohort 通用知识。
#pragma once
#include "household_control.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct InteractionState;
struct OrganizationState;
struct OrganizationCatalog;
struct PoliticalOfficeReceptionState;
struct OrganizationFact;
struct OrganizationServiceEffect;
struct CharacterOriginRecordV2;
struct CapacityTransitionV2;
struct FamilyDeathRecord;
class CurrentMomentRead;
class CharacterAvailabilityProvenanceSink;
struct HouseholdPurchaseDisclosureHistorySources {
  const CharacterConditionSources &condition;
  const FamilyState &family;
  const FamilyCatalog &family_catalog;
  const CommonCatalog &shared;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const HouseholdProvisionControlRead &controls;
  const InteractionState &interaction;
  const CurrentMomentRead &clock;
  const PoliticalOfficeReceptionState *office_receptions = nullptr;
};
struct HouseholdPurchaseDisclosureControllerRead {
  const RoleDefinition *authored_role = nullptr;
  bool role_enabled = false, present = false, alive = false, available = false;
  const CharacterOriginRecordV2 *original_origin = nullptr;
  const CapacityTransitionV2 *latest_availability_transition = nullptr;
  const FamilyDeathRecord *actual_death = nullptr;
  const OrganizationFact *enabling_parent = nullptr;
  const OrganizationServiceEffect *enabling_effect = nullptr;
};
// 唯一 consumer 是 FaceToFaceContactSources/3 的真实
// interaction/household_purchase_report_disclosed/1。只调 S3 原 raw 身份口，
// 不回 S3 full/知识/采购执行。所有根和 controls 必须由同 World owner 借出。
// 原 Organization/Family 父事实的完整授权仍由其原 owner 验；本口重放其
// 私务 role 后果，并沿严格 Origin2/Life4/availability 原 reader 核当时身体。
// 返回只借原对象/布尔，无新拥有 head 或身体副本。根变动前须放弃借用。
// retained 若给出则保全整个真实前缀；retained_presence 沿原 typed 来源口
// 连已检查但未应用的 suffix 也保留，不由本口猜 SourceRef。两个 sink
// 均不可改根；失败不得采用部分 sink 输出。
Outcome<HouseholdPurchaseDisclosureControllerRead>
read_household_purchase_disclosure_controller_before_fact(
    const HouseholdPurchaseDisclosureHistorySources &, const SourceRef &,
    Integer source_schema = 1, CommonActualFactPrefixSink *retained = nullptr,
    CharacterAvailabilityProvenanceSink *retained_presence = nullptr);
} // namespace hanmo::resident
