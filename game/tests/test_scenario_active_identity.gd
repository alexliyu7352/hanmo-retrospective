## 有效目录的静态身份、缺失首错和原型动态创建合同；只跑短状态校验。
extends SceneTree
const Factory=preload("res://core/world_factory.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
const Shape=preload("res://core/base/world_shape.gd")
const Identity=preload("res://core/actor_identity.gd")
const Estate=preload("res://core/estate_rules.gd")
var checks=0
var failures=0
## 引擎初始化后同步运行独立短合同。
func _init(): call_deferred("run")
## 真实失败计数并保留非零退出。
func check(ok,label):
 checks+=1
 if not ok: failures+=1; printerr("FAIL:",label)
## 编译可信测试fixture的索引包装，保持源与世界身份明确。
func fixture():
 var text=FileAccess.get_file_as_string("res://../schemas/content.schema.json")
 var registered=Snapshot.register_schema(text)
 var schema=Strict.parse_text(text).value
 var pack=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/content_packs/second_scenario/two_eras.json")).value
 var data={"texts":pack.texts,"flags":pack.flags,"required_capabilities":pack.required_capabilities}
 for table in schema["x-hanmo-tables"]:
  data[table]={}
  for row in pack.get(table,[]): data[table][row.id]=row
 return {"ok":true,"data":data,"content_id":pack.id,"content_version":pack.version,"content_digest":(JSON.stringify(data,"",true,true)+"\n").sha256_text(),"schema_sha256":registered.schema_sha256}
## 不让额外动态人掩盖缺失静态人，也不把保留原型直接当真人。
func run():
 var catalog=fixture()
 for scenario in ["scenario.west","scenario.east"]:
  var built=Factory.create(catalog,scenario)
  var state=built.state; var effective=built.catalog
  check(Estate.initialize(state,effective.data).ok,"real estate initialization")
  check(Shape.validate(state,effective).ok,"shape accepts explicit active people")
  check(Identity.validate(state,effective.data).ok,"identity accepts inactive definition")
  var missing=state.duplicate(true); missing.actors.erase("actor.partner")
  missing.actors["actor:999"]=state.actors["actor.player"].duplicate(true)
  check(Shape.validate(missing,effective).get("code")=="MissingWorldEntities","missing active ID cannot hide behind count")
  var injected=state.duplicate(true); injected.actors["actor.prototype"]=state.actors["actor.player"].duplicate(true)
  check(Shape.validate(injected,effective).get("code")=="InactiveStaticActor","inactive static instance rejected")
  injected.actors["actor.prototype"].id="actor.prototype"
  check(Identity.validate(injected,effective.data).get("code")=="InactiveStaticActor","identity rejects inactive static instance")
  state.facts["adoption:test"]={"kind":"family_adoption"}
  var spawned=Identity.instantiate(state,effective.data,"actor.prototype","新生人物",-720,state.actors["actor.player"].location.place_id,"adoption:test")
  check(spawned.ok,"inactive prototype can create dynamic identity")
  if spawned.ok:
   check(Identity.validate(state,effective.data).ok,"dynamic source chain remains valid")
   check(Shape.validate(state,effective).ok,"dynamic actor uses active place")
 print("SCENARIO_ACTIVE_CHECKS=",checks," FAILURES=",failures)
 quit(0 if failures==0 else 1)
