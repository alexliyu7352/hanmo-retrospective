## 冻结448的15个公共查询真实输出与原失败；只验证迁位、输入隔离和Backend接线，不重跑领域或存读矩阵。
extends "res://tests/test_legacy_backend_ownership.gd"
const Backend=preload("res://core/backends/legacy_world_backend.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const OLD_QUERY_SHA="81072bfdf91647b0ef9293343ea9f8fd9d71fb69464cc521be541fc47ebe360b"
const METHODS=["command_context","character_input","character_summary","scene_view","hud_view","map_view","trade_view","journal_text","explain","study_courses","study_etiquette","study_challenge","military_drill","interaction_view","trade_bargain"]

## 只统计查询是否借公开snapshot复制整世界，生产对象和全部规则保持原样。
class QueryKernel extends Kernel:
	var snapshot_calls: int=0
	## 原有读取仍返回真深副本，统计不替代输入隔离判断。
	func snapshot() -> Dictionary:snapshot_calls+=1;return super.snapshot()

## 15口接线哨兵只返回明确拒绝或标识字符串，绝不伪造游戏查询成功。
class QueryBoundary extends Backend:
	var seen: Array=[]
	## 保存普通参数副本以验证默认值和原引用输入内容，不持有Kernel或Callable。
	func rejected(method: String,args: Array) -> Dictionary:
		seen.append([method,args.duplicate(true)]);return {"ok":false,"code":"BackendQueryBoundary","method":method}
	## 当前命令上下文参数按原口转发。
	func command_context(actor_id: String="") -> Dictionary:return rejected("command_context",[actor_id])
	## 指定人物参数按原口转发。
	func character_input(actor_id: String) -> Dictionary:return rejected("character_input",[actor_id])
	## 成长页保留原默认页。
	func character_summary(page: String="skills") -> Dictionary:return rejected("character_summary",[page])
	## 场景位置与种类不由门面重写。
	func scene_view(place_id: String,view_kind: String) -> Dictionary:return rejected("scene_view",[place_id,view_kind])
	## HUD保留原显式或空个人战标识。
	func hud_view(personal_battle_id: String="") -> Dictionary:return rejected("hud_view",[personal_battle_id])
	## 地图无隐式新增参数。
	func map_view() -> Dictionary:return rejected("map_view",[])
	## 货单三项参数保持原顺序与空收据默认。
	func trade_view(shop_id: String,provider_actor_id: String,receipt_action_id: String="") -> Dictionary:return rejected("trade_view",[shop_id,provider_actor_id,receipt_action_id])
	## 手记保留String返回，标识只用于委托证明。
	func journal_text() -> String:seen.append(["journal_text",[]]);return "BackendQueryBoundary:journal_text"
	## 解释查询保留原observer缺省位置。
	func explain(query: Dictionary,observer_id: String="") -> Dictionary:return rejected("explain",[query,observer_id])
	## 课程保留actor/context/ticks原参数。
	func study_courses(actor_id: String,context: Dictionary,ticks: int=2) -> Dictionary:return rejected("study_courses",[actor_id,context,ticks])
	## 旧礼议入口仍存在。
	func study_etiquette(actor_id: String,action_id: String) -> Dictionary:return rejected("study_etiquette",[actor_id,action_id])
	## 普通课程题面不借迁位改名。
	func study_challenge(actor_id: String,action_id: String) -> Dictionary:return rejected("study_challenge",[actor_id,action_id])
	## 军阵题面保原本人和来源ID。
	func military_drill(actor_id: String,action_id: String) -> Dictionary:return rejected("military_drill",[actor_id,action_id])
	## 对白默认actor也在Backend处理。
	func interaction_view(actor_id: String="") -> Dictionary:return rejected("interaction_view",[actor_id])
	## 冻结真实议价view，新的来源查找口不在本片。
	func trade_bargain(actor_id: String,quote_id: String) -> Dictionary:return rejected("trade_bargain",[actor_id,quote_id])

## 原正式场景开局，不制作局部Dictionary冒称Schema接受的世界。
func query_world() -> QueryKernel:
	var k=QueryKernel.new();kernels.append(k)
	check(k.new_game(scenario_id,content_path).ok,"正式同作者查询世界创建")
	return k

## 15口均在有效世界读取；没有课程/军阵/议价实例时保其真实原失败，不造成功题面。
func standard_cases(k) -> Array:
	var actor: String=k._state.player_actor_id;var provider: String=k._state.roles["base::role.merchant"].actor_id
	return [["command_context",[]],["character_input",[actor]],["character_summary",[]],
		["scene_view",["base::west.street","city"]],["hud_view",[]],["map_view",[]],
		["trade_view",["base::shop.grain",provider]],["journal_text",[]],
		["explain",[{"kind":"offer","offer_id":"base::offer.grain"}]],
		["study_courses",[actor,Interaction.for_role(k._state,k._catalog.data,"base::role.office")]],
		["study_etiquette",[actor,"missing.action"]],["study_challenge",[actor,"missing.action"]],
		["military_drill",[actor,"missing.action"]],["interaction_view",[]],["trade_bargain",[actor,"missing.quote"]]]

## 递归破坏返回的首个嵌套容器；空Array也实际追加，避免clear空集合伪称深隔离。
func poison(value: Variant) -> void:
	if value is Dictionary:
		for item in value.values():
			if item is Dictionary or item is Array:poison(item);break
		value["test.query.poison"]=true
	elif value is Array:
		for item in value:
			if item is Dictionary or item is Array:poison(item);break
		value.append("test.query.poison")

## 每次记录真正公共口的原结果，并证原输入、完整三元组、世界及全局RNG均不变。
func query(k,method: String,args: Array,label: String) -> Variant:
	var before: Dictionary=k.save_context();var input: Array=args.duplicate(true)
	seed(4481500);var expected_random: int=randi();seed(4481500);k.snapshot_calls=0
	var output: Variant=k.callv(method,args)
	check(output is String if method=="journal_text" else output is Dictionary,"原返回类型 "+label)
	check(k.snapshot_calls==0 and args==input and k.save_context()==before and randi()==expected_random,"查询不改输入/三元组/RNG且不取snapshot "+label)
	var saved: Variant=output.duplicate(true) if output is Dictionary or output is Array else output
	poison(output)
	check(k.save_context()==before and k.callv(method,args)==saved,"返回嵌套修改不泄漏到已提交来源 "+label)
	rows.append({"label":label,"method":method,"args":input,"value":saved})
	return saved

## 公共动作只用于建立已知任务、当前对白和实际旅行这些原查询上下文。
func send(k,kind: String,params: Dictionary={}) -> bool:
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	return check(k.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}).ok,"真实查询前置 "+kind)

## 只测原本支持空世界的公开口，journal/character_summary/trade_view不新增空字典合同。
func cold() -> void:
	var k=QueryKernel.new();kernels.append(k)
	for spec in [["command_context",[]],["character_input",["missing"]],["hud_view",[]],["map_view",[]],["explain",[{"kind":"fact","fact_id":"missing"}]],["interaction_view",[]]]:
		query(k,spec[0],spec[1],"cold:"+spec[0])
	check(k.interaction_view()=={"phase":"scene"},"原空对白特殊形状保持")

## 有效正式世界的十五口、默认/显式actor、真实缺来源失败及既有知识边界。
func public_queries() -> void:
	var k=query_world();var actor: String=k._state.player_actor_id
	for spec in standard_cases(k):query(k,spec[0],spec[1],"initial:"+spec[0])
	check(k.command_context()==k.command_context(actor) and k.interaction_view()==k.interaction_view(actor),"原默认player与显式actor一致")
	query(k,"command_context",["missing"],"unknown_actor")
	query(k,"character_input",["missing"],"unknown_character")
	query(k,"character_summary",["inventory"],"inventory_page")
	query(k,"character_summary",["unknown_page"],"old_unknown_page_fallback")
	query(k,"hud_view",["missing"],"bad_personal_battle")
	query(k,"scene_view",["base::west.street","unknown_view"],"bad_scene_kind")
	query(k,"study_courses",[actor,{},1],"bad_course_request")
	query(k,"study_challenge",["missing","missing.action"],"unknown_study_actor")
	query(k,"explain",[{"kind":"command","request_seq":1},"missing"],"unknown_observer")
	query(k,"explain",[{"kind":"command","request_seq":[],"extra":[1]}],"bad_explanation_shape")
	if not send(k,"move_local",{"place_id":"base::west.office"}) or not send(k,"inspect",{"prop_id":"base::notice.grain"}):return
	query(k,"journal_text",[],"known_journal")
	query(k,"explain",[{"kind":"offer","offer_id":"base::offer.grain"}],"known_offer")
	query(k,"explain",[{"kind":"command","request_seq":1}],"own_command")
	check(k.explain({"kind":"offer","offer_id":"base::offer.grain"})==k.explain({"kind":"offer","offer_id":"base::offer.grain"},actor),"默认observer保持原本人")
	if not send(k,"talk",{"role_id":"base::role.office"}):return
	query(k,"interaction_view",[],"real_dialogue")
	query(k,"scene_view",["base::west.office","interior"],"real_interior")
	if not send(k,"end_interaction") or not send(k,"move_local",{"place_id":"base::west.street"}) or not send(k,"travel",{"destination":"base::village.east"}):return
	query(k,"map_view",[],"real_travel_overlay")
	query(k,"scene_view",["","world"],"real_world_scene")
	if not send(k,"advance"):return
	query(k,"hud_view",[],"advanced_hud")
	query(k,"map_view",[],"real_arrival_map")

## 有效真实世界上切换明确拒绝后端，旧Kernel即使未接线也不会靠坏输入触发脚本异常。
func delegation() -> void:
	var backend=Backend.new();var missing: bool=false
	for method in METHODS:
		if not check(backend.has_method(method),"Backend公开查询 "+method):missing=true
	if missing:return
	var k=query_world();var cases: Array=standard_cases(k);var probe=QueryBoundary.new();probe.publish(k._state,k._catalog,k._registry);k._backend=probe
	for spec in cases:
		var result: Variant=k.callv(spec[0],spec[1])
		check(result=="BackendQueryBoundary:journal_text" if spec[0]=="journal_text" else result.get("code")=="BackendQueryBoundary" and result.get("method")==spec[0],"原公共口实际委托 "+spec[0])
	var seen: Array=[]
	for row in probe.seen:seen.append(row[0])
	check(seen==METHODS,"十五个原口恰好各委托一次")
	check(probe.seen[0][1]==[""] and probe.seen[2][1]==["skills"] and probe.seen[4][1]==[""] and probe.seen[6][1][-1]=="" and probe.seen[8][1][-1]=="" and probe.seen[9][1][-1]==2 and probe.seen[13][1]==[""],"默认参数仍由同名Backend口收到原值")

## 原Kernel首次冻结typed输出，迁位后只读比较，不从两个新入口互比建立期望。
func oracle(args: PackedStringArray) -> void:
	for arg in args:
		if arg.begins_with("--record-oracle="):
			var path: String=arg.trim_prefix("--record-oracle=")
			if not check(failures==0 and FileAccess.get_file_as_string("res://core/world_kernel.gd").sha256_text()==OLD_QUERY_SHA and not FileAccess.file_exists(path),"只从无失败原448首次冻结"):return
			var file=FileAccess.open(path,FileAccess.WRITE)
			if not check(file!=null,"原查询oracle可写"):return
			file.store_var({"kernel_sha256":OLD_QUERY_SHA,"rows":rows},false);file.close();return
		if arg.begins_with("--oracle="):
			var file=FileAccess.open(arg.trim_prefix("--oracle="),FileAccess.READ);var old: Variant=file.get_var(false) if file!=null else null
			if not check(old is Dictionary and old.get("kernel_sha256")==OLD_QUERY_SHA and old.get("rows") is Array,"读取原15口oracle"):return
			check(rows==old.rows,"原15口完整结果/失败/参数逐项相等");return
	check(false,"明确指定原oracle读取或首次记录路径")

## 同现正式内容复用；不加载新规则、修改运行世界或重建完整stage。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if configure_input(args):
		cold();public_queries();oracle(args)
		if "--backend-required" in args:delegation()
	kernels.clear();print("LEGACY_BACKEND_QUERIES_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
