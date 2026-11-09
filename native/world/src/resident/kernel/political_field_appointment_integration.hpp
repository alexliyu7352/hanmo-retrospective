// 服务对象仅持完整World引用，每次调用重建短命上下文，不跨回滚借用optional内部。
#pragma once
#include "resident/domains/politics/field/political_field_appointments_profile.hpp"
#include "resident/domains/politics/field/political_field_appointments_director.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PoliticalFieldAppointmentWorldAccess final:public MilitaryFieldCommandAppointmentRead {
public:
 explicit PoliticalFieldAppointmentWorldAccess(const WorldBundle& world):world_(world){}
 Outcome<MilitaryFieldCommandAppointmentProof> appointment_for_application(const StableId&,const StableId&,const StableId&)const override;
 Outcome<MilitaryFieldCommandAppointmentProof> appointment_before_application(const StableId&,const StableId&,const StableId&,Integer)const override;
private:const WorldBundle& world_;
};
Status validate_political_field_appointment_world(const WorldBundle&);
Outcome<std::optional<DecisionProposal>> propose_political_field_appointment_world(const WorldBundle&,const KnownSubjectView&,const DirectorPolicy&,const DecisionProposal&);
Outcome<PoliticalFieldAppointmentQuote> prepare_political_field_appointment_world(const WorldBundle&,const StableId&,const PoliticalFieldAppointmentCommand&);
Outcome<StableId> commit_political_field_appointment_world(WorldBundle&,const StableId&,const PoliticalFieldAppointmentCommand&,const PoliticalFieldAppointmentQuote&);
Outcome<BoundaryValue> query_political_field_appointment_world(const WorldBundle&,Integer);
}
