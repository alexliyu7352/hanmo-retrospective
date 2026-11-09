#include "local_move_component_test.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/native_local_move_catalog.hpp"
#include "resident/kernel/native_field_group_world.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_complete_world_core.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/military/military_supply_exact.hpp"
#include "resident/domains/travel/local_move_source.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/checkpoint_job_budget.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace hanmo::resident {
namespace {
// 只传回真实错误；负例不能以任意异常替代所需精确拒绝。
void check(bool value,const std::string& message) {
 if(!value) throw std::runtime_error("local-move component: "+message);
}
template<class T> T take(Outcome<T> value) {
 check(value.ok,value.error.original.code); return std::move(value.value);
}
void take(Status value) { check(value.ok,value.error.original.code); }
template<class T> void reject(const Outcome<T>& value,const char* code) {
 check(!value.ok && value.error.original.code==code,std::string("expected ")+code+" actual "+value.error.original.code);
}
// 固定原回调仅用于同一真实行的二进制旧 bytes 对照；不会借它写出重复 JSON 过门。
template<class C>Status original_inventory_binding_fields(C& c,InventorySpatialBinding& v,
    const MovementRecordCodecSelection& selection){
 using movement_record_fields::field;
 field(c,"inventory_id",v.inventory_id,selection);
 field(c,"placement",v.placement,selection);
 field(c,"source_ref",v.source_ref,selection);
 field(c,"revision",v.revision,selection);return c.status();
}
// 每个损坏 JSON 使用独立有限原 job；预期首错不能污染下一反例或正例候选。
void reject_inventory_json(const BoundaryValue& json,const MovementRecordCodecSelection& selection,
                           const char* code){
 auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
 check(job.ok,"inventory negative original engineering job");
 auto resource=checkpoint_memory_resource(job.value,CheckpointJobUse::candidate);
 check(resource.ok,"inventory negative candidate resource");
 ScopedCheckpointMemory memory(resource.value);
 CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),job.value);
 try{
  ScopedCheckpointJsonDecode scope(budget,job.value,"transport_manifest_bindings",resource.value);
  InventorySpatialBinding restored;ActorFoundationJsonVisitor reader(json);
  auto result=movement_record_fields::value(reader,restored,selection);
  if(result.ok)result=reader.finish();
  check(!result.ok&&result.error.original.code==code,
      std::string("inventory exact rejection ")+code+" actual "+result.error.original.code);
 }catch(checkpoint_job_detail::FailureException& failure){
  check(failure.error.try_materialize_rule_error(),"inventory original job error");
  const auto* error=failure.error.rule_error();
  check(error&&error->original.code==code,"inventory original job exact rejection");
 }
}
// 只读真实 inventory 行；零行及未出现的 carrier 形态明记未覆盖，不造作者正事实。
void check_inventory_placement_codecs(const WorldBundle& world,
    const MovementRecordCodecSelection& selection,CheckpointJobBudget& job,
    CodecDecodeBudget& budget,const std::shared_ptr<CheckpointMemoryResource>& resource){
 Integer rows=0,carrier_rows=0,stable_rows=0,negative=0;unsigned carrier_mask=0;
 if(world.spatial_carriers)for(const auto& item:world.spatial_carriers->inventory_bindings.rows()){
  auto row=item.second;
  check(item.first==row.inventory_id,"actual inventory placement key");
  CodecVisitor::Buffer old_bytes,bytes;
  CodecVisitor old_writer(old_bytes);take(original_inventory_binding_fields(old_writer,row,selection));take(old_writer.finish());
  CodecVisitor writer(bytes);take(movement_record_fields::value(writer,row,selection));take(writer.finish());
  check(bytes==old_bytes,"inventory binary original two-tag bytes");
  InventorySpatialBinding binary_row;
  CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes),
      engineering_checkpoint_capacity_profile(),"transport_manifest_bindings",budget);
  take(movement_record_fields::value(reader,binary_row,selection));take(reader.finish());
  CodecVisitor::Buffer binary_again;CodecVisitor binary_writer(binary_again);
  take(movement_record_fields::value(binary_writer,binary_row,selection));take(binary_writer.finish());
  check(bytes==binary_again,"inventory binary symmetry");
  BoundaryValue json;BoundaryProjectionWriter sink(json);
  take(emit_projection_record<ActorFoundationProjectionArchive>(sink,"",row,
      [&](auto& archive,auto& value){return movement_record_fields::value(archive,value,selection);}));take(sink.finish());
  ProjectionShapeWriter shape(engineering_checkpoint_capacity_profile(),"transport_manifest_bindings",job);
  take(emit_projection_record<ActorFoundationProjectionArchive>(shape,"",row,
      [&](auto& archive,auto& value){return movement_record_fields::value(archive,value,selection);}));take(shape.finish());
  InventorySpatialBinding json_row;
  {
   ScopedCheckpointJsonDecode scope(budget,job,"transport_manifest_bindings",resource);
   ActorFoundationJsonVisitor json_reader(static_cast<const BoundaryValue&>(json));
   take(movement_record_fields::value(json_reader,json_row,selection));take(json_reader.finish());
  }
  CodecVisitor::Buffer from_json;CodecVisitor json_writer(from_json);
  take(movement_record_fields::value(json_writer,json_row,selection));take(json_writer.finish());
  check(bytes==from_json,"inventory JSON preserves both actual discriminants");
  BoundaryValue direct;ActorFoundationJsonVisitor direct_writer(direct);
  take(movement_record_fields::value(direct_writer,row,selection));take(direct_writer.finish());
  check(take(render_boundary_json(json))==take(render_boundary_json(direct)),"inventory tree/projection symmetry");
  auto* placement=json.find("placement");check(placement&&placement->find("variant"),"actual placement JSON");
  auto outer=json;outer.find("placement")->find("variant")->data=Integer{97};
  reject_inventory_json(outer,selection,"InvalidMovementVariant");++negative;
  auto duplicate=json;auto* duplicate_fields=duplicate.find("placement")->as<BoundaryValue::Dict>();
  const auto original_tag=*duplicate.find("placement")->find("variant");
  duplicate_fields->emplace_back("variant",original_tag);
  reject_inventory_json(duplicate,selection,"DuplicateResidentField");++negative;
  auto wrong_type=json;wrong_type.find("placement")->find("variant")->data=StableId{"wrong-tag-type"};
  reject_inventory_json(wrong_type,selection,"InvalidActorFoundationJsonScalar");++negative;
  auto extra=json;extra.find("placement")->as<BoundaryValue::Dict>()->emplace_back("unknown",BoundaryValue{Integer{1}});
  reject_inventory_json(extra,selection,"UnsupportedResidentField");++negative;
  if(const auto* carrier=std::get_if<SpatialCarrierRef>(&row.placement)){
   ++carrier_rows;carrier_mask|=1u<<carrier->index();
   check(placement->find("carrier")&&placement->find("carrier")->find("variant"),"nested carrier discriminant");
   auto inner=json;inner.find("placement")->find("carrier")->find("variant")->data=Integer{97};
   reject_inventory_json(inner,selection,"InvalidMovementVariant");++negative;
   auto missing=json;auto* fields=missing.find("placement")->as<BoundaryValue::Dict>();
   fields->erase(std::remove_if(fields->begin(),fields->end(),[](const auto& field){return field.first=="carrier";}),fields->end());
   reject_inventory_json(missing,selection,"MissingResidentField");++negative;
   // 原错误平铺有两个真实 variant：仍拒绝，不取first/last或静默丢掉任一判别。
   auto flattened=missing;auto* flat=flattened.find("placement")->as<BoundaryValue::Dict>();
   for(const auto& field:*placement->find("carrier")->as<BoundaryValue::Dict>())flat->push_back(field);
   reject_inventory_json(flattened,selection,"DuplicateResidentField");++negative;
  }else{
   ++stable_rows;BoundaryValue old_json;ActorFoundationJsonVisitor old_json_writer(old_json);
   take(original_inventory_binding_fields(old_json_writer,row,selection));take(old_json_writer.finish());
   check(take(render_boundary_json(json))==take(render_boundary_json(old_json)),"inventory original valid variant1 JSON bytes");
  }
  ++rows;
 }
 std::cout<<"FIELD_THIRD_GATE INVENTORY_PLACEMENT_CODEC rows="<<rows
          <<" carrier_rows="<<carrier_rows<<" stable_rows="<<stable_rows
          <<" carrier_mask="<<carrier_mask<<" negatives="<<negative
          <<" status="<<(rows?"EXERCISED_REAL_BANK_ROWS":"NOT_EXERCISED")<<"\n";
}

// 原真实 World/M/phase 的 owner 只活在同步检查栈，不能借到下一份候选。
template<class F>void with_supply_group(const WorldBundle& world,F&& f){
 const auto now=take(world.actual_phase_moment());
 auto phase=world.has_active_read_phase()?std::unique_ptr<PhaseCurrentReadScope>{}:
     take(world.enter_phase_read_context("supply_rebind_component","military",{now,now},now));
 auto owner=take(world.complete_exact_time_bindings());check(bool(owner),"supply original complete owner");
 const auto* core=owner->reads().original_core;check(core&&&core->characters.early.world==&world,"supply original same-world core");
 auto group=take(NativeFieldGroupWorld::create(world,*core));f(*group);
}
// 只损坏真实 bank 的深副本；每个首错独占有限 job，绝不修改原运行 World。
template<class Mutate>void reject_supply_world_copy(const WorldBundle& original,const StableId& id,
    const char* code,Mutate&& mutate){
 auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
 check(job.ok,"supply negative engineering job");auto resource=checkpoint_memory_resource(job.value,CheckpointJobUse::candidate);
 check(resource.ok,"supply negative resource");ScopedCheckpointMemory memory(resource.value);
 WorldBundle changed(original);mutate(changed);
 with_supply_group(changed,[&](const NativeFieldGroupWorld& owner){
   const auto* row=changed.group_movement_executions->military_supply_rebinds.find(id);
   const auto status=row?owner.group().actual_sources.validate_military_supply_rebind(*row):owner.group().actual_sources.validate_military_supply_causes();
   check(!status.ok&&status.error.original.code==code,std::string("supply original negative ")+code+" actual "+status.error.original.code);
 });
}
// 损坏格式的首错独占有限解码 job；未知枚举、丢 cause、旧版本均不得污染下一行。
void reject_supply_json(const BoundaryValue& json,const MovementRecordCodecSelection& selection,const char* code){
 auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
 check(job.ok,"supply JSON negative job");auto resource=checkpoint_memory_resource(job.value,CheckpointJobUse::candidate);
 check(resource.ok,"supply JSON negative resource");ScopedCheckpointMemory memory(resource.value);
 CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),job.value);
 try{ScopedCheckpointJsonDecode scope(budget,job.value,"group_transport",resource.value);
  GroupMilitarySupplyRebindSource row;ActorFoundationJsonVisitor reader(json);
  auto status=movement_record_fields::value(reader,row,selection);if(status.ok)status=reader.finish();
  check(!status.ok&&status.error.original.code==code,std::string("supply JSON negative ")+code+" actual "+status.error.original.code);
 }catch(checkpoint_job_detail::FailureException& failure){
  check(failure.error.try_materialize_rule_error(),"supply JSON original error");const auto* error=failure.error.rule_error();
  check(error&&error->original.code==code,"supply JSON original exact rejection");
 }
}
// 同趟供给四项只计真实产生的行；未出现 Held/Road/多次供给的形态明确不冒正例。
void check_supply_rebind_components(const WorldBundle& world,const MovementRecordCodecSelection& selection,
    CheckpointJobBudget& job,CodecDecodeBudget& budget,const std::shared_ptr<CheckpointMemoryResource>& resource){
 if(!world.catalog->field_group_passage)return;
 check(world.group_movement_executions&&world.spatial_carriers,"actual supply group roots");
 const auto& groups=*world.group_movement_executions;Integer rows=0,negative=0,codec=0;unsigned states=0;bool road=false;Integer opening_causes=0,journey_causes=0,wrong_journey_cases=0;
 for(const auto& supply:world.military.supply_facts)if(supply.cause_fact_ids.size()==1){const auto* head=world.common.state().facts.find(supply.cause_fact_ids.front());if(head&&head->domain=="travel"&&head->kind=="group_journey")++journey_causes;else ++opening_causes;}
 with_supply_group(world,[&](const NativeFieldGroupWorld& owner){take(owner.group().actual_sources.validate_military_supply_causes());});
 for(const auto& [id,actual]:groups.military_supply_rebinds.rows()){
  const SourceRef source{"travel","group_military_supply_rebind",id};
  check(take(read_group_military_supply_rebind_source(world.common,*world.spatial_carriers,*world.movement_exact,groups,source,1))==&actual,"actual rebind row identity");
  reject(read_group_military_supply_rebind_source(world.common,*world.spatial_carriers,*world.movement_exact,groups,source,97),"MilitarySupplyRebindSourceRequired");++negative;
  reject(read_group_military_supply_rebind_source(world.common,*world.spatial_carriers,*world.movement_exact,groups,{"military",source.kind,id},1),"MilitarySupplyRebindSourceRequired");++negative;
  auto changed=groups;auto bad=actual;bad.before_composition_revision=bad.before_composition_revision==std::numeric_limits<Integer>::max()?0:bad.before_composition_revision+1;changed.military_supply_rebinds.set(id,std::move(bad));
  reject(read_group_military_supply_rebind_source(world.common,*world.spatial_carriers,*world.movement_exact,changed,source,1),"MilitarySupplyRebindOriginalPrefixMismatch");++negative;
  auto row=actual;CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);
  take(movement_record_fields::value(writer,row,selection));take(writer.finish());
  GroupMilitarySupplyRebindSource binary;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes),engineering_checkpoint_capacity_profile(),"group_transport",budget);
  take(movement_record_fields::value(reader,binary,selection));take(reader.finish());
  CodecVisitor::Buffer again;CodecVisitor replay(again);take(movement_record_fields::value(replay,binary,selection));take(replay.finish());
  check(bytes==again,"supply rebind binary all owned fields");
  BoundaryValue json;ActorFoundationJsonVisitor json_writer(json);take(movement_record_fields::value(json_writer,row,selection));take(json_writer.finish());
  GroupMilitarySupplyRebindSource decoded;
  {ScopedCheckpointJsonDecode scope(budget,job,"group_transport",resource);ActorFoundationJsonVisitor json_reader(static_cast<const BoundaryValue&>(json));take(movement_record_fields::value(json_reader,decoded,selection));take(json_reader.finish());}
  CodecVisitor::Buffer json_bytes;CodecVisitor json_replay(json_bytes);take(movement_record_fields::value(json_replay,decoded,selection));take(json_replay.finish());check(bytes==json_bytes,"supply rebind JSON/binary symmetry");
  ProjectionShapeWriter shape(engineering_checkpoint_capacity_profile(),"group_transport",job);take(emit_projection_record<ActorFoundationProjectionArchive>(shape,"",row,[&](auto& archive,auto& value){return movement_record_fields::value(archive,value,selection);}));take(shape.finish());
  auto old_selection=selection;old_selection.group_version=6;reject_supply_json(json,old_selection,"MilitarySupplyRebindRequiresGroup7");++negative;
  auto bad_tag=json;bad_tag.find("disposition")->find("enum")->data=Integer{97};reject_supply_json(bad_tag,selection,"InvalidMovementEnum");++negative;
  auto no_cause=json;auto* fields=no_cause.as<BoundaryValue::Dict>();fields->erase(std::remove_if(fields->begin(),fields->end(),[](const auto& pair){return pair.first=="cause_fact_ids";}),fields->end());
  reject_supply_json(no_cause,selection,"MissingResidentField");++negative;
  auto bad_mapping=groups;auto mapping=actual;
  auto* running=std::get_if<RunningMovementMap>(&mapping.original_mapping);
  auto& funding=running?running->budget_rate:std::get<HeldMovementMap>(mapping.original_mapping).resume_budget_rate;
  funding=take(add_fractions(funding,take(normalize_fraction("1","1"))));bad_mapping.military_supply_rebinds.set(id,std::move(mapping));
  reject(read_group_military_supply_rebind_source(world.common,*world.spatial_carriers,*world.movement_exact,bad_mapping,source,1),actual.disposition==GroupSupplyRateDisposition::closed_unit?"MilitarySupplyClosedMappingRequired":"MilitarySupplyRebindOriginalRateMismatch");++negative;
  ++rows;codec+=2;states|=1u<<Integer(actual.disposition);road=road||actual.before_budget==0;
 }
 static bool full_negative_checked=false;
 if(!full_negative_checked&&!groups.military_supply_rebinds.empty()){
  const auto id=groups.military_supply_rebinds.order().front();
  reject_supply_world_copy(world,id,"MilitarySupplyRebindForeignProfile",[&](WorldBundle& changed){auto& value=changed.group_movement_executions->military_supply_rebinds.find(id)->after_profile.original_load_total;value=value==std::numeric_limits<Integer>::max()?0:value+1;});++negative;
  reject_supply_world_copy(world,id,"MilitarySupplyRebindForeignProfile",[&](WorldBundle& changed){auto& value=changed.group_movement_executions->military_supply_rebinds.find(id)->after_profile.original_load_people;value=value==std::numeric_limits<Integer>::max()?1:value+1;});++negative;
  reject_supply_world_copy(world,id,"MilitarySupplyRebindForeignProfile",[&](WorldBundle& changed){changed.group_movement_executions->military_supply_rebinds.find(id)->after_profile.actual_rule_source.id="not-original-formation";});++negative;
  reject_supply_world_copy(world,id,"MilitarySupplyRebindCoverageRequired",[&](WorldBundle& changed){changed.group_movement_executions->military_supply_rebinds.erase(id);});++negative;
  const auto& original=*groups.military_supply_rebinds.find(id);const auto supply_id=original.cause_fact_ids.front();
  reject_supply_world_copy(world,id,"MilitarySupplyActualCauseRequired",[&](WorldBundle& changed){for(auto& supply:changed.military.supply_facts)if(supply.id==supply_id)supply.cause_fact_ids.push_back(supply.cause_fact_ids.front());});++negative;
  for(const auto& pair:world.spatial_carriers->journeys.rows())if(pair.first!=original.journey_id){
   const auto* head=world.common.state().facts.find(pair.first);
   if(!head||head->domain!="travel"||head->kind!="group_journey")continue;
   // 只用真正存在且早于供养的另一旅程，避免把“缺头”冒充同军/异旅反例。
   auto supply_pos=take(read_actual_common_fact_position(world.common,{"military","military_supply",supply_id}));
   auto other_pos=take(read_actual_common_fact_position(world.common,{"travel","group_journey",pair.first}));
   if(other_pos>=supply_pos)continue;
   reject_supply_world_copy(world,id,"MilitarySupplyRebindCauseMismatch",[&](WorldBundle& changed){for(auto& supply:changed.military.supply_facts)if(supply.id==supply_id)supply.cause_fact_ids={pair.first};});++negative;++wrong_journey_cases;break;
  }
  full_negative_checked=true;
 }
 std::cout<<"FIELD_THIRD_GATE MILITARY_SUPPLY_REBIND rows="<<rows<<" states_mask="<<states<<" road="<<road<<" codec="<<codec<<" negatives="<<negative<<" opening_causes="<<opening_causes<<" journey_causes="<<journey_causes<<" wrong_journey_cases="<<wrong_journey_cases<<" status="<<(rows?"EXERCISED_REAL_BANK_ROWS":"NOT_EXERCISED")<<"\n";
}
}
// 所有正例行都取真实 bank；目录副本和序列副本仅作明确负例，不执行新业务。
void check_local_move_components(const WorldBundle& world,LocalMoveComponentCoverage& coverage) {
 // 二进制与 JSON 的真实候选同借一个有限 job，不能靠无上下文 decode 绕过生命周期。
 auto job=CheckpointJobBudget::create(engineering_checkpoint_capacity_profile(),engineering_slot_capacity_profile());
 check(job.ok,"original component engineering job");
 auto resource=checkpoint_memory_resource(job.value,CheckpointJobUse::candidate);
 check(resource.ok,"original component candidate resource");
 ScopedCheckpointMemory memory(resource.value);
 CodecDecodeBudget budget(engineering_checkpoint_capacity_profile(),job.value);
 check(world.catalog && world.movement_exact,"actual travel roots required");
 const auto selected=take(selected_native_local_move_version(world.catalog->identity));
 check(selected==1,"actual derived selection");
 auto missing=world.catalog->identity;
 missing.module_versions.erase(std::remove_if(missing.module_versions.begin(),missing.module_versions.end(),
     [](const auto& row){return row.first==local_move_derivation_key;}),missing.module_versions.end());
 reject(selected_native_local_move_version(missing),"LocalMoveDerivedSelectionRequired");
 auto wrong=world.catalog->identity;
 for(auto& row:wrong.module_versions) if(row.first==local_move_derivation_key) row.second="travel/3";
 reject(selected_native_local_move_version(wrong),"LocalMoveDerivationMismatch");
 auto duplicate=world.catalog->identity; duplicate.module_versions.emplace_back(local_move_module,"1");
 reject(selected_native_local_move_version(duplicate),"LocalMoveModuleMismatch");
 coverage.selection_cases+=4;
 take(validate_native_local_move_layout(world));
 const auto selection=take(native_local_move_codec_selection(*world.catalog));
 check_inventory_placement_codecs(world,selection,job.value,budget,resource.value);
 check_supply_rebind_components(world,selection,job.value,budget,resource.value);
 for(const auto& [key,row]:world.movement_exact->local_moves.rows()) {
   const SourceRef source{"travel","local_move",key};
   check(take(read_local_move_source(world.common,*world.movement_exact,source,1))==&row,"actual source row identity");
   reject(read_local_move_source(world.common,*world.movement_exact,source,97),"LocalMoveSourceRequired");
   reject(read_local_move_source(world.common,*world.movement_exact,{"military","local_move",key},1),"LocalMoveSourceRequired");
   reject(read_local_move_source(world.common,*world.movement_exact,{"travel","movement_arrival",key},1),"LocalMoveSourceRequired");
   // 真行副本换成“包括自身”的真实 Common cut，不能借合法前缀逃过紧邻写入门。
   MovementExactState wrong_prefix=*world.movement_exact;
   auto changed=row; changed.before_prefix.common_fact_count+=1;
   changed.before_prefix.last_common_fact=source;
   wrong_prefix.local_moves.set(key,std::move(changed));
   reject(read_local_move_source(world.common,wrong_prefix,source,1),"LocalMoveOriginalPrefixMismatch");
   ++coverage.actual_source_rows; coverage.negative_cases+=4;
 }
 // 四路中的二进制和 JSON 都借真实表；无源时只记选择覆盖，不称源 roundtrip。
 if(world.movement_exact->local_moves.empty()) return;
 CodecVisitor::Buffer bytes;
 CodecVisitor writer(bytes);
 take(visit_local_move_checkpoint(writer,const_cast<MovementExactState&>(*world.movement_exact),1,selection));
 take(writer.finish());
 MovementExactState restored;
 CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes),
     engineering_checkpoint_capacity_profile(),local_move_module,budget);
 take(visit_local_move_checkpoint(reader,restored,1,selection)); take(reader.finish());
 CodecVisitor::Buffer again;
 CodecVisitor replay(again); take(visit_local_move_checkpoint(replay,restored,1,selection)); take(replay.finish());
 check(bytes==again,"binary exact symmetry");
 const auto json=take(encode_local_moves(*world.movement_exact,1,selection));
 MovementExactState json_restored;
 {
  ScopedCheckpointJsonDecode json_scope(budget,job.value,local_move_module,resource.value);
  take(decode_local_moves(json,json_restored,1,selection));
 }
 CodecVisitor::Buffer via_json; CodecVisitor json_writer(via_json);
 take(visit_local_move_checkpoint(json_writer,json_restored,1,selection)); take(json_writer.finish());
 check(bytes==via_json,"JSON and binary same actual fields");
 reject(encode_local_moves(*world.movement_exact,2,selection),"UnsupportedLocalMovePhysicalRevision");
 bytes.pop_back(); MovementExactState truncated;
 try {
  CodecVisitor short_reader(static_cast<const CodecVisitor::Buffer&>(bytes),
      engineering_checkpoint_capacity_profile(),local_move_module,budget);
  const auto short_result=visit_local_move_checkpoint(short_reader,truncated,1,selection);
  check(!short_result.ok && short_result.error.original.code=="TruncatedCheckpoint",
        "truncated source exact rejection");
 } catch(checkpoint_job_detail::FailureException& failure) {
  // 原有限 job 在损坏输入处停止；不能把资源耗尽或其他错误冒充截断反例。
  check(failure.error.try_materialize_rule_error(),"truncated original job error");
  const auto* error=failure.error.rule_error();
  check(error && error->original.code=="TruncatedCheckpoint","truncated original job exact rejection");
 }
 coverage.codec_cases+=2; coverage.negative_cases+=2;
}
} // namespace hanmo::resident
