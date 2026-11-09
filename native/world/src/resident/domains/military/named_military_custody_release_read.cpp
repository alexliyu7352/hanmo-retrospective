#include "resident/domains/military/military_model.hpp"
// 整根重读每次关闭的原因果截面；今日控制者、今日位置或一个关闭布尔值都不是证明。
#include "named_military_custody_release_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/field_battle_exit_placement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <set>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
Integer sequence(const StableId &id) {
  constexpr std::string_view prefix="named_military_release:";
  if(!id.starts_with(prefix))return -1;
  const auto suffix=std::string_view(id).substr(prefix.size());Integer n=0;
  const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),n);
  return parsed.ec==std::errc{}&&parsed.ptr==suffix.data()+suffix.size()&&n>0&&std::to_string(n)==suffix?n:-1;
}
bool same_sources(const std::vector<MomentOwnerSource> &a,const std::vector<MomentOwnerSource> &b) {
  if(a.size()!=b.size())return false;
  for(std::size_t i=0;i<a.size();++i)if(a[i].source_schema!=b[i].source_schema||!same_source(a[i].source_ref,b[i].source_ref))return false;
  return true;
}
void validate_release(const NamedMilitaryCustodyReleaseContext &x,const NamedMilitaryCustodyRelease &release) {
  const auto &c=x.custody.common;
  if(!release.field_order.empty()) {
    auto fields=release.field_order;
    FieldOrder expected{"id","term_id","actor_id","keeper_army_id","keeper_organization_id",
      "prior_custody_source","released_at","recorded_tick","origin_anchor","origin_sources",
      "destination_priority","destination_settlement_id","jurisdiction_id","road_distance",
      "road_ids","initial_returning","released_anchor"};
    std::sort(fields.begin(),fields.end());std::sort(expected.begin(),expected.end());
    if(fields!=expected)fail("NamedMilitaryCustodyReleaseShapeMismatch",release.id);
  }
  const SourceRef source{"military","military_named_custody_released",release.id};
  if(take(x.placements.read_named_custody_placement(source,release.actor_id))!=&release)
    fail("NamedMilitaryCustodyActualReleaseRequired",release.id);
  const auto prior=take(read_named_military_custody_before_release(x.custody,release));
  if(!prior.term||release.origin_sources.empty())fail("NamedMilitaryCustodyReleaseTermRequired",release.id);
  const auto origin=take(read_actor_spatial_location_before_source(c,x.common_catalog,x.movement,
      x.carriers,x.registrations,x.foundation,x.actors,x.spatial,release.actor_id,
      release.origin_sources.front().source_ref,source,release.released_at,x.military_handoffs,
      x.budget,x.family_catalog,x.births,x.military_actions,&x.placements,x.field_exits));
  if(!take(same_spatial_anchor(origin.actual_anchor,release.origin_anchor,x.budget))||
      !same_sources(origin.original_sources,release.origin_sources))
    fail("NamedMilitaryCustodyReleaseOriginMismatch",release.id);
  const auto destination=take(quote_named_military_custody_destination_before_release(
      x.custody,x.travel,x.territory,release,x.budget));
  if(destination.priority!=release.destination_priority||destination.settlement_id!=release.destination_settlement_id||
      destination.jurisdiction_id!=release.jurisdiction_id||
      compare(destination.road_distance,release.road_distance,x.budget)!=0||
      destination.road_ids!=release.road_ids||destination.initial_returning!=release.initial_returning)
    fail("NamedMilitaryCustodyReleaseDestinationMismatch",release.id);
  if(destination.destination) {
    const MovementAnchor expected=OriginalMovementAnchor{*destination.destination,source};
    if(!take(same_spatial_anchor(expected,release.released_anchor,x.budget)))
      fail("NamedMilitaryCustodyReleasePlacementMismatch",release.id);
  }else if(!take(same_actual_movement_position(release.origin_anchor,release.released_anchor,x.budget))) {
    fail("NamedMilitaryCustodyReleasePlacementMismatch",release.id);
  }
}
}
Status validate_named_military_custody_release_sources(const NamedMilitaryCustodyReleaseContext &x) {
  return status([&] {
    const auto &c=x.custody.common;const auto &history=x.custody.military.named_custody;
    if(!x.placements.bound_to(c,x.movement))fail("CrossCandidateNamedMilitaryCustodyPlacementRead");
    if(x.field_exits&&!x.field_exits->bound_to(c,x.movement))
      fail("CrossCandidateNamedMilitaryCustodyFieldExitRead");
    take(validate_named_military_custody_terms(x.custody));
    std::set<StableId> ids,terms;Integer previous_sequence=0;std::size_t previous_order=0;
    // 首次俘获应用与关闭共享真实 Common 顺序；新一次俘获不能倒到上一关闭之前。
    std::vector<std::pair<std::size_t, const WorldMoment *>> applications;
    for (const auto &term : history.terms)
      applications.emplace_back(take(read_actual_common_fact_position(c,
          SourceRef{"military", "military_named_custody_term", term.id})), &term.adopted_at);
    for(const auto &release:history.closures) {
      const auto serial=sequence(release.id);
      const auto order=take(read_actual_common_fact_position(c,SourceRef{"military","military_named_custody_released",release.id}));
      if(serial<=previous_sequence||serial>=c.state().next_instance_id||
          (previous_sequence&&order<=previous_order)||!ids.insert(release.id).second||
          !terms.insert(release.term_id).second)
        fail("NamedMilitaryCustodyReleaseRepeated",release.id);
      validate_release(x,release);previous_sequence=serial;previous_order=order;
      applications.emplace_back(order, &release.released_at);
    }
    std::sort(applications.begin(), applications.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });
    for (std::size_t i = 1; i < applications.size(); ++i)
      if (applications[i - 1].first == applications[i].first ||
          take(compare_world_moments(*applications[i - 1].second,
              *applications[i].second, x.budget)) > 0)
        fail("NamedMilitaryCustodyApplicationOrderMismatch");
    for(const auto &[id,head]:c.state().facts.rows())
      if(head.kind=="military_named_custody_released"&&(head.domain!="military"||!ids.contains(id)))
        fail("NamedMilitaryCustodyReleasePayloadRequired",id);
  });
}
Outcome<const NamedMilitaryCustodyRelease *> read_original_named_military_custody_release(
    const NamedMilitaryCustodyReleaseContext &context, const SourceRef &source) {
  return guard<const NamedMilitaryCustodyRelease *>([&] {
    take(validate_named_military_custody_release_sources(context));
    if (source.domain != "military" || source.kind != "military_named_custody_released")
      fail("NamedMilitaryCustodyReleaseSourceRequired", source.id);
    const NamedMilitaryCustodyRelease *actual = nullptr;
    for (const auto &release : context.custody.military.named_custody.closures)
      if (release.id == source.id) {
        if (actual) fail("NamedMilitaryCustodyReleaseRepeated", source.id);
        actual = &release;
      }
    if (!actual) fail("NamedMilitaryCustodyReleaseSourceRequired", source.id);
    take(read_actual_common_fact_position(context.custody.common, source));
    return actual;
  });
}
bool OriginalNamedMilitaryCustodyReleaseRead::bound_to(const CommonStore &common,const MilitaryState &military) const noexcept {
  return &common==&context_.custody.common&&&military==&context_.custody.military;
}
Outcome<const NamedMilitaryCustodyRelease *> OriginalNamedMilitaryCustodyReleaseRead::read_current_named_custody_release(const StableId &actor) const {
  return guard<const NamedMilitaryCustodyRelease *>([&] {
    take(validate_named_military_custody_release_sources(context_));
    if(!context_.custody.common.actor(actor))fail("UnknownActor",actor);
    const NamedMilitaryCustodyRelease *latest=nullptr;std::size_t latest_order=0;
    const auto position=[&](const SourceRef &source){return take(read_actual_common_fact_position(context_.custody.common,source));};
    for(const auto &release:context_.custody.military.named_custody.closures)if(release.actor_id==actor) {
      const auto order=position({"military","military_named_custody_released",release.id});
      if(!latest||order>latest_order){latest=&release;latest_order=order;}
    }
    if(!latest)return latest;
    for(const auto &receipt:context_.custody.military.casualty_facts)
      for(const auto &change:receipt.actor_changes)if(change.actor_id==actor&&change.status=="captured"&&
          position({"military","military_casualties",receipt.id})>latest_order)return static_cast<const NamedMilitaryCustodyRelease *>(nullptr);
    return latest;
  });
}
} // namespace hanmo::resident
