## 为同行和路遇测试准备独立内容；不修改正式作者源文件或玩家存档。
extends RefCounted
const Strict = preload("res://core/strict_json.gd")
const Kernel = preload("res://core/world_kernel.gd")
## 从真实目录复制测试定义，通过正式装载器开启同一世界。
static func fresh(schedule: bool = false, companion_role: String = "role.carter"):
	var header: Dictionary = Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/lived_city.hmcatalog.json")).value
	header.catalog.roles[companion_role].services.append("join_party")
	header.catalog.roles[companion_role].companion_cost_per_tick = 6
	if not "companions" in header.catalog.required_capabilities: header.catalog.required_capabilities.append("companions")
	if schedule: add_walker(header.catalog)
	header.catalog_sha256 = (JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String = "user://test-companions-"+str(schedule)+"-"+companion_role+".json"
	var file = FileAccess.open(path,FileAccess.WRITE)
	file.store_string(JSON.stringify(header)); file.close()
	var kernel = Kernel.new()
	var result: Dictionary = kernel.new_game("scenario.lived_city",path)
	if not result.ok: printerr("FIXTURE_LOAD: ",result)
	return kernel
## 增加会沿真实道路往返的旅人，静态岗位仍在原村口而不随身提供机构服务。
static func add_walker(data: Dictionary) -> void:
	var actor: Dictionary = data.actors["actor.carter"].duplicate(true)
	actor.id="actor.walker"; actor.initial_place="east.entrance"; actor.dialogue_id="dialog.walker"
	actor.road_dialogue_id="dialog.walker"
	actor.travel_plan={"destinations":["city.west","village.east"],"dwell_ticks":2,"start_tick":0}
	data.actors[actor.id]=actor
	data.roles["role.walker"]={"id":"role.walker","name_key":"name.role.carter","initial_actor":actor.id,"place_id":"east.entrance","organization_id":null,"services":["information"]}
	data.wallets["wallet.walker"]={"id":"wallet.walker","owner":{"kind":"actor","id":actor.id},"initial_balance":800}
	data.dialogues["dialog.walker"]={"id":"dialog.walker","context_role":"role.walker","entry":"hello","nodes":[{"id":"hello","kind":"say","speaker":"$provider","text_key":"dialog.carter.hello","next":"end"},{"id":"end","kind":"end"}]}
	if not "road_encounters" in data.required_capabilities: data.required_capabilities.append("road_encounters")
## 通过正常版本化入口提交命令，可指定NPC但不直接修改运行状态。
static func send(kernel, kind: String, params: Dictionary = {}, actor_id: String = "") -> Dictionary:
	var state: Dictionary = kernel.snapshot()
	if actor_id.is_empty(): actor_id=state.player_actor_id
	return kernel.submit({"kind":kind,"params":params,"actor_id":actor_id,"request_seq":state.request_sequences[actor_id]+1,"expected_revision":state.revision})
## 构造当前实际岗位的上下文，不给不存在或未授权的人物赋予权限。
static func context(kernel, role: String = "role.carter") -> Dictionary:
	return preload("res://core/interaction_rules.gd").for_role(kernel.snapshot(),kernel.content().data,role)
