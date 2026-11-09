// 同候选的原日历、控制和 Life4 来源拥有面；不调用全世界工厂递归造身体源。
#pragma once
#include "native_life_age_catalog.hpp"
#include "world_read_stamp.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
struct WorldBundle;
class NativeWorldLifeAgeRead final:public LifeAgeExactReadLease,public NaturalAgeDeathSourceRead,public CheckpointAllocatedObject {
    const WorldBundle& world_;
    WorldCurrentMomentRead clock_;
    OriginalCalendarRead calendar_;
    ProfileControlEligibilitySources control_;
    NativeLifeAgeExactRead original_;
    NativeWorldLifeAgeRead(const WorldBundle&,OriginalCalendarRead,const EnvironmentalSeaDeathSourceRead*,const BornCharacterSourceRead*,
                          const MilitaryFormationSourceRead*,const MilitaryReturnSourceRead*);
public:
    // All professional sources borrow the same outer lease and must outlive this owner.
    static Outcome<std::unique_ptr<NativeWorldLifeAgeRead>> create(
        const WorldBundle&,const EnvironmentalSeaDeathSourceRead*,const BornCharacterSourceRead* born=nullptr,
        const MilitaryFormationSourceRead* formations=nullptr,const MilitaryReturnSourceRead* returns=nullptr);
    NativeWorldLifeAgeRead(const NativeWorldLifeAgeRead&)=delete;
    NativeWorldLifeAgeRead& operator=(const NativeWorldLifeAgeRead&)=delete;
    LifeAgeExactReadBinding bindings()const override;
    Outcome<LifeCauseView4> read_age_death_source(const NaturalAgeDeathCause4&)const override;
};
Status validate_native_life_age_layout(const WorldBundle&);
Status validate_native_life_age_world(const WorldBundle&);
LifeAgeExactWorldFactory native_life_age_world_factory();
}
