// Same-candidate original domain revisions. No revision is synthesized for a missing source.
#pragma once
#include "read_stamp.hpp"
namespace hanmo::resident {
struct WorldBundle;class MilitaryFormationSourceRead;class MilitaryReturnSourceRead;class NativeMilitaryTrainingPeriodRead;
class NativeDomainReadVersionSource final:public DomainReadVersionSource {
public:
    explicit NativeDomainReadVersionSource(const WorldBundle& world,
        const MilitaryFormationSourceRead* formations=nullptr,
        const MilitaryReturnSourceRead* returns=nullptr,
        const NativeMilitaryTrainingPeriodRead* periods=nullptr)
        :world_(world),formations_(formations),returns_(returns),periods_(periods){}
    Outcome<std::uint64_t> current_version(const StableId&,const StableId&,const StableId&)const override;
private:
    const WorldBundle& world_;
    const MilitaryFormationSourceRead* formations_;
    const MilitaryReturnSourceRead* returns_;
    const NativeMilitaryTrainingPeriodRead* periods_;
};
// The enclosing original owner keeps all three source readers alive. This reader owns the
// finite version resolver, but never recursively opens a complete World lease.
std::unique_ptr<CurrentReadStampRead> make_native_current_read_stamp_reader(
    const WorldBundle&,ReadStampLimits,const MilitaryFormationSourceRead* formations=nullptr,
    const MilitaryReturnSourceRead* returns=nullptr,const NativeMilitaryTrainingPeriodRead* periods=nullptr);
}
