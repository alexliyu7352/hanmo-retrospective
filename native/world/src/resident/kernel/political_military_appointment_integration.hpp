// 服务对象仅持完整World引用，每次调用重建短命上下文，不跨回滚借用optional内部。
#pragma once
#include "resident/domains/politics/officers/political_military_appointments_profile.hpp"
#include "resident/domains/politics/officers/political_military_appointments_director.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PoliticalMilitaryAppointmentWorldAccess final:public MilitaryOfficerAppointmentRead {
public:
 explicit PoliticalMilitaryAppointmentWorldAccess(const WorldBundle& world):world_(world){}
 Outcome<MilitaryOfficerAppointmentProof> appointment_for_application(const StableId&,const StableId&,const StableId&)const override;
 Outcome<MilitaryOfficerAppointmentProof> appointment_before_application(const StableId&,const StableId&,const StableId&,Integer)const override;
private:const WorldBundle& world_;
};
Outcome<std::optional<DecisionProposal>> propose_political_military_appointment_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Status validate_political_military_appointment_world(const WorldBundle&);
Outcome<PoliticalMilitaryAppointmentQuote> prepare_political_military_appointment_world(const WorldBundle&,const StableId&,const PoliticalMilitaryAppointmentCommand&);
Outcome<StableId> commit_political_military_appointment_world(WorldBundle&,const StableId&,const PoliticalMilitaryAppointmentCommand&,const PoliticalMilitaryAppointmentQuote&);
Outcome<BoundaryValue> merge_political_military_appointment_world_page(const WorldBundle&,const StableId&,const StableId&,BoundaryValue);
}
