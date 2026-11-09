// Military7只在显式双pair启用；固定非空Military6样本保持逐字节原格式。
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/military_officer_codec.hpp"
#ifndef HM_FIELD_LEGACY
#include "resident/domains/military/military_field_command_codec.hpp"
#endif
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;void check(bool ok,const char* s){++checks;if(!ok)throw std::runtime_error(s);}
MilitaryState fixture(){MilitaryState s;MilitaryUnitRecord u;u.id="unit";u.source_garrison_id=u.garrison_id="camp";u.organization_id="org";u.commander_actor_id="next";u.member_ids={"soldier:1","soldier:2"};u.fit={"soldier:1"};u.wounded={"soldier:2"};u.training=20;u.fatigue=5;s.military_units.push_back(u);MilitaryOfficerAssignmentFact f;f.id="military_assignment:10";f.request_key="garrison-1";f.actor_id="next";f.authorization_fact_id="politics:8";f.acceptance_fact_id="politics:9";f.place_id="place";f.target={"org","camp","unit","officer","next",{MilitaryUnitCommandSourceKind::authored,"unit"}};f.issuer_actor_id="issuer";f.office_id="office";f.grant_id="grant";f.authority_epoch=1;f.issued_tick=1;f.accepted_tick=2;f.tick=3;s.officer_assignments.push_back(f);return s;}
CodecVisitor::Buffer encode(MilitaryState s,std::uint32_t features){CodecVisitor::Buffer b;CodecVisitor c(b);c.schema_features=features;check(visit_military_checkpoint(c,s).ok&&c.finish().ok,"typed write");return b;}
bool reads(const CodecVisitor::Buffer& b,std::uint32_t features){MilitaryState s;CodecVisitor c(b);c.schema_features=features;return visit_military_checkpoint(c,s).ok&&c.finish().ok;}
}
int main(){try{const auto six=military_officer_checkpoint_feature;auto original=encode(fixture(),six);std::cout<<"MILITARY_6=";for(auto c:original)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);std::cout<<std::dec<<'\n';check(reads(original,six),"fixed six roundtrip");
#ifndef HM_FIELD_LEGACY
 const auto seven=six|military_field_command_checkpoint_feature;check(!reads(original,seven),"six cannot be silently upgraded");auto next=encode(fixture(),seven);check(reads(next,seven)&&!reads(next,six),"explicit seven roundtrip and old reader rejection");check(!reads(next,military_field_command_checkpoint_feature),"field requires explicit original feature");next.pop_back();check(!reads(next,seven),"truncated seven rejected");
#endif
 std::cout<<"MILITARY_FIELD_CODEC="<<checks<<" FAILURES=0\n";return 0;}catch(const std::exception& e){std::cerr<<"MILITARY_FIELD_CODEC_FAIL "<<e.what()<<'\n';return 1;}}
