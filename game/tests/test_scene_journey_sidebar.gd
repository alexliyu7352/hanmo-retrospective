## 真实Main普通场所与行旅右栏的旧控件oracle；世界前置经公共命令，合法restore边界单独注明。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const OLD_COMPOSER="439ef11373e1822fd59fe876450abc04152f5080b06a24c63f274d72f9e88bcf"
const OLD_MAP="7b5a62bfe8a5099501c29fc9df1896cecf494a557fb61bfc4f6b481a967522a4"
var checks: int=0
var failures: int=0
var fixtures: Dictionary={}
var rows: Array=[]
var snapshot_counts: Array=[]
var required: bool=false

## 只观测实际公开请求和快照；不覆盖报价、提交或世界推进的结果。
class ObservedKernel extends Kernel:
	var snapshots: int=0
	var submitted: Array=[]
	## 返回原真实深副本，额外计数只用于证明专用整态读取被消除。
	func snapshot() -> Dictionary:snapshots+=1;return super.snapshot()
	## 记录实际请求参数后执行原事务，按钮不得由成功替身通过。
	func submit(command: Dictionary) -> Dictionary:
		submitted.append(command.duplicate(true));return super.submit(command)

## 内容与UI依赖准备完毕后再挂真实节点。
func _init() -> void:call_deferred("run")

## 保留每条失败和进程退出码，脚本诊断不计作通过。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value

## 等实际控件布局和旧页面释放，不推进游戏半日。
func settle() -> void:
	await process_frame;await process_frame;await process_frame

## 真实Kernel与原Art初始化后交真实Main；不请求存储、导入、自动循环或换一套屏幕。
func opened(variant: String="road"):
	var k=ObservedKernel.new()
	if not check(k.new_game("base::scenario.alpha_life",fixtures.variants[variant].catalog).ok,"正式作者开局 "+variant):return null
	var art: Dictionary=Art.prepare_content(k.content())
	if not check(art.ok,"正式目录tiny表现准备"):return null
	Art.commit_content(art)
	var m=Main.new();m.kernel=k;m._data=k.content().data;m._started=true
	m.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT);root.add_child(m);await settle()
	return m

## 每步前置只用当前真实人物序号/版本，NPC旅行也走正式公共命令。
func send(m,kind: String,params: Dictionary={},actor_id: String="") -> bool:
	var state: Dictionary=m.kernel.snapshot();var actor: String=state.player_actor_id if actor_id.is_empty() else actor_id
	var result: Dictionary=m.kernel.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	return check(result.ok,"真实前置 "+kind+" "+str(result))

## 按原语义标记查实际按钮，不按文字臆造入口。
func button(node: Node,kind: String,target: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target:return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null:return found
	return null

## 从真实控件按树序采文字、可用性和目标，忽略自动节点名及纯美术时间，不忽略隐藏菜单状态。
func controls(node: Node,result: Array) -> void:
	if node is Label:result.append(["label",node.text,node.visible])
	elif node is Button:
		result.append(["button",node.text,node.get_meta("command_kind",""),node.get_meta("target_id",""),node.disabled,node.tooltip_text,node.visible])
	if node.name in ["ScenePersonPortrait","EncounterPortrait"] and node.has_method("describe"):
		var descriptor: Dictionary=node.describe();result.append(["portrait",str(node.name),descriptor.get("actor_id",""),descriptor.get("view_kind","")])
	for child in node.get_children():controls(child,result)

## 每次原Main重绘只读全世界/目录/注册与RNG；新口返回嵌套容器也不可写回来源。
func capture(m,label: String) -> void:
	var before: Dictionary=m.kernel.save_context();m.kernel.snapshots=0
	seed(986500);var expected: int=randi();seed(986500)
	m._render();await settle()
	check(m.kernel.save_context()==before and randi()==expected,"真实Main重绘无世界/输入/RNG副作用 "+label)
	var rendered: Array=[];controls(m.page,rendered)
	rows.append({"label":label,"controls":rendered});snapshot_counts.append(m.kernel.snapshots)
	if required:
		var method: String="journey_sidebar" if m.journey_view.opened or m.kernel._state.actors[m.kernel._state.player_actor_id].location.kind in ["OnRoute","Wilderness"] else "place_sidebar"
		if check(m.kernel.has_method(method),"完整用途口存在 "+method):
			var before_reads: int=m.kernel.snapshots
			var view: Dictionary=m.kernel.call(method);var expected_view: Dictionary=view.duplicate(true)
			check(view.ok and not view.has("actors") and not view.has("actions") and not view.has("facts") and not view.has("inventories"),"有限用途结果不返回权威容器 "+label)
			view.location["test_only"]=true
			if view.has("people") and not view.people.is_empty():view.people[0].topics.clear()
			if view.has("companions"):view.companions.clear()
			check(m.kernel.save_context()==before and m.kernel.call(method)==expected_view,"用途返回值深隔离 "+label)
			check(m.kernel.snapshots==before_reads,"有限查询不借公开snapshot转接 "+label)

## 真实第二岗位、无对白首岗位、合法禁用恢复、人员离场及本人queued事件保原顺序。
func places() -> void:
	var m=await opened()
	if m==null:return
	if not send(m,"move_local",{"place_id":"base::west.grain"}):m.queue_free();await settle();return
	await capture(m,"grain_multi_role")
	var invalid: Button=button(m,"talk","base::role.a_optional");var merchant: Button=button(m,"talk","base::role.merchant")
	check(invalid!=null and invalid.disabled and merchant!=null and not merchant.disabled,"无对白首岗位保禁用行且第二合法岗位仍可点击")
	check(button(m,"open_event","base::event.optional")!=null,"真实move_local产生本人queued事件入口")
	var baseline: Dictionary=m.kernel.snapshot();var count: int=m.kernel.submitted.size()
	merchant.pressed.emit();var after: Dictionary=m.kernel.snapshot()
	check(after.interactions[after.player_actor_id].context.role_id=="base::role.merchant","真实第二岗位pressed建立正确context")
	merchant.pressed.emit()
	check(m.kernel.submitted.size()==count+1 and m.kernel.snapshot()==after,"旧topic按钮不借新scene_epoch/revision再次提交")
	await capture(m,"real_shop_dialogue")
	var disabled: Dictionary=baseline.duplicate(true);disabled.roles["base::role.a_optional"].enabled=false
	check(m.kernel.restore(disabled).ok,"明确合法restore边界：禁用首岗位，不冒充任免命令")
	await capture(m,"disabled_first_role")
	check(m.kernel.restore(baseline).ok,"返回原真实店铺保存点")
	if send(m,"move_local",{"place_id":"base::west.street"},"base::actor.zheng"):
		await capture(m,"provider_left")
		check(button(m,"talk","base::role.merchant")==null and button(m,"talk","base::role.clerk")!=null,"离场人物不因作者岗位在此而显示，其他在场人仍可见")
	check(m.kernel.restore(baseline).ok,"恢复原真实岗位位置")
	m._advancing=true;await capture(m,"local_busy");m._advancing=false
	if send(m,"move_local",{"place_id":"base::west.street"}):await capture(m,"street_navigation")
	m.queue_free();await settle()

## 用原酒肆公开短工取得同行旅费，再经原对白真实建立party。
func companions(m) -> bool:
	if not send(m,"move_local",{"place_id":"base::west.tavern"}):return false
	var context: Dictionary=Interaction.for_role(m.kernel._state,m._data,"base::role.host")
	if not send(m,"service",{"activity":"work","context":context}) or not send(m,"advance"):return false
	for request in [["talk",{"role_id":"base::role.carter"}],["dialogue_next",{}],["dialogue_choose",{"option_id":"together"}],["dialogue_next",{}],["dialogue_confirm",{}],["end_interaction",{}],["move_local",{"place_id":"base::west.street"}]]:
		if not send(m,request[0],request[1]):return false
	return check(not m.kernel._state.parties.is_empty(),"原对白确认实际建立同行")

## 真实同时对向旅行产生Contact/encounter；路遇前后右栏短路、费用和一般按钮点击版本保持。
func journey() -> void:
	var m=await opened("companions")
	if m==null:return
	if not companions(m):m.queue_free();await settle();return
	m.journey_view.opened=true;await capture(m,"companions_idle")
	m.journey_view.selected="base::village.east";await capture(m,"companions_quote")
	var confirm: Button=button(m,"map_confirm");var rendered_revision: int=m.kernel._state.revision
	if not check(confirm!=null and not confirm.disabled,"原地图确认按钮可用"):m.queue_free();await settle();return
	if not send(m,"travel",{"destination":"base::city.west"},"base::actor.walker"):m.queue_free();await settle();return
	var current_revision: int=m.kernel._state.revision;confirm.pressed.emit()
	check(current_revision>rendered_revision and m.kernel.submitted.back().expected_revision==current_revision and m.kernel._state.actions[m.kernel._state.actors[m.kernel._state.player_actor_id].action_id].kind=="travel","普通旧绘制map按钮按点击时当前context提交，不强加旧revision必拒")
	await capture(m,"real_road_departure")
	if not send(m,"pause_travel"):m.queue_free();await settle();return
	await capture(m,"paused_road")
	if not send(m,"resume_travel") or not send(m,"advance"):m.queue_free();await settle();return
	var meeting: Dictionary=Encounters.pending_for(m.kernel._state,m.kernel._state.player_actor_id)
	if not check(not meeting.is_empty(),"正式双向旅行实际产生待处理会面"):m.queue_free();await settle();return
	await capture(m,"real_encounter")
	check(button(m,"dismiss_companion","base::actor.carter")==null and button(m,"map_confirm")==null,"路遇早退不继续渲染同行与通常行程控制")
	var talk: Button=button(m,"encounter_talk",meeting.id)
	if check(talk!=null and not talk.disabled,"原会面交谈按钮可达"):
		talk.pressed.emit();await capture(m,"real_road_dialogue")
	if send(m,"end_interaction") and send(m,"encounter_leave",{"encounter_id":meeting.id}):
		await capture(m,"after_encounter_leave")
		if send(m,"advance"):await capture(m,"arrived_with_companion")
	m.queue_free();await settle()

## 当前正式地形表的真实原野落点；不手改location/action来获得停留画面。
func terrain() -> void:
	var m=await opened("terrain")
	if m==null:return
	if not send(m,"move_local",{"place_id":"base::west.tavern"}):m.queue_free();await settle();return
	if not send(m,"service",{"activity":"work","context":Interaction.for_role(m.kernel._state,m._data,"base::role.host")}) or not send(m,"advance") or not send(m,"move_local",{"place_id":"base::west.street"}):m.queue_free();await settle();return
	var origin: Array=m._data.settlements["base::city.west"].map_xy;var point: Array=[]
	for delta in [[50,0],[40,20],[0,50],[-40,20]]:
		var candidate: Array=[origin[0]+delta[0],origin[1]+delta[1]]
		if m.kernel.preview("travel",{"destination":candidate}).ok:point=candidate;break
	if not check(not point.is_empty(),"原真实地形预览找到近处合法原野点"):m.queue_free();await settle();return
	m.journey_view.opened=true;m.journey_view.selected_point=point;await capture(m,"terrain_quote")
	var confirm: Button=button(m,"map_confirm")
	if not check(confirm!=null and not confirm.disabled,"原野实际确认按钮存在"):m.queue_free();await settle();return
	confirm.pressed.emit();await capture(m,"terrain_moving")
	for _half in range(20):
		if m.kernel._state.actors[m.kernel._state.player_actor_id].action_id.is_empty():break
		if not send(m,"advance"):break
	check(m.kernel._state.actors[m.kernel._state.player_actor_id].action_id.is_empty() and m.kernel._state.actors[m.kernel._state.player_actor_id].location.kind=="Wilderness","真实到原野后行动结束仍保原野停留")
	await capture(m,"wilderness_stopped");m.queue_free();await settle()

## 本人实军与他人实军分别由正式任官/受令/编组产生，地图不能误展示敌军库存。
func logistics(label: String) -> void:
	var m=await opened()
	if m==null:return
	var actor: String="base::actor.military."+label+".commander"
	var context: Dictionary=Interaction.for_role(m.kernel._state,m._data,"base::role.military."+label)
	if not send(m,"office_accept",{"office_id":"base::office.military."+label,"context":context},actor) or not send(m,"advance"):m.queue_free();await settle();return
	if not send(m,"campaign_accept",{"template_id":"base::order.military."+label,"context":context},actor):m.queue_free();await settle();return
	var campaign: String=m.kernel._state.campaigns.keys()[0]
	if not send(m,"army_form",{"order_id":campaign,"unit_ids":["base::unit.military."+label],"food_quantity":0,"money":10},actor) or not send(m,"advance"):m.queue_free();await settle();return
	m.journey_view.opened=true;await capture(m,"army_"+label+"_logistics")
	check((m.find_child("ArmyLogistics",true,false)!=null)==(label=="a"),"仅本人实际统军显示原供粮，不泄露他军 "+label)
	m.queue_free();await settle()

## 已提交真实前置的复制输入只做未读字段哨兵；不将这些缺字段副本提交或恢复到Kernel。
func short_circuits() -> void:
	var old_screen=load("res://tests/sidebar_short_old/screen_composer.gd")
	var old_map=load("res://tests/sidebar_short_old/world_map_screen.gd")
	var source=load("res://core/views/scene_journey_sidebar.gd")
	check(FileAccess.get_file_as_string("res://tests/sidebar_short_old/screen_composer.gd").sha256_text()==OLD_COMPOSER,"原inhabitants源码固定9865")
	var map_text: String=FileAccess.get_file_as_string("res://tests/sidebar_short_old/world_map_screen.gd").replace("res://tests/sidebar_short_old/journey_people.gd","res://ui/journey_people.gd")
	check(map_text.sha256_text()==OLD_MAP and FileAccess.get_file_as_string("res://tests/sidebar_short_old/journey_people.gd").sha256_text()=="59a0e7835fc91d0e1b9d5035527948e245215b3616b73dd994113569ea277004","原populate与原People仅重定向已冻结依赖路径")
	var m=await opened()
	if m==null:return
	if not send(m,"move_local",{"place_id":"base::west.grain"}) or not send(m,"talk",{"role_id":"base::role.merchant"}):m.queue_free();await settle();return
	var before: Dictionary=m.kernel.save_context();var state: Dictionary=m.kernel.snapshot();var data: Dictionary=m._data.duplicate(true)
	var actual_view: Dictionary=m.kernel.interaction_view();var actor: Dictionary=state.actors[state.player_actor_id]
	for key in ["roles","estates","campaigns","armies","events"]:state[key]=null
	for key in ["roles","garrisons","estates"]:data[key]=null
	var box=VBoxContainer.new();m.add_child(box)
	old_screen.inhabitants(m,box,state,actor,m._data.places[actor.location.place_id],actual_view)
	check(box.get_child_count()==0,"原非scene inhabitants不读名单/导航来源")
	var view: Variant=source.place(state,data)
	check(view is Dictionary and view.get("ok",false) and not view.get("dialogue_speaker",{}).is_empty() and view.get("people",[]).is_empty(),"新place对白不读未用角色/庄务/军务/事件容器")
	check(m.kernel.save_context()==before,"纯对白哨兵未写回真实Kernel")
	box.queue_free();m.queue_free();await settle()
	m=await opened("companions")
	if m==null:return
	if not companions(m) or not send(m,"travel",{"destination":"base::city.west"},"base::actor.walker") or not send(m,"travel",{"destination":"base::village.east"}) or not send(m,"advance"):m.queue_free();await settle();return
	state=m.kernel.snapshot();before=m.kernel.save_context()
	if not check(not Encounters.pending_for(state,state.player_actor_id).is_empty(),"短路前置是真实公共命令产生的待处理会面"):m.queue_free();await settle();return
	actor=state.actors[state.player_actor_id]
	actor.location.erase("edge_id");actor.location.erase("progress")
	state.actions[actor.action_id].erase("params");state.actions[actor.action_id].erase("remaining_ticks")
	state.parties=null;state.armies=null
	box=VBoxContainer.new();m.add_child(box)
	old_map.populate(m,box,state,actor,{})
	check(button(box,"encounter_leave",Encounters.pending_for(state,state.player_actor_id).id)!=null and button(box,"map_close")==null,"原pending会面在路线属性/同行/军粮前返回")
	view=source.journey(state,m._data)
	check(view is Dictionary and view.get("ok",false) and not view.get("encounter",{}).is_empty() and view.get("travel",{})=={"active":true},"新pending会面只取travel布尔，不读params/edge/progress/余程")
	check(m.kernel.save_context()==before,"纯会面哨兵未写回真实Kernel")
	box.queue_free();m.queue_free();await settle()
	m=await opened()
	if m==null:return
	state=m.kernel.snapshot();before=m.kernel.save_context();state.interactions=null
	box=VBoxContainer.new();m.add_child(box)
	old_map.populate(m,box,state,state.actors[state.player_actor_id],{})
	check(button(box,"map_close")!=null,"原无会面地图不读Dialogue来源")
	view=source.journey(state,m._data)
	check(view is Dictionary and view.get("ok",false) and view.get("dialogue_speaker",{}).is_empty(),"新无会面地图不因common辅助提前读Dialogue")
	check(m.kernel.save_context()==before,"纯无会面哨兵未写回真实Kernel")
	box.queue_free();m.queue_free();await settle()

## 原生产UI字节仅首次生成oracle；新UI须逐控件相等，快照仅按预期减少而不弱化文本比较。
func oracle(args: PackedStringArray) -> void:
	for arg in args:
		if arg.begins_with("--record-oracle="):
			var path: String=arg.trim_prefix("--record-oracle=")
			if not check(failures==0 and FileAccess.get_file_as_string("res://ui/screen_composer.gd").sha256_text()==OLD_COMPOSER and FileAccess.get_file_as_string("res://ui/world_map_screen.gd").sha256_text()==OLD_MAP and not FileAccess.file_exists(path),"无失败原9865真实控件首次冻结"):return
			var file=FileAccess.open(path,FileAccess.WRITE);file.store_var({"composer_sha256":OLD_COMPOSER,"map_sha256":OLD_MAP,"rows":rows,"snapshots":snapshot_counts},false);file.close();return
		if arg.begins_with("--oracle="):
			var file=FileAccess.open(arg.trim_prefix("--oracle="),FileAccess.READ);var old: Variant=file.get_var(false) if file!=null else null
			if not check(old is Dictionary and old.get("composer_sha256")==OLD_COMPOSER and old.get("map_sha256")==OLD_MAP,"读取原侧栏oracle身份"):return
			check(rows.size()==old.rows.size(),"原场景控件记录数一致")
			var reduced: bool=false
			for index in mini(rows.size(),old.rows.size()):
				check(rows[index]==old.rows[index],"原控件/文字/顺序/禁用/tooltip一致 "+rows[index].label)
				check(snapshot_counts[index]<=old.snapshots[index],"快照不增加 "+rows[index].label)
				if snapshot_counts[index]<old.snapshots[index]:reduced=true
				print("SIDEBAR_SNAPSHOTS=",JSON.stringify({"case":rows[index].label,"old":old.snapshots[index],"current":snapshot_counts[index]}))
			if required:check(reduced,"本片实际专用人物/右栏快照减少")
			return
	check(false,"显式指定原oracle记录或对照路径")

## 原输入与三个作者变体独立pin，不靠安装索引顺序选择正式目录。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args();required="--require-query" in args
	for arg in args:
		if arg.begins_with("--fixture="):fixtures=JSON.parse_string(FileAccess.get_file_as_string(arg.trim_prefix("--fixture=")))
	if not check(fixtures.has("variants"),"明确提供本片正式显式目录输入"):quit(1);return
	root.size=Vector2i(1280,720)
	print("SIDEBAR_STORAGE={requests:none,user_data_dir:",OS.get_user_data_dir(),",profile:scene-journey-sidebar}")
	if "--short-circuit-only" in args:
		await short_circuits()
		print("SIDEBAR_SHORT_CIRCUIT_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1);return
	await places();await journey();await terrain();await logistics("a");await logistics("b");oracle(args)
	print("SCENE_JOURNEY_SIDEBAR_CHECKS=",checks," FAILURES=",failures," CASES=",rows.size());quit(0 if failures==0 else 1)
