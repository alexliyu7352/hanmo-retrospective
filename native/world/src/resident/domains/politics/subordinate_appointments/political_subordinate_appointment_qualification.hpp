// 资格只消费原人物四种身份初态；本域不另建行政/功勋账。
#pragma once
#include "political_subordinate_appointment_state.hpp"
#include "resident/domains/growth/growth_attribute_origin.hpp"
namespace hanmo::resident {
class PoliticalSubordinateQualificationAccess final:public PoliticalSubordinateQualificationRead {
 const CommonStore& common_;const PoliticalSubordinateAppointmentState& state_;GrowthSkillOriginContext origins_;
public:
 PoliticalSubordinateQualificationAccess(const CommonStore& c,const PoliticalSubordinateAppointmentState& s,GrowthSkillOriginContext origins):common_(c),state_(s),origins_(origins){}
 PoliticalSubordinateQualificationAccess(const PoliticalSubordinateQualificationAccess&)=delete;
 Outcome<PoliticalSubordinateQualificationSource> subordinate_office_qualification(const StableId&)const override;
 Outcome<PoliticalSubordinateQualificationSource> subordinate_office_qualification_before(const StableId&,Integer,const StableId&)const override;
};
}
