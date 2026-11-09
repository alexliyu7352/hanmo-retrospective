// 新亲代标签JSON/typed必要坏档门；不启动或修改原family_legacy/1格式。
#include "resident/domains/family/biological_control_origin.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}}
int main(){try{
 using V=BoundaryValue;BiologicalControlOrigin original;original.parent_actor_id="parent";original.child_actor_id=original.original_catalog_actor_id="child";
 BiologicalControlOrigin born;born.kind=BiologicalChildOriginKind::recorded_birth;born.parent_actor_id="parent";born.child_actor_id=born.creation_fact_id="actor:7";born.birth_fact_id="family_birth:6";born.source_available_tick=3;
 for(auto source:{original,born}){auto json=encode_biological_control_origin(source);auto restored=decode_biological_control_origin(json);check(restored.ok&&biological_control_origin_equal(source,restored.value),"JSON changed tagged relation");check(compare_boundary(json,encode_biological_control_origin(restored.value)).exact(),"JSON field order changed");CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_biological_control_origin_checkpoint(writer,restored.value).ok&&writer.finish().ok,"typed encode rejected source");const auto& input=bytes;CodecVisitor reader(input);BiologicalControlOrigin cold;check(visit_biological_control_origin_checkpoint(reader,cold).ok&&reader.finish().ok&&biological_control_origin_equal(source,cold),"typed source round trip changed");bytes.pop_back();const auto& truncated=bytes;CodecVisitor bad_reader(truncated);check(!visit_biological_control_origin_checkpoint(bad_reader,cold).ok||!bad_reader.finish().ok,"truncated origin accepted");
  auto duplicate=json;duplicate.as<V::Dict>()->emplace_back("origin_kind",V{StableId{"recorded_birth"}});check(!decode_biological_control_origin(duplicate).ok,"duplicate kind hidden by first match");auto unknown=json;*unknown.find("origin_kind")=V{StableId{"current_actor"}};check(!decode_biological_control_origin(unknown).ok,"unknown kind inferred from actor");
 }
 auto mixed=encode_biological_control_origin(original);mixed.as<V::Dict>()->emplace_back("birth_fact_id",V{StableId{"child"}});check(!decode_biological_control_origin(mixed).ok,"Installed actor id accepted as fact");
 mixed=encode_biological_control_origin(born);mixed.as<V::Dict>()->emplace_back("original_catalog_actor_id",V{StableId{"actor:7"}});check(!decode_biological_control_origin(mixed).ok,"Birth supplemented with fake catalog fallback");
 auto incorrect=born;incorrect.creation_fact_id="actor:8";CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(!visit_biological_control_origin_checkpoint(writer,incorrect).ok,"wrong actual creation identity accepted");
 incorrect=original;incorrect.source_available_tick=4;CodecVisitor other(bytes);check(!visit_biological_control_origin_checkpoint(other,incorrect).ok,"Installed source moved to later time");
 incorrect=born;incorrect.field_order={"origin_kind","origin_kind"};CodecVisitor order(bytes);check(!visit_biological_control_origin_checkpoint(order,incorrect).ok,"duplicate field order accepted");
 std::cout<<"BIOLOGICAL_CONTROL_ORIGIN_CODEC "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
