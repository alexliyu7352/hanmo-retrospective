// F§5派生查询索引：完整原源核验后发行热票；票据不持久，clone只复制纯数据并丢票。
#pragma once
#include "facility_claims.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/local_economy/economic_facility_sources.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
// 三个整集合key是遗漏竞争者/原变更/业务反向引用的读依赖，不能降成自己的claim行。
struct FacilityIndexDependencySet {
    DomainReadKey slot_neighborhood,capacity_source_collection,holder_source_collection;
    std::vector<ResourceKey> resources;
    std::vector<DomainReadKey> original_dependencies;
    std::vector<StableId> actual_common_fact_ids;
};
class FacilityIndexDependencyRead {
public:
    virtual ~FacilityIndexDependencyRead()=default;
    virtual Outcome<FacilityIndexDependencySet> read_facility_index_dependencies(const FacilitySlotRef&)const=0;
    // 必须独立核selected provider/holder/physical/PIN完整读闭包，未知或漏登记明确拒绝。
    virtual Status validate_facility_index_dependency_coverage(const FacilitySlotRef&,const FacilityIndexDependencySet&)const=0;
    // 实际物理binding原DTO；Aggregate/market无单位由原provider明确返回空，不伪binding。
    virtual Outcome<std::vector<FacilityPhysicalUnitRef>> read_facility_index_units(const FacilityClaim2&)const=0;
};
class ValidatedFacilityIndex {
public:
    ValidatedFacilityIndex();
    // 即使在同identity内复制，旧candidate凭据也不迁移；需重新认证。
    ValidatedFacilityIndex(const ValidatedFacilityIndex&);
    ValidatedFacilityIndex& operator=(const ValidatedFacilityIndex&);
    ValidatedFacilityIndex(ValidatedFacilityIndex&&)noexcept=default;
    ValidatedFacilityIndex& operator=(ValidatedFacilityIndex&&)noexcept=default;
    ~ValidatedFacilityIndex();
    // 现阶段显式完整fallback。未实现全部受控dirty更新前，不宣称局部热验证成本闭合。
    Status rebuild(const WorldBundle&,const FacilityClaimsState&,const FacilityClaimsCatalog&,const WorldMoment&,
        const FacilityClaimSourceRead&,const FacilityIndexDependencyRead&,const CurrentReadStampRead&,
        const ReadStampAuthority&,const WorldIdentity&,const CommonRead&,const DomainReadVersionSource&,ExactArithmeticBudget={});
    void mark_dirty(const FacilitySlotRef&);
    void invalidate()noexcept;
    Status validate_ticket(const ReadStampAuthority&,const WorldIdentity&,const CommonRead&,const DomainReadVersionSource&)const;
    // 内部只读结果为ID深值；不把别人的claim透过公众DTO泄露，也不返回跨stamp借用。
    Outcome<std::vector<StableId>> overlapping_claims(const FacilitySlotRef&,const MomentSpan&,
        const ReadStampAuthority&,const WorldIdentity&,const CommonRead&,const DomainReadVersionSource&,ExactArithmeticBudget={})const;
    Outcome<std::vector<StableId>> overlapping_physical_claims(const FacilitySlotRef&,const FacilityPhysicalUnitRef&,const MomentSpan&,
        const ReadStampAuthority&,const WorldIdentity&,const CommonRead&,const DomainReadVersionSource&,ExactArithmeticBudget={})const;
private:
    struct Data;
    std::shared_ptr<const Data> data_;
    std::shared_ptr<const ReadStamp> ticket_;
    std::vector<FacilitySlotRef> dirty_slots_;
};
}
