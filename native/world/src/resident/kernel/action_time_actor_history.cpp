#include "action_time_validation.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
Status ActionTimeOriginalSourceRead::visit_actor_action_history(
    const ActionTimeRegistration &registration, const StableId &,
    ActionTimeActorHistorySink &) const {
  return common_failure("OriginalActorActionHistoryReaderRequired",
                        registration.action_kind,
                        ErrorKind::unsupported_capability);
}
} // namespace hanmo::resident
