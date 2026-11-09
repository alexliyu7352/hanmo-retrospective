#include "military_return_detachment_model.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <type_traits>
#include <utility>
namespace hanmo::resident {
namespace {
static_assert(std::is_nothrow_move_constructible<MilitaryReturnDetachmentRelease1>::value,"detachment payload must transfer without a second copy");
CheckpointMemoryCharge reserve(const OriginalEstateDetachmentReleaseView& v){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryReturnDetachmentReleasesBuildView& v){return CheckpointMemoryCharge(0);}
CheckpointMemoryCharge reserve(const MilitaryReturnDetachmentReleases1& v){return CheckpointMemoryCharge(0);}
MilitaryReturnDetachmentRelease1 copy(const OriginalEstateDetachmentReleaseView& v){
 MilitaryReturnDetachmentRelease1 out{StableId(v.estate_id),StableId(v.actor_id),StableId(v.source_id),{}, {}};
 if(v.detached_before)out.detached_before.emplace(*v.detached_before);
 if(v.detached_after)out.detached_after.emplace(*v.detached_after);
 return out;
}
OriginalEstateDetachmentReleaseView view(const MilitaryReturnDetachmentRelease1& v){return {v.estate_id,v.actor_id,v.source_id,v.detached_before?&*v.detached_before:nullptr,v.detached_after?&*v.detached_after:nullptr};}
}
MilitaryReturnDetachmentReleases1::MilitaryReturnDetachmentReleases1(const MilitaryReturnDetachmentReleasesBuildView& v):copied_fields(reserve(v)){
 releases.reserve(v.releases.size());
 for(const auto& release:v.releases)releases.emplace_back(copy(release));
}
MilitaryReturnDetachmentReleases1::MilitaryReturnDetachmentReleases1(const MilitaryReturnDetachmentReleases1& v):copied_fields(reserve(v)),releases(v.releases){}
MilitaryReturnDetachmentReleases1::MilitaryReturnDetachmentReleases1(MilitaryReturnDetachmentReleases1&& v):MilitaryReturnDetachmentReleases1(static_cast<const MilitaryReturnDetachmentReleases1&>(v)){}
MilitaryReturnDetachmentReleases1& MilitaryReturnDetachmentReleases1::operator=(MilitaryReturnDetachmentReleases1 v){swap(v);return *this;}
void MilitaryReturnDetachmentReleases1::swap(MilitaryReturnDetachmentReleases1& v)noexcept{copied_fields.swap(v.copied_fields);releases.swap(v.releases);}
MilitaryReturnDetachmentCapture::PreparedRelease::PreparedRelease(const OriginalEstateDetachmentReleaseView& v):copied_fields(reserve(v)),row(copy(v)){}
Status MilitaryReturnDetachmentCapture::append(const OriginalEstateDetachmentReleaseView& v){releases_.emplace_back(v);return common_success();}
MilitaryReturnDetachmentReleases1 MilitaryReturnDetachmentCapture::finish()const{
 CheckpointVector<OriginalEstateDetachmentReleaseView> borrowed;
 borrowed.reserve(releases_.size());
 for(const auto& release:releases_)borrowed.push_back(view(release.row));
 return MilitaryReturnDetachmentReleases1(MilitaryReturnDetachmentReleasesBuildView{borrowed});
}
}
