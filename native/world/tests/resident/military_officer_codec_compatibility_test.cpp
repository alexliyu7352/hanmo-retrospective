// 同一具名非空旧Military样本对照原编解码字节；新6的拒读不能借探测格式自动升级。
#include "resident/domains/military/military_codec.hpp"
#include "resident/domains/military/battle_request_codec.hpp"
#ifndef HM_LEGACY_MILITARY_CODEC
#include "resident/domains/military/military_officer_codec.hpp"
#endif
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
MilitaryState fixture(){MilitaryState s;s.offices.push_back({"office","officer",true,"military:1",{}});s.garrisons.push_back({"camp",{"unit"},{},3,{}});MilitaryUnitRecord u;u.id="unit";u.source_garrison_id=u.garrison_id="camp";u.organization_id="org";u.commander_actor_id="officer";u.troop_type_id="infantry";u.member_ids={"soldier:1","soldier:2"};u.fit={"soldier:1"};u.wounded={"soldier:2"};u.training=20;u.fatigue=5;s.military_units.push_back(u);return s;}
CodecVisitor::Buffer encoded(MilitaryState s,std::uint32_t feature){CodecVisitor::Buffer bytes;CodecVisitor v(bytes);v.schema_features=feature;check(visit_military_checkpoint(v,s).ok&&v.finish().ok,"typed write");return bytes;}
bool reads(const CodecVisitor::Buffer& bytes,std::uint32_t feature){MilitaryState s;CodecVisitor v(bytes);v.schema_features=feature;return visit_military_checkpoint(v,s).ok&&v.finish().ok;}
void sample(const CodecVisitor::Buffer& b,const char* name){std::cout<<name<<'=';for(auto c:b)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(c);std::cout<<std::dec<<'\n';}
}
int main(){try{auto plain=encoded(fixture(),0),request=encoded(fixture(),military_request_codec::checkpoint_feature);sample(plain,"LEGACY_3_4");sample(request,"LEGACY_5");check(reads(plain,0)&&reads(request,military_request_codec::checkpoint_feature),"old profile round trip");
#ifndef HM_LEGACY_MILITARY_CODEC
 const auto bit=military_officer_checkpoint_feature;check(!reads(plain,bit)&&!reads(request,bit|military_request_codec::checkpoint_feature),"old payload cannot masquerade as new6");auto six=encoded(fixture(),bit);check(reads(six,bit)&&!reads(six,0),"new6 round trip and old reader refusal");
 auto state=fixture();MilitaryOfficerAssignmentFact f;f.id="military_assignment:10";f.request_key="handover-1";f.actor_id="next";f.authorization_fact_id="politics:8";f.acceptance_fact_id="politics:9";f.place_id="place";f.target={"org","camp","unit","officer","next",{MilitaryUnitCommandSourceKind::authored,"unit"}};f.issuer_actor_id="issuer";f.office_id="office";f.grant_id="grant";f.authority_epoch=1;f.issued_tick=1;f.accepted_tick=2;f.tick=3;state.officer_assignments.push_back(f);state.military_units[0].commander_actor_id="next";auto populated=encoded(state,bit);check(reads(populated,bit),"nonempty assignment round trip");CodecVisitor::Buffer refused;CodecVisitor old(refused);check(!visit_military_checkpoint(old,state).ok,"new fact cannot be emitted as old profile");populated.pop_back();check(!reads(populated,bit),"truncated new6 rejects");
#endif
 std::cout<<"MILITARY_CODEC_CHECKS="<<checks<<" FAILURES=0\n";return 0;}catch(const std::exception& e){std::cerr<<"MILITARY_CODEC_FAIL "<<e.what()<<'\n';return 1;}}
