#pragma once
#include "resident/domains/estate/estate_detachment_release.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <optional>
#include <vector>
namespace hanmo::resident {
struct MilitaryReturnDetachmentRelease1 {
 StableId estate_id,actor_id,source_id;
 std::optional<StableId> detached_before,detached_after;
};
struct MilitaryReturnDetachmentReleasesBuildView {
 const CheckpointVector<OriginalEstateDetachmentReleaseView>& releases;
};


struct MilitaryReturnDetachmentReleases1 {
 CheckpointMemoryCharge copied_fields;
 std::vector<MilitaryReturnDetachmentRelease1> releases;
 MilitaryReturnDetachmentReleases1()=default;
 explicit MilitaryReturnDetachmentReleases1(const MilitaryReturnDetachmentReleasesBuildView&);
 MilitaryReturnDetachmentReleases1(const MilitaryReturnDetachmentReleases1&);
 MilitaryReturnDetachmentReleases1(MilitaryReturnDetachmentReleases1&&);
 MilitaryReturnDetachmentReleases1& operator=(MilitaryReturnDetachmentReleases1);
 void swap(MilitaryReturnDetachmentReleases1&)noexcept;
};

// Synchronous observations own their fields before append returns. No view
// or Estate slot pointer survives a callback. This is transient capture only.
class MilitaryReturnDetachmentCapture final:public OriginalEstateDetachmentReleaseSink {
 struct PreparedRelease {
  CheckpointMemoryCharge copied_fields;
  MilitaryReturnDetachmentRelease1 row;
  explicit PreparedRelease(const OriginalEstateDetachmentReleaseView&);
  PreparedRelease(const PreparedRelease&)=delete;
  PreparedRelease& operator=(const PreparedRelease&)=delete;
  PreparedRelease(PreparedRelease&&)noexcept=default;
  PreparedRelease& operator=(PreparedRelease&&)=delete;
 };
 CheckpointVector<PreparedRelease> releases_;
public:
 MilitaryReturnDetachmentCapture()=default;
 MilitaryReturnDetachmentCapture(const MilitaryReturnDetachmentCapture&)=delete;
 MilitaryReturnDetachmentCapture& operator=(const MilitaryReturnDetachmentCapture&)=delete;
 Status append(const OriginalEstateDetachmentReleaseView&)override;
 MilitaryReturnDetachmentReleases1 finish()const;
};
}
