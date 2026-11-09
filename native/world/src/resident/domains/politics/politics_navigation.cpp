// 受任后真实行旅由原旅行命令办理；失效权源、拒任与不足资格都不能生成到任路线。
#include "politics_navigation.hpp"
#include "politics_organization_runtime.hpp"
#include "politics_views.hpp"
namespace hanmo::resident {
Outcome<TravelNextStep> plan_political_arrival(
    const CommonStore &c, const PoliticsState &state,
    const PoliticsCatalog &catalog, const OrganizationState &org,
    const OrganizationCatalog &oc, const LocalEconomyState &economy,
    const SocialState &social, const ActivityRead &activity,
    const PoliticalNavigationRead &nav, const StableId &actor,
    const StableId &appointment, Integer revision, const StableId &source,
    const PoliticalOrganizationRuntimeReadContext *runtime) {
  auto known = read_political_fact_known(c, state, social, actor, source,
                                         runtime ? &runtime->clock : nullptr);
  if (!known.ok)
    return {false, {}, known.error};
  if (!known.value)
    return {false,
            {},
            common_failure("PoliticalAppointmentNotKnown", source).error};
  const auto preview = preview_politics_command(
      c, state, catalog, org, oc, economy, activity, actor,
      PoliticalArriveCommand{actor, appointment, revision, source}, runtime);
  if (!preview.ok &&
      preview.error.original.code != "PoliticalCandidateNotAtSeat")
    return {false, {}, preview.error};
  const auto *accepted = state.appointments.find(appointment);
  const auto *j = state.jurisdictions.find(accepted->jurisdiction_id);
  if (preview.ok) {
    TravelNextStep done;
    done.target_place_id = j->seat_place_id;
    return {true, std::move(done), {}};
  }
  auto step = next_travel_command(c, nav.state, nav.catalog, nav.services,
                                  actor, j->seat_place_id);
  if (!step.ok)
    return step;
  if (!step.value.command)
    return {false,
            {},
            common_failure("PoliticalNavigationLocationMismatch", actor,
                           ErrorKind::internal)
                .error};
  if (std::holds_alternative<TravelCommand>(*step.value.command) &&
      !step.value.travel_quote)
    return {false,
            {},
            common_failure("PoliticalTravelQuoteRequired", actor,
                           ErrorKind::internal)
                .error};
  return step;
}
} // namespace hanmo::resident
