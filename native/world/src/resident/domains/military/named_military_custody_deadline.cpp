#include "resident/domains/military/military_model.hpp"
#include "named_military_custody_deadline.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Outcome<std::vector<const NamedMilitaryCustodyTerm *>> read_due_named_military_custody_terms(
    const NamedMilitaryCustodyReleaseContext &x,const WorldMoment &from,const WorldMoment &to) {
  return guard<std::vector<const NamedMilitaryCustodyTerm *>>([&] {
    take(validate_named_military_custody_release_sources(x));
    if(!x.custody.clock)fail("NamedMilitaryCustodyActualClockRequired");
    const auto now=take(x.custody.clock->read_current_moment());
    if(take(compare_world_moments(from,now,x.budget))!=0||
        take(compare_world_moments(from,to,x.budget))>0)
      fail("NamedMilitaryCustodyDeadlineRangeMismatch");
    std::vector<const NamedMilitaryCustodyTerm *> out;
    for(const auto &term:x.custody.military.named_custody.terms) {
      bool closed=false;
      for(const auto &release:x.custody.military.named_custody.closures)if(release.term_id==term.id)closed=true;
      if(closed)continue;
      const auto elapsed=take(compare_world_moments(term.due_at,from,x.budget));
      if(elapsed<0)fail("NamedMilitaryCustodyUnconsumedDeadline",term.id);
      // 相等仍是本次实际 due；真正关闭后上面的原记录自然排除重复，不靠技术去重表。
      if(take(compare_world_moments(term.due_at,to,x.budget))<=0)out.push_back(&term);
    }
    return out;
  });
}
} // namespace hanmo::resident
