// 复用原political_career枚举预算，仅显式新intent目录允许这个具名计划，不改变旧0..18枚举范围。
#pragma once
#include "political_subordinate_appointment_decisions.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
inline constexpr const char* political_subordinate_appointment_template="political_subordinate_appointment";
class PoliticalSubordinateAppointmentDirectorRead {
public:
 virtual ~PoliticalSubordinateAppointmentDirectorRead()=default;
 virtual Outcome<PoliticalSubordinateAppointmentChoice> choose(const StableId& actor,const StableId& selected_policy,Integer budget)const=0;
 // 只有World内部桥可把typed authored命令封装进原CommandPayload；public decoder不可调用。
 virtual Outcome<std::shared_ptr<const CommandPayload>> command(const AuthoredSubordinateAppointmentCommand&)const=0;
};
Outcome<std::optional<DecisionProposal>> propose_director_subordinate_appointment(const CommonStore&,const KnownSubjectView&,const DirectorPolicy&,const StableId& selected_policy,const PoliticalSubordinateAppointmentDirectorRead&,const DecisionProposal&,Integer candidate_budget);
// 调用者显式传已选新目录；不借新Director成员，旧目录/原visitor没有隐式准入。
Status validate_political_subordinate_appointment_director(const DirectorCatalog&,const DirectorState&,const PoliticalSubordinateAppointmentContext*);
}
