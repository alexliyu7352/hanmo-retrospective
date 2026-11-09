// 公共入口先完整历史与actualresult，再调用原同候选Common Role writer；不新增异域权限。
#include "political_office_reception_business.hpp"
namespace hanmo::resident {
Status validate_political_office_receptions(const PoliticalOfficeReceptionBusinessContext& x){if(!x.result_match)return common_failure("PoliticalOfficeReceptionResultReadRequired");auto valid=validate_political_office_reception_sources(x.rules);return valid.ok?validate_political_office_reception_player_results(x.rules.common,x.rules.state,x.result_match):valid;}
Outcome<PoliticalOfficeReceptionQuote> preview_political_office_reception_business(const PoliticalOfficeReceptionBusinessContext& x,const StableId& actor,const PoliticalAssumeOfficeReception& command){auto v=validate_political_office_receptions(x);return v.ok?preview_political_office_reception(x.rules,actor,command):Outcome<PoliticalOfficeReceptionQuote>{false,{},v.error};}
Outcome<StableId> apply_political_office_reception_business_in_candidate(CommonStore& common,PoliticalOfficeReceptionState& state,const PoliticalOfficeReceptionBusinessContext& x,const StableId& actor,const PoliticalAssumeOfficeReception& command,const PoliticalOfficeReceptionQuote& quote){auto v=validate_political_office_receptions(x);return v.ok?apply_political_office_reception_in_candidate(common,state,x.rules,actor,command,quote):Outcome<StableId>{false,{},v.error};}
}
