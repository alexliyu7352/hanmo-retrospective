// 共享候选：补任办理短栈自持原辖区、军事和人物控制只读，不把临时引用留给后续世界。
#pragma once
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_profile.hpp"
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_control.hpp"
#include "resident/domains/military/military_model.hpp"
#include <memory>
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_qualification.hpp"
#include "resident/domains/politics/subordinate_appointments/political_subordinate_appointment_legacy.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PoliticalSubordinateAppointmentWorldAccess final {
public:
 static Outcome<std::unique_ptr<PoliticalSubordinateAppointmentWorldAccess>> create(const WorldBundle&);
 PoliticalSubordinateAppointmentWorldAccess(const PoliticalSubordinateAppointmentWorldAccess&)=delete;
 PoliticalSubordinateAppointmentWorldAccess& operator=(const PoliticalSubordinateAppointmentWorldAccess&)=delete;
 const PoliticalSubordinateAppointmentContext& context()const{return rules_;}
private:
 explicit PoliticalSubordinateAppointmentWorldAccess(const WorldBundle&);
 const WorldBundle& world_;std::optional<MilitaryAccess> military_;std::optional<PoliticalSuccessionOccupationContext> occupations_;
 PoliticalSubordinateTerritoryContext territory_;PoliticalSubordinateQualificationAccess qualification_;PoliticalSubordinateAppointmentContext rules_;
};
Status preview_political_subordinate_legacy_world(const WorldBundle&,const PoliticsCommand&);
Status validate_political_subordinate_appointment_world(const WorldBundle&);
Outcome<PoliticalSubordinateAppointmentQuote> prepare_political_subordinate_appointment_world(const WorldBundle&,const StableId&,const PoliticalSubordinateAppointmentCommand&);
Outcome<PoliticsEffects> commit_political_subordinate_appointment_world(WorldBundle&,const StableId&,const PoliticalSubordinateAppointmentCommand&,const PoliticalSubordinateAppointmentQuote&);
Outcome<BoundaryValue> query_political_subordinate_appointment_world(const WorldBundle&,const StableId& place,Integer cursor=0);
Status bind_political_subordinate_appointment_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
