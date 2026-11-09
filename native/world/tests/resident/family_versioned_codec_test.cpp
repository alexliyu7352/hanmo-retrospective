// 原冻结 visitor 逐版本字节对照；这里只验有限格式，不冒人物/关系全史准入。
#include "resident/domains/family/family_checkpoint_versioned.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
#ifdef HANMO_COMPARE_ORIGINAL_FAMILY_CODEC
namespace hanmo::resident {Status original_visit_family_checkpoint(CodecVisitor&,FamilyState&,bool,bool,bool,bool,bool);}
#endif
namespace {
Integer checks=0;void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
FamilyState fixture(bool voluntary,bool narrative){FamilyState s;s.founder_actor_id="parent";
 IdentityCreationFact born;born.prototype_id="prototype";born.actor_id="actor:2";born.created_tick=2;born.birth_tick=-100;born.display_name="child";born.source="action:1";s.facts.insert("actor:2",born);
 if(voluntary){SuccessionConsentFact f;auto& a=f.agreement;a.request_id="request";a.relation_fact_id="family:3";a.dialogue_id="dialogue";a.node_id="yes";a.node_kind="say";a.session_id="interaction:4";a.scenario_id="scene";a.world_id[0]=1;a.accepted_tick=10;a.accepted_revision=3;a.predecessor_birth_tick=-40*720;a.successor_birth_tick=-20*720;a.context={"home","child","role"};a.context.field_order={"place_id","role_id","provider_actor_id"};
 a.personal.predecessor_actor_id="parent";a.personal.successor_actor_id="child";a.personal.source_wallet_id="cash:parent";a.personal.destination_wallet_id="cash:child";a.personal.source_inventory_id="bag:parent";a.personal.destination_inventory_id="bag:child";
 if(narrative){a.context.role_id.clear();a.context.kind="narrative";a.context.field_order.push_back("kind");}s.facts.insert("family:5",f);}
 return s;}
BoundaryValue input(const BoundaryValue& out){auto world=out;world.as<BoundaryValue::Dict>()->emplace_back("actions",BoundaryValue{BoundaryValue::Dict{}});return world;}
}
int main(){try{
 for(Integer version=1;version<=16;++version){auto shape=inspect_family_codec_version(version);check(shape.ok,"old finite version");auto state=fixture(shape.value.voluntary_succession,false);auto old=state;CodecVisitor::Buffer before,after,compat;CodecVisitor aw(after),cw(compat);
 check(visit_family_checkpoint_versioned(aw,state,version).ok&&aw.finish().ok,"versioned original write");
 check(visit_family_checkpoint(cw,old,shape.value.household_control,shape.value.character_creation,false,shape.value.adult_adoption,shape.value.voluntary_succession).ok&&cw.finish().ok&&compat==after,"old entry exact bytes");
#ifdef HANMO_COMPARE_ORIGINAL_FAMILY_CODEC
 CodecVisitor bw(before);check(original_visit_family_checkpoint(bw,old,shape.value.household_control,shape.value.character_creation,false,shape.value.adult_adoption,shape.value.voluntary_succession).ok&&bw.finish().ok&&before==after,"frozen original 1..16 bytes");
#endif
 const auto& bytes=after;CodecVisitor reader(bytes);FamilyState read;check(visit_family_checkpoint_versioned(reader,read,version).ok&&reader.finish().ok&&compare_boundary(encode_family_state(state),encode_family_state(read)).exact(),"old typed roundtrip");
 auto json=encode_family_state_versioned(state,version);check(json.ok&&compare_boundary(json.value,encode_family_state(state)).exact(),"old JSON unchanged");auto decoded=decode_family_seed_versioned(input(json.value),version);check(decoded.ok&&compare_boundary(json.value,encode_family_state(decoded.value)).exact(),"old JSON read unchanged");
 }
 for(Integer version=25;version<=32;++version){auto state=fixture(true,true);CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_family_checkpoint_versioned(writer,state,version).ok&&writer.finish().ok,"new explicit Known context write");const auto& in=bytes;CodecVisitor reader(in);FamilyState read;check(visit_family_checkpoint_versioned(reader,read,version).ok&&reader.finish().ok&&compare_boundary(encode_family_state(state),encode_family_state(read)).exact(),"new Known typed source roundtrip");CodecVisitor oldreader(in);FamilyState old;check(!visit_family_checkpoint_versioned(oldreader,old,version-16).ok,"old typed version refuses new context metadata");
 auto json=encode_family_state_versioned(state,version);check(json.ok,"new JSON write");auto decoded=decode_family_seed_versioned(input(json.value),version);check(decoded.ok&&compare_boundary(json.value,encode_family_state(decoded.value)).exact(),"new JSON read source");check(!decode_family_seed_versioned(input(json.value),version-16).ok&&!encode_family_state_versioned(state,version-16).ok,"old JSON profile rejects new source");check(!validate_family_codec_module_pair(version,2,1,{}).ok&&validate_family_codec_module_pair(version,2,1,1).ok,"Known pair mandatory");
 }
 auto wrong=fixture(true,true);std::get<SuccessionConsentFact>(*wrong.facts.find("family:5")).agreement.context.role_id="foreign_role";CodecVisitor::Buffer bad_bytes;CodecVisitor bad_writer(bad_bytes);check(!visit_family_checkpoint_versioned(bad_writer,wrong,25).ok&&!encode_family_state_versioned(wrong,25).ok,"new typed and JSON both reject role in Known context");
 for(Integer version:{0,17,18,19,20,21,22,23,24,33}){FamilyState s;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(!visit_family_checkpoint_versioned(writer,s,version).ok&&!encode_family_state_versioned(s,version).ok&&!decode_family_seed_versioned(input(encode_family_state(s)),version).ok,"all actual entry points reject unsupported version");}
 std::cout<<"FAMILY_VERSIONED_CODEC "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
