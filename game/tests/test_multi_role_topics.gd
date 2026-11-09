## 正常prepare与真实Kernel上的多岗位纯查询合同；覆盖显式岗位对白，UI节点另行验证。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Topics=preload("res://core/interaction_topics.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")
const Events=preload("res://core/event_rules.gd")
var checks: int=0
var failures: int=0

## 等引擎类缓存就绪后读取显式夹具路径，不打开默认世界。
func _init() -> void:
	call_deferred("_run")

## 任何失败独立记录并令短runner非零退出。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)
	return value

## 使用正常开局及正式移动命令到店铺，不构造伪核心state。
func fresh(path: String):
	var kernel=Kernel.new()
	var started: Dictionary=kernel.new_game("scenario.two_stories",path)
	if not check(started.ok,"正式目录真实开局："+str(started)): return null
	var state: Dictionary=kernel.snapshot()
	var actor: String=state.player_actor_id
	var moved: Dictionary=kernel.submit({"kind":"move_local","params":{"place_id":"west.grain"},"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	if not check(moved.ok,"正式move_local到店铺："+str(moved)): return null
	return kernel

## 查找返回的人物组；缺项用空字典保留明确失败，不臆造主题。
func provider(rows: Array, actor_id: String) -> Dictionary:
	for row in rows:
		if row.provider_actor_id==actor_id: return row
	return {}

## 查找某岗位主题，供固定期望与真实can_begin逐项核验。
func topic(rows: Array, actor_id: String, role_id: String) -> Dictionary:
	for row in provider(rows,actor_id).get("topics",[]):
		if row.role_id==role_id: return row
	return {}

## 当前地点的每个岗位保留原预览首错；浏览完整输入和真实Kernel都不变。
func query_checked(kernel, place_id: String="west.grain") -> Array:
	var state: Dictionary=kernel.snapshot()
	var data: Dictionary=kernel.content().data
	var before_state: Dictionary=state.duplicate(true)
	var before_data: Dictionary=data.duplicate(true)
	var rows: Array=Topics.query(state,data,state.player_actor_id,place_id)
	for group in rows:
		for entry in group.topics:
			var checked: Dictionary=Dialogue.can_begin(state,data,state.player_actor_id,entry.role_id)
			check(entry.available==checked.ok and entry.code==("" if checked.ok else checked.code),"岗位可用性与现can_begin首错相同："+entry.role_id)
	check(state==before_state and data==before_data and kernel.snapshot()==before_state,"查询不改变任何state、目录、实例号或RNG")
	check(Topics.query(state,data,state.player_actor_id,place_id)==rows,"相同真实状态重复查询完全一致")
	return rows

## 原目录的唯一对白仍走原role/context，原服务数组完整返回。
func check_legacy(path: String) -> void:
	var kernel=fresh(path)
	if kernel==null: return
	var rows: Array=query_checked(kernel)
	check(rows==[
		{"provider_actor_id":"actor.apprentice","topics":[{"role_id":"role.clerk","dialogue_id":"dialog.clerk","services":["trade"],"available":true,"code":""}]},
		{"provider_actor_id":"actor.zheng","topics":[{"role_id":"role.merchant","dialogue_id":"dialog.shop","services":["information","trade"],"available":true,"code":""}]}],"旧唯一对白、岗位、服务与人物ID原样可用")
	check(Topics.query(kernel.snapshot(),kernel.content().data,"actor.missing","west.grain")==[],"未知请求人物没有主题")
	check(Topics.query(kernel.snapshot(),kernel.content().data,"actor.player","place.missing")==[],"未知地点没有主题")
	check(Topics.query(kernel.snapshot(),kernel.content().data,"actor.player","west.street")==[],"无岗位地点为空且不启动默认对白")

## 第一岗位无对白或禁用仍保留第二合法岗位，任职与在场变化用真实Kernel恢复门复核。
func check_multiple(path: String) -> void:
	var kernel=fresh(path)
	if kernel==null: return
	var baseline: Dictionary=kernel.snapshot()
	var rows: Array=query_checked(kernel)
	check(rows.size()==2,"同人多岗位归为一个人物组")
	var zheng: Dictionary=provider(rows,"actor.zheng")
	if not check(not zheng.is_empty() and zheng.topics.size()==2,"第一无对白岗位不去掉第二主题"): return
	check(zheng.topics[0]=={"role_id":"role.a_optional","dialogue_id":"","services":["information"],"available":false,"code":"NoDialogueForRole"},"第一岗位保留原NoDialogueForRole且不猜另一岗位对白")
	check(zheng.topics[1]=={"role_id":"role.merchant","dialogue_id":"dialog.shop","services":["information","trade"],"available":true,"code":""},"同人第二岗位仍有原合法对白")
	var state: Dictionary=kernel.snapshot()
	var data: Dictionary=kernel.content().data
	var ids: Array=data.roles.keys(); ids.reverse()
	var reversed_roles: Dictionary={}
	for id in ids: reversed_roles[id]=data.roles[id]
	data.roles=reversed_roles
	check(Topics.query(state,data,state.player_actor_id,"west.grain")==rows,"逆序定义枚举保持人物与主题ID排序")
	zheng.topics[0].services.append("invented")
	zheng.topics[1].dialogue_id="invented"
	check(data.roles["role.a_optional"].services==["information"] and provider(rows,"actor.apprentice").topics[0].services==["trade"],"返回服务与别的人物组无可变别名")
	check(topic(Topics.query(state,data,state.player_actor_id,"west.grain"),"actor.zheng","role.merchant").dialogue_id=="dialog.shop","修改输出不污染后续查询")
	var candidate: Dictionary=baseline.duplicate(true)
	candidate.roles["role.a_optional"].enabled=false
	if check(kernel.restore(candidate).ok,"第一岗位禁用候选通过真实全状态恢复门"):
		rows=query_checked(kernel)
		check(topic(rows,"actor.zheng","role.a_optional").code=="RoleNotAuthorized" and topic(rows,"actor.zheng","role.merchant").available,"第一岗位禁用仍不遮挡同人第二岗位")
	candidate=baseline.duplicate(true)
	candidate.actors["actor.zheng"].location.place_id="west.street"
	if check(kernel.restore(candidate).ok,"人员离岗候选通过真实全状态恢复门"):
		rows=query_checked(kernel)
		check(topic(rows,"actor.zheng","role.a_optional").code=="PersonAbsent" and topic(rows,"actor.zheng","role.merchant").code=="PersonAbsent","同人不在场时全部岗位如实不可用")
		check(topic(rows,"actor.apprentice","role.clerk").available,"缺席人物不遮挡另一在场人物")
	candidate=baseline.duplicate(true)
	candidate.roles["role.a_optional"].actor_id="actor.chen"
	if check(kernel.restore(candidate).ok,"真实岗位重任职候选通过全状态恢复门"):
		rows=query_checked(kernel)
		check(topic(rows,"actor.chen","role.a_optional").code=="PersonAbsent" and provider(rows,"actor.zheng").topics.size()==1,"按当前任职人分组，不沿作者初始人物去重")
		check(topic(rows,"actor.zheng","role.merchant").available,"第一岗位任职人不在场仍保留后续合法岗位")
	check(kernel.restore(baseline).ok,"恢复原真实世界以重查当前交互")
	rows=query_checked(kernel,"west.office")
	check(rows.size()==1 and topic(rows,"actor.chen","role.office").code=="NotAtPlace","仅列指定地点岗位并沿原规则诊断请求人不在场")
	check(not provider(rows,"actor.zheng").has("topics"),"其他地点岗位不进入当前地点人物清单")

## 同人两份合法对白通过原can_begin选择；岗位显式入口不得改写人物旧唯一对白。
func check_explicit(path: String) -> void:
	var kernel=fresh(path)
	if kernel==null: return
	var before: Dictionary=kernel.snapshot()
	var content: Dictionary=kernel.content()
	var rows: Array=query_checked(kernel)
	var first: Dictionary=topic(rows,"actor.zheng","role.a_optional")
	var second: Dictionary=topic(rows,"actor.zheng","role.merchant")
	check(first.get("available",false) and first.get("dialogue_id")=="dialog.optional","显式role对白优先于actor原对白")
	check(second.get("available",false) and second.get("dialogue_id")=="dialog.shop","同人原岗位继续使用旧actor对白")
	check(content.data.actors["actor.zheng"].dialogue_id=="dialog.shop" and kernel.content()==content,"新入口不改人物定义或目录")
	var disabled: Dictionary=before.duplicate(true)
	disabled.roles["role.a_optional"].enabled=false
	if check(kernel.restore(disabled).ok,"显式首岗位禁用候选通过Kernel恢复门"):
		rows=query_checked(kernel)
		check(topic(rows,"actor.zheng","role.a_optional").code=="RoleNotAuthorized" and topic(rows,"actor.zheng","role.merchant").available,"显式首岗位禁用仍不遮住第二合法对白")

## 显式岗位on_enter事件由真实移动排队；尾部reconcile不得按actor旧对白错误取消。
func check_explicit_event(path: String) -> void:
	var kernel=fresh(path)
	if kernel==null: return
	var state: Dictionary=kernel.snapshot()
	var key: String=state.player_actor_id+"|event.optional"
	check(state.events.get(key,{}).get("status")=="queued","真实move_local提交尾部仍保留显式岗位事件queued")
	var candidate: Dictionary=state.duplicate(true)
	var result: Dictionary=Events.reconcile(candidate,kernel.content().data)
	check(result.ok and candidate==state and candidate.events.get(key,{}).get("status")=="queued","再次原reconcile保持相同queued记录")
	var preview: Dictionary=kernel.preview("open_event",{"event_id":"event.optional"})
	check(preview.ok and preview.get("dialogue_id")=="dialog.optional","原open_event预览选显式岗位对白")
	check(kernel.snapshot()==state,"事件预览不修改真实世界或RNG")
	result=kernel.submit({"kind":"open_event","params":{"event_id":"event.optional"},"actor_id":state.player_actor_id,"request_seq":state.request_sequences[state.player_actor_id]+1,"expected_revision":state.revision})
	if check(result.ok,"正式open_event提交通过："+str(result)):
		var opened: Dictionary=kernel.snapshot()
		var session: Dictionary=opened.interactions.get(opened.player_actor_id,{})
		check(opened.events[key].status=="shown" and session.get("dialogue_id")=="dialog.optional" and session.get("context",{}).get("role_id")=="role.a_optional","实际事件shown与会话使用同一显式岗位context")
	check(kernel.content().data.actors["actor.zheng"].dialogue_id=="dialog.shop","事件链不改actor原唯一对白")

## 只执行正式fixture与本纯查询短门；不执行完整对白、存档或UI套件。
func _run() -> void:
	var folder: String=""
	var args: PackedStringArray=OS.get_cmdline_user_args()
	for argument in args:
		if argument.begins_with("--topics-fixtures="): folder=argument.trim_prefix("--topics-fixtures=")
	if check(not folder.is_empty(),"显式提供正常prepare输出目录"):
		if not "--role-event-only" in args:
			check_legacy(folder.path_join("legacy.hmcatalog.json"))
			check_multiple(folder.path_join("two-roles.hmcatalog.json"))
			check_explicit(folder.path_join("explicit-roles.hmcatalog.json"))
		check_explicit_event(folder.path_join("role-event.hmcatalog.json"))
	print("MULTI_ROLE_TOPICS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
