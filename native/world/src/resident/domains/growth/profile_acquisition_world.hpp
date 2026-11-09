// canonical取得只从一个真正WorldBundle借读；宿主不能拼另一世界的Evidence或角色目录。
#pragma once
#include "card_acquisition_contract.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Only the fresh-world candidate invokes this after all original creation/card
// owners are installed. Restore never manufactures missing acquisitions.
Status initialize_profile_acquisitions_from_original_seed_in_candidate(WorldBundle&);
Status validate_profile_acquisitions_world(const WorldBundle&);
Status record_growth_training_card_acquisitions_in_candidate(WorldBundle&,const StableId& actual_training_id);
class ProfileAcquisitionWorldRead final {
public:
    static Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>> create(const WorldBundle&);
    ~ProfileAcquisitionWorldRead();
    ProfileAcquisitionWorldRead(const ProfileAcquisitionWorldRead&)=delete;
    ProfileAcquisitionWorldRead& operator=(const ProfileAcquisitionWorldRead&)=delete;
    const CardAcquisitionSources& sources()const;
    const CardAcquisitionState& acquisitions()const;
    const ProfileControlEligibilitySources& control_sources()const;
private:
    friend Status initialize_profile_acquisitions_from_original_seed_in_candidate(WorldBundle&);
    static Outcome<std::unique_ptr<ProfileAcquisitionWorldRead>> bind_original_sources(const WorldBundle&);
    struct Impl;explicit ProfileAcquisitionWorldRead(CheckpointMemoryCharge,std::unique_ptr<Impl>);
    CheckpointMemoryCharge object_memory_;
    std::unique_ptr<Impl> impl_;
};
}
