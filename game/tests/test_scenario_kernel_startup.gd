## 正式prepare目录经Catalog、Factory、全部初态校验进入Kernel；两个年代保持同一人物ID。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Kernel=preload("res://core/world_kernel.gd")
var checks=0
var failures=0
## 引擎初始化后运行有限的新局与坏候选保留检查。
func _init(): call_deferred("run")
## 失败断言保留非零终态。
func check(ok,label):
 checks+=1
 if not ok: failures+=1; printerr("FAIL:",label)
## 真实新局使用受信生成索引，不用人工注入effective catalog替代装载。
func run():
 var path=""
 for argument in OS.get_cmdline_user_args():
  if argument.begins_with("--scenario-content="): path=argument.trim_prefix("--scenario-content=")
 check(not path.is_empty(),"explicit formally prepared fixture supplied")
 if path.is_empty(): quit(1); return
 var kernel=Kernel.new()
 for row in [["scenario.west","west.square",24],["scenario.east","east.square",44]]:
  var started=kernel.new_game(row[0],path)
  check(started.ok,"normal kernel startup "+str(started))
  if not started.ok: continue
  var state=kernel.snapshot();var content=kernel.content()
  check(state.tick==0 and state.actors.size()==3,"zero time and exact active instances")
  check(state.actors["actor.player"].location.place_id==row[1],"correct map start")
  check(state.actors["actor.player"].birth_tick==-row[2]*720,"correct historical age")
  check(content.data.actors.has("actor.prototype") and not state.actors.has("actor.prototype"),"prototype remains definition")
  check(content.data.places.size()==2 and content.data.settlements.size()==1,"inactive map hidden from runtime")
  var before=kernel.snapshot()
  var bad=kernel.new_game("scenario.absent",path)
  check(not bad.ok and kernel.snapshot()==before,"failed new game keeps exact old world")
 var chosen=kernel.new_game("scenario.east",path,{},"actor.partner")
 check(chosen.ok,"fourth argument selects allowed actor")
 if chosen.ok:
  var before=kernel.snapshot()
  check(before.player_actor_id=="actor.partner" and before.family.founder_actor_id=="actor.partner","kernel preserves selected identity")
  var rejected=kernel.new_game("scenario.east",path,{},"actor.deceased")
  check(not rejected.ok and kernel.snapshot()==before,"dead selection fails without world replacement")
  var header=Strict.parse_text(FileAccess.get_file_as_string(path)).value
  header.catalog.scenarios["scenario.east"].erase("initial")
  header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
  var temporary="user://scenario-invalid-"+str(OS.get_process_id())+".hmcatalog.json"
  var file=FileAccess.open(temporary,FileAccess.WRITE);file.store_string(JSON.stringify(header,"",true,true)+"\n");file.close()
  rejected=kernel.new_game("scenario.east",temporary)
  check(rejected.get("code")=="PartialScenarioSnapshot" and kernel.snapshot()==before,"partial snapshot never becomes legacy or replaces world")
  DirAccess.remove_absolute(ProjectSettings.globalize_path(temporary))
 check(kernel.has_method("save_context"),"read-only trusted save context exists")
 if kernel.has_method("save_context"):
  var expected=kernel.snapshot()
  var context=kernel.call("save_context")
  check(context.state==expected and context.catalog==kernel.content() and context.registry.get("ok",false),"save context captures exact world catalog and registry")
  context.state.tick=999;context.catalog.data.actors.clear();context.registry.modules.clear()
  var again=kernel.call("save_context")
  check(kernel.snapshot()==expected and not again.catalog.data.actors.is_empty() and not again.registry.modules.is_empty(),"caller cannot mutate live state catalog or registry")
 check(kernel.has_method("restore_with_catalog"),"explicit restore boundary exists")
 if kernel.has_method("restore_with_catalog"):
  var source=kernel.save_context()
  var target=Kernel.new()
  check(target.new_game("scenario.west",path).ok,"target has another existing world")
  var before=target.save_context()
  var bad=source.state.duplicate(true);bad.actors.erase("actor.partner")
  var refused=target.call("restore_with_catalog",bad,source.catalog,source.registry)
  check(not refused.ok and target.snapshot()==before.state and target.content()==before.catalog,"bad restore preserves world and catalog")
  refused=target.call("restore_with_catalog",source.state,source.catalog,{})
  check(not refused.ok and target.snapshot()==before.state,"explicit bad registry cannot be rebuilt")
  var restored=target.call("restore_with_catalog",source.state,source.catalog,source.registry)
  check(restored.ok and target.snapshot()==source.state and target.content()==source.catalog,"valid effective view restores exact selected founder without initialization")
  source.state.tick=888;source.catalog.data.actors.clear();source.registry.modules.clear()
  check(target.snapshot().tick==0 and not target.content().data.actors.is_empty() and not target.save_context().registry.modules.is_empty(),"restored triple is detached from caller")
 print("SCENARIO_KERNEL_CHECKS=",checks," FAILURES=",failures)
 quit(0 if failures==0 else 1)
