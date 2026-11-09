// 已验证原许可读取的真实依赖；类型不等于来源schema，注册由原owner负责。
#pragma once
#include "political_shop_licensing.hpp"
#include "resident/domains/actor_foundation/character_availability_provenance.hpp"
namespace hanmo::resident {
struct PoliticalTakeoverRule;
struct PoliticalTakeoverRegistration;
struct MilitaryOccupationFact;
class PoliticalShopLicensingOriginalSink {
public:
  virtual ~PoliticalShopLicensingOriginalSink() = default;
  virtual Status boundary(const PoliticalShopLicensingContext &,
                          const FactRecord &, const WorldMoment &) = 0;
  // 同步借用实际根和行；不可保存context地址/改候选。任一失败须丢弃整组输出。
  virtual Status court(const PoliticalShopLicensingContext &,
                       const PoliticalShopCourt &) = 0;
  virtual Status office(const PoliticalShopLicensingContext &,
                        const OfficeState &initial,
                        const OfficeState &current) = 0;
  virtual Status organization_fact(const PoliticalShopLicensingContext &,
                                   const FactRecord &,
                                   const OrganizationFact &) = 0;
  virtual Status member(const PoliticalShopLicensingContext &,
                        const OrgMemberState *initial,
                        const OrgMemberState &current,
                        const MemberPolicy &) = 0;
  virtual Status membership(const PoliticalShopLicensingContext &,
                            const MembershipOffer &, const MembershipDecision &,
                            const MembershipTerms &) = 0;
  virtual Status initial_grant(const PoliticalShopLicensingContext &,
                               const Grant &) = 0;
  virtual Status grant_policy(const PoliticalShopLicensingContext &,
                              const GrantPolicy &) = 0;
  virtual Status current_grant(const PoliticalShopLicensingContext &,
                               const GrantRuntimeRecord &) = 0;
  virtual Status
  control(const PoliticalShopLicensingContext &, const PoliticalShopCourt &,
          const PoliticalShopControlContext &, const PoliticalTakeoverRule &,
          const PoliticalTakeoverRegistration &,
          const FactRecord &registration_head, const MilitaryOccupationFact &,
          const FactRecord &occupation_head) = 0;
  // 实际每个人/每个合法来源截面，返回同次原11槽mapper；nullptr不是成功。
  virtual Outcome<CharacterAvailabilityProvenanceSink *>
  availability(const PoliticalShopLicensingContext &, const StableId &actor,
               const FactRecord &actual_cut, const WorldMoment &) = 0;
};
Status visit_political_shop_permission_originals2(
    const PoliticalShopLicensingContext &, const PrivateShopPermissionRequest &,
    const StableId &actual_opening, Integer opened_tick,
    PoliticalShopLicensingOriginalSink &);
namespace shop_law_provenance {
Status authority(const PoliticalShopLicensingContext &, const ActingAuthority &,
                 const StableId &actual_boundary);
Status boundary(const PoliticalShopLicensingContext &,
                const StableId &actual_boundary);
} // namespace shop_law_provenance
} // namespace hanmo::resident
