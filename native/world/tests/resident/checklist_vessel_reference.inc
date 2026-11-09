// 原 Naval 引用的值组件；不创建另一个 World，不声明照护或运输来源已实现。
namespace checklist_vessel_reference {
// 比较原读取者与名义引用委托的同一成功值或原首错，不以任意失败通过。
void same_read(const NavalAssetState &state, const NavalAssetCatalog &catalog,
               const VesselRef &reference) {
  const auto original =
      read_original_naval_vessel_identity(state, catalog, reference.vessel_id);
  const auto typed =
      read_original_naval_vessel_reference(state, catalog, reference);
  require(original.ok == typed.ok, "vessel nominal read preserves status");
  if (original.ok) {
    require(original.value == typed.value,
            "vessel nominal read returns same original row");
  } else {
    require(original.error.original.code == typed.error.original.code &&
                original.error.original.path == typed.error.original.path &&
                original.error.kind == typed.error.kind,
            "vessel nominal read preserves original failure");
  }
}
// 真正执行标量 JSON/二进制的配对及原身份 reader；共享世界只作无副作用守卫。
void run(Run &world) {
  const auto before = world.bytes();
  VesselRef reference{"component::actual_vessel"};
  StableId scalar = reference.vessel_id;
  CodecVisitor::Buffer raw, typed;
  CodecVisitor original_writer(raw), typed_writer(typed);
  must(original_writer.field("vessel_ref", scalar), "original vessel scalar");
  must(naval_vessel_ref_codec::reference(typed_writer, "vessel_ref", reference),
       "typed vessel scalar");
  must(original_writer.finish(), "original vessel scalar finish");
  must(typed_writer.finish(), "typed vessel scalar finish");
  require(raw == typed, "vessel nominal wrapper changes no scalar wire byte");
  const CodecVisitor::Buffer saved = typed;
  CodecVisitor reader(saved);
  VesselRef restored;
  must(naval_vessel_ref_codec::reference(reader, "vessel_ref", restored),
       "vessel scalar binary read");
  must(reader.finish(), "vessel scalar binary read finish");
  require(restored.vessel_id == reference.vessel_id,
          "vessel scalar binary paired identity");
  V json;
  ActorFoundationJsonVisitor json_writer(json);
  must(naval_vessel_ref_codec::reference(json_writer, "vessel_ref", reference),
       "vessel scalar JSON write");
  must(json_writer.finish(), "vessel scalar JSON finish");
  exact(json, V{V::Dict{{"vessel_ref", V{scalar}}}},
        "vessel JSON remains original scalar field");
  const V saved_json = json;
  ActorFoundationJsonVisitor json_reader(saved_json);
  VesselRef decoded;
  must(naval_vessel_ref_codec::reference(json_reader, "vessel_ref", decoded),
       "vessel scalar JSON read");
  must(json_reader.finish(), "vessel scalar JSON read finish");
  require(decoded.vessel_id == reference.vessel_id,
          "vessel scalar JSON paired identity");
  const V wrong{V::Dict{{"vessel_ref", V{V::Dict{{"vessel_id", V{scalar}}}}}}};
  ActorFoundationJsonVisitor wrong_reader(wrong);
  VesselRef rejected;
  const auto wrong_status =
      naval_vessel_ref_codec::reference(wrong_reader, "vessel_ref", rejected);
  require(!wrong_status.ok && wrong_status.error.original.code ==
                                  "InvalidActorFoundationJsonScalar",
          "vessel object spelling is not accepted as alternate layout");

  NavalAssetCatalog catalog;
  NavalVesselDefinition definition;
  definition.id = reference.vessel_id;
  definition.hull_id = "component::original_hull";
  definition.cargo_inventory_id = "component::original_cargo";
  catalog.vessels.push_back(definition);
  NavalAssetState state;
  NavalVesselRecord actual;
  actual.id = definition.id;
  actual.hull_id = definition.hull_id;
  actual.cargo_inventory_id = definition.cargo_inventory_id;
  actual.origin_source = "author:" + definition.id;
  state.vessels.push_back(actual);
  same_read(state, catalog, reference);
  require(read_original_naval_vessel_reference(state, catalog, reference).ok,
          "vessel actual authored identity accepted");
  same_read(state, catalog, VesselRef{});
  same_read(state, catalog, VesselRef{"component::unknown_vessel"});
  state.vessels.front().origin_source = "component::unproved_origin";
  same_read(state, catalog, reference);
  require(!read_original_naval_vessel_reference(state, catalog, reference).ok,
          "vessel forged origin rejected");
  state.vessels.front() = actual;
  state.vessels.push_back(actual);
  same_read(state, catalog, reference);
  require(!read_original_naval_vessel_reference(state, catalog, reference).ok,
          "vessel duplicate actual identity rejected");
  state.vessels.pop_back();
  catalog.vessels.push_back(definition);
  same_read(state, catalog, reference);
  require(!read_original_naval_vessel_reference(state, catalog, reference).ok,
          "vessel duplicate author identity rejected");
  require(world.bytes() == before,
          "vessel value component leaves world intact");
}
} // namespace checklist_vessel_reference
