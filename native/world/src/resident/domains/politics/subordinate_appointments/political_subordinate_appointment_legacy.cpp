#include "political_subordinate_appointment_legacy.hpp"
#include "political_subordinate_appointment_detail.hpp"
namespace hanmo::resident {
Status preview_political_subordinate_legacy_command(const PoliticalSubordinateAppointmentContext& x,const PoliticsCommand& command){const auto* proposed=std::get_if<PoliticalAppointCommand>(&command);if(!proposed)return common_success();auto valid=validate_political_subordinate_appointments(x);if(!valid.ok)return valid;for(const auto& row:x.state.offers.rows()){const auto& o=row.second;const auto* p=x.catalog.policies.find(o.policy_id);if(!p)return common_failure("PoliticalSubordinateAppointmentPolicyRequired",o.id);if(p->jurisdiction_id==proposed->jurisdiction_id&&subordinate_appointment_detail::pending(x.state,o,x.common.tick()))return common_failure("PoliticalSubordinateAppointmentPending",o.id);}return common_success();}
}
