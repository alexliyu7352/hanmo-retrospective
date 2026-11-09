// 旧撤职实际头才可读取本人/原辖区；历史控制读不借今日metadata改旧签字。
#define POLITICAL_DISMISSAL_SOURCE_FIXTURE_ONLY
#include "political_dismissal_source_test.cpp"
#include "resident/domains/politics/dismissal_sources/political_dismissal_control.hpp"
#include "resident/domains/family/family_model.hpp"
int main(){try{Fixture f;auto source=f.remove();FamilyState family;family.founder_actor_id="leader";
 auto player=read_political_dismissal_player_before_fact(f.common,family,nullptr,source.tick,source.dismissal_fact_id);check(player.ok&&player.value=="leader","exact old dismissal permits original historical player read");
 check(!read_political_dismissal_player_before_fact(f.common,family,nullptr,source.tick,source.office_change_fact_id).ok,"adjacent Office change is not a dismissal boundary");
 check(!read_political_dismissal_player_before_fact(f.common,family,nullptr,1,source.dismissal_fact_id).ok,"future time cannot authorize old boundary");
 f.common.state().metadata.player_actor_id="worker";check(!read_political_dismissal_player_before_fact(f.common,family,nullptr,0,source.dismissal_fact_id).ok,"today player change without original control source rejected");f.common.state().metadata.player_actor_id="leader";
 PoliticalDismissalTerritoryContext territory{f.common,f.politics,f.pc};check(validate_political_dismissal_seats(territory,f.policy,0,source.dismissal_fact_id).ok,"actual original single seat proven at dismissal");
 f.politics.jurisdictions.find("county")->revision=1;check(!validate_political_dismissal_seats(territory,f.policy,0,source.dismissal_fact_id).ok,"unproven changed target seat rejected");
 std::cout<<checks<<" original dismissal control/seat checks; complete Family and Military histories are prerequisites\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
