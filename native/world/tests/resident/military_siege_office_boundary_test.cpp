// 实际Common头表的窄反例：不构造World、不复制另一套军职算法，也不把今天任职视作过去授权。
#include "resident/domains/military/military_siege_office_boundary.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {int checks=0,bad=0;void check(bool yes,const char* label){++checks;if(!yes){++bad;std::cerr<<label<<'\n';}}
CommonState state(std::initializer_list<FactRecord> facts){CommonState s;s.next_instance_id=100;s.identity.tick=8;for(const auto& f:facts)s.facts.insert(f.id,f);return s;}}
int main(){
 const StableId edge="military_siege_lifecycle:4";const FactRecord accept{"action:1","military_action","military"},resign{"military:2","office_resignation","military"},before{"military:3","military_occupation","military"},boundary{edge,"military_siege_strategic","military"},later{"military:5","military_integration","military"};
 const std::vector<MilitarySiegeOfficeSourceRef> refs{{accept.id,accept.kind},{resign.id,resign.kind},{before.id,before.kind},{later.id,later.kind}};
 auto normal=state({accept,resign,before,boundary,later});check(validate_military_siege_office_boundary(normal,edge,refs).ok,"same tick real sequence");
 check(!validate_military_siege_office_boundary(state({resign,before,boundary,accept,later}),edge,refs).ok,"same tick move accept behind boundary");
 check(!validate_military_siege_office_boundary(state({accept,before,boundary,resign,later}),edge,refs).ok,"same tick move resign behind boundary");
 check(!validate_military_siege_office_boundary(state({accept,resign,before,later,boundary}),edge,refs).ok,"same tick later disable cannot move before boundary");
 check(!validate_military_siege_office_boundary(state({resign,accept,before,boundary,later}),edge,refs).ok,"same side events cannot reverse order");
 auto wrong=normal;wrong.facts.find(resign.id)->kind="military_delivery";check(!validate_military_siege_office_boundary(wrong,edge,refs).ok,"wrong resignation kind");wrong=normal;wrong.facts.find(before.id)->domain="politics";check(!validate_military_siege_office_boundary(wrong,edge,refs).ok,"wrong domain");
 auto alternate=state({{"military:1","military_action","military"},boundary});check(!validate_military_siege_office_boundary(alternate,edge,{{"military:1","military_action"}}).ok,"wrong accept prefix");
 alternate=state({{"action:01","military_action","military"},boundary});check(!validate_military_siege_office_boundary(alternate,edge,{{"action:01","military_action"}}).ok,"noncanonical allocation");
 auto duplicate=refs;duplicate.push_back(refs.front());check(!validate_military_siege_office_boundary(normal,edge,duplicate).ok,"duplicate source");wrong=normal;wrong.facts.find(edge)->kind="military_battle";check(!validate_military_siege_office_boundary(wrong,edge,refs).ok,"wrong boundary kind");check(!validate_military_siege_office_boundary(normal,"military_siege_lifecycle:9",refs).ok,"missing boundary");
 check(validate_military_siege_office_boundary(state({boundary}),edge,{}).ok,"original unheld office has no events");
 for(const auto* reason:{"responsible_actor_lost","authority_lost","cancelled"})check(validate_military_siege_tracked_termination_reason(reason).ok,"real terminal source accepted");check(!validate_military_siege_tracked_termination_reason("truce").ok,"unproved truce refused before write");check(!validate_military_siege_tracked_termination_reason("unknown").ok,"unknown terminal refused");
 std::cout<<"SIEGE_OFFICE_BOUNDARY="<<checks<<" FAILURES="<<bad<<" SCOPE=COMMON_HEADER_ONLY\n";return bad?1:0;
}
