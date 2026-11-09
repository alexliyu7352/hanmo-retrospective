## 复用既有two_eras有效东图与收养钱包作者变化，正常两半日生成动态人物；不运行存读或成长长链。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Capture=preload("res://tests/source_capture.gd")
var destination: String=""
var checks: int=0
var failures: int=0

## 延迟到正常引擎初始化后读取已准备内容，不启动UI或Native。
func _init() -> void: call_deferred("run")

## 每个原命令和语义断言均影响退出码，失败即停依赖步骤。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 当前人物、序号、revision来自同一正式命令上下文，不改世界字段。
func send(kernel,kind: String,params: Dictionary={}) -> Dictionary:
	var context: Dictionary=kernel.command_context()
	return kernel.submit({"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})

## 捕获原完整Schema合法三元组；源与原答案读取后活世界和目录严格相等。
func capture(kernel,label: String,actors: Array) -> void:
	var before: Dictionary=kernel.save_context(); var result: Dictionary=Capture.capture(before,destination,label,"actual_kernel_commands",actors)
	check(result.ok and kernel.save_context()==before,"原Schema与只读来源 "+label+" "+str(result.get("code","")))

## 合法公共收养短前缀，完成后不继续继任、长期休整或任何保存。
func run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): destination=arg.trim_prefix("--output=")
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.ok and installed.catalogs.size()==1,"正式compose/stage单一有效收养包"): finish(); return
	var kernel=Kernel.new(); var made: Dictionary=kernel.new_game("adopt::scenario.east",installed.catalogs[0].catalog_path)
	if not check(made.ok,"既有effective east真实玩家开局 "+str(made)): finish(); return
	var initial: Dictionary=kernel.snapshot(); var guardian: String=initial.player_actor_id
	check(kernel.content().data.actors.has("adopt::actor.prototype") and not initial.actors.has("adopt::actor.prototype"),"原型属于目录，未制造额外运行人物")
	capture(kernel,"family_initial",[guardian])
	var moved: Dictionary=send(kernel,"move_local",{"place_id":"adopt::east.office"})
	if not check(moved.ok,"真实move_local抵达东官署 "+str(moved)): finish(); return
	var context: Dictionary=Interaction.for_role(kernel.snapshot(),kernel.content().data,"adopt::role.east")
	var started: Dictionary=send(kernel,"family_adopt",{"context":context,"display_name":"阿宁"})
	if not check(started.ok,"真实family_adopt开始 "+str(started)): finish(); return
	var pending: Dictionary=kernel.snapshot(); var action: String=pending.actors[guardian].action_id
	check(pending.wallets[pending.actors[guardian].wallet_id].reserved.get(action)==20 and pending.actors.size()==initial.actors.size(),"先预留真实20费用，不提前生成动态人物")
	capture(kernel,"family_pending",[guardian,context.provider_actor_id])
	for index in range(2):
		if not check(kernel.advance_one_tick().ok,"真实收养半日 "+str(index+1)): finish(); return
	var completed: Dictionary=kernel.snapshot()
	if not check(completed.family.children.size()==1,"完成后新增唯一子女"): finish(); return
	var child: String=completed.family.children.keys()[0]; var person: Dictionary=completed.actors[child]
	check(completed.facts.has(child) and completed.facts[child].kind=="actor_creation" and completed.facts[child].source==action,"运行Actor与actor_creation Fact合法同ID，来源为完成收养")
	check(person.prototype_id=="adopt::actor.prototype" and not completed.actors.has(person.prototype_id) and not kernel.content().data.actors.has(child),"运行ID/目录原型明确分域")
	check(completed.wallets[person.wallet_id].balance==0 and completed.inventories[person.inventory_id].lots.is_empty() and person.wallet_id!=completed.actors[guardian].wallet_id,"动态子女有独立空私人资产")
	check(not completed.actions.has(action) and completed.facts[action].kind=="family_adoption" and completed.facts[action].success,"完成行动删除，家庭事实与创建事实分别保留")
	capture(kernel,"family_completed",[guardian,child])
	print("ACTUAL_DYNAMIC_ACTOR=",child," fact_id=",child," source=",action," prototype=",person.prototype_id," birth_tick=",person.birth_tick," created_tick=",person.created_tick)
	finish()

## 只声明本次真实来源短链，不声明native或保存验收。
func finish() -> void:
	print("M1_FAMILY_SOURCES checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
