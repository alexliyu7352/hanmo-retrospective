## 正式玩家援军的navigation入口门；实际Button及Kernel均为生产对象，宿主仅承接布局和通用提交。
extends "res://tests/test_siege_start_occupation.gd"
const Composer=preload("res://ui/screen_composer.gd")

## 局部页面宿主仅提供原控件语义和当前版本提交；不替代preview、不返回伪造成功或改_state。
class NavigationHost extends Control:
	var kernel
	var _data: Dictionary={}
	var _advancing: bool=false
	var submissions: Array=[]
	## 使用真实Button保存原语义标记并连接生产navigation传入的Callable。
	func _button(parent: Node, label: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var button=Button.new();button.text=label;button.set_meta("command_kind",kind);button.set_meta("target_id",target)
		button.disabled=disabled;button.pressed.connect(callback);parent.add_child(button);return button
	## 原导航只需Label承载文字，不在测试里另造规则提示或美术接受标准。
	func _text(parent: Node, words: String, size: int=16) -> Label:
		var label=Label.new();label.text=words;label.add_theme_font_size_override("font_size",size);parent.add_child(label);return label
	## 名称来自本次正式作者文本，名称不充当实体身份。
	func _name(record: Dictionary) -> String:
		return _data.texts.get(record.get("name_key",""),record.get("id",""))
	## 与Main通用路径同样在点击时取得现revision；旧固定revision请求另测真实Kernel拒绝。
	func _perform(kind: String, params: Dictionary={}) -> void:
		if _advancing: return
		var state: Dictionary=kernel.snapshot();var actor: String=state.player_actor_id
		var request: Dictionary={"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}
		submissions.append({"request":request.duplicate(true),"result":kernel.submit(request)})
	## 未被本门点击的相邻页面仅提供导航所需Callable，不模拟其完成。
	func _open_military_camp() -> void: pass
	## 未被本门点击的家事入口不执行任何替代行为。
	func _open_family() -> void: pass
	## 未被本门点击的课程入口不执行任何替代行为。
	func _open_courses() -> void: pass
	## 未被本门点击的武学入口不执行任何替代行为。
	func _open_growth() -> void: pass
	## 未被本门点击的庄务入口不执行任何替代行为。
	func _open_estate(_id: String) -> void: pass

## 使用真实NPC主攻开局；C始终是作者指定玩家，不靠restore或私有状态切换人物。
func entry_fixture() -> Dictionary:
	var f: Dictionary=assembled()
	if f.is_empty(): return {}
	var k=f.k;var params: Dictionary={"campaign_id":f.campaigns.a}
	if not check(k.snapshot().player_actor_id==C,"新作者锁真实玩家为C"): return {}
	if not check(k.preview("siege_start",params,A).ok,"NPC主攻真实开局预览"): return {}
	var started: Dictionary=send(k,"siege_start",params,A)
	if not started.ok: return {}
	f.siege_id=started.details.siege_id
	return f

## 实际调用原navigation而非重写按钮生成；反复渲染只读并允许直接观察Button元数据。
func navigation(f: Dictionary, advancing: bool=false) -> NavigationHost:
	var host=NavigationHost.new();host.kernel=f.k;host._data=f.k.content().data;host._advancing=advancing;root.add_child(host)
	var state: Dictionary=f.k.snapshot();var actor: Dictionary=state.actors[state.player_actor_id]
	Composer.navigation(host,host,f.k.place_sidebar(),host._data.places[actor.location.place_id],true)
	check(f.k.snapshot()==state,"真实navigation渲染不改世界或请求序号")
	return host

## 只收集实际原函数生成的命令Button，不把文本匹配或假控件当可玩入口。
func join_buttons(node: Node) -> Array:
	var result: Array=[]
	for child in node.get_children():
		if child is Button and child.get_meta("command_kind","")=="siege_join": result.append(child)
		result.append_array(join_buttons(child))
	return result

## 合法到城玩家必须能看见并点击加入；同请求再次提交只返回原成功回执。
func visible_entry() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var k=f.k;var params: Dictionary={"siege_id":f.siege_id,"campaign_id":f.campaigns.c};var before: Dictionary=k.snapshot()
	check(k.preview("siege_join",params).ok and SiegeWorld.for_actor(before,C).is_empty(),"真实玩家C可合法加入且尚无围城页面")
	var host= navigation(f);var buttons: Array=join_buttons(host)
	if not check(buttons.size()==1 and not buttons[0].disabled and buttons[0].is_visible_in_tree(),"合法玩家援军实际可见可用的唯一加入按钮"):
		host.free();return
	buttons[0].pressed.emit()
	if not check(host.submissions.size()==1 and host.submissions[0].result.ok,"点击原Button回调经真实Kernel成功加入"):
		host.free();return
	var state: Dictionary=k.snapshot();var submission: Dictionary=host.submissions[0];var army: String=state.campaigns[f.campaigns.c].army_id
	check(state.sieges[f.siege_id].armies.has(army) and SiegeWorld.for_actor(state,C)==f.siege_id,"成功后真实玩家进入既有围城")
	check(k.submit(submission.request)==submission.result and k.snapshot()==state,"同加入请求不二结算或分ID")
	check(state.wallets==before.wallets and state.inventories==before.inventories and state.military_units==before.military_units,"页面加入守恒兵员和资产")
	var after_host=navigation(f);check(join_buttons(after_host).is_empty(),"已加入者不再显示有效加入按钮");after_host.free();host.free()

## 忙碌时重新渲染无有效按钮，旧按钮点击也必须由当前真实ActorBusy拒绝并保持整态。
func busy_entry() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var host=navigation(f);var old: Array=join_buttons(host)
	check(old.size()==1,"忙碌前保留真实已绘制加入Button")
	if not send(f.k,"wait",{},C).ok: host.free();return
	var before: Dictionary=f.k.snapshot();var params: Dictionary={"siege_id":f.siege_id,"campaign_id":f.campaigns.c}
	check(f.k.preview("siege_join",params).get("code")=="ActorBusy","原忙碌规则是页面报价来源")
	var current=navigation(f);check(join_buttons(current).is_empty(),"忙碌玩家不显示有效加入入口")
	if old.size()==1:
		old[0].pressed.emit()
		check(host.submissions.size()==1 and host.submissions[0].result.get("code")=="ActorBusy" and f.k.snapshot()==before,"旧Button按当前context提交，真实busy拒绝且不写世界")
	current.free();host.free()

## 玩家真实加入后提交本军计划，旧按钮沿现有入口保留PlanAlreadySubmitted首错。
func submitted_plan_entry() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var host=navigation(f);var old: Array=join_buttons(host)
	check(old.size()==1,"交军议前保留真实已绘制加入Button")
	if not send(f.k,"siege_join",{"siege_id":f.siege_id,"campaign_id":f.campaigns.c},C).ok: host.free();return
	var orders: Dictionary={"base::unit.military.c":{"action":"wait","node_id":"approach"}}
	if not send(f.k,"siege_plan",{"siege_id":f.siege_id,"orders":orders},C).ok: host.free();return
	var before: Dictionary=f.k.snapshot();var params: Dictionary={"siege_id":f.siege_id,"campaign_id":f.campaigns.c}
	check(f.k.preview("siege_join",params).get("code")=="SiegePlanAlreadySubmitted","原已交军议首错不被UI换成加入成功")
	var current=navigation(f);check(join_buttons(current).is_empty(),"已交军议玩家不显示有效加入入口")
	if old.size()==1:
		old[0].pressed.emit()
		check(host.submissions.size()==1 and host.submissions[0].result.get("code")=="SiegePlanAlreadySubmitted" and f.k.snapshot()==before,"旧Button真实军议首错且原候选不变")
	current.free();host.free()

## 正在处理另一点击时只禁用原Button，不把UI忙标记写入军团或改变规则报价。
func advancing_entry() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var host=navigation(f,true);var buttons: Array=join_buttons(host)
	check(buttons.size()==1 and buttons[0].disabled,"宿主_advancing禁用原加入Button")
	check(f.k.preview("siege_join",{"siege_id":f.siege_id,"campaign_id":f.campaigns.c}).ok,"UI处理锁不伪装规则拒绝")
	host.free()

## 显式旧Kernel请求与通用按钮的当前context语义分开；另一真人推进revision后旧报价必须拒绝。
func stale_kernel_request() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var request: Dictionary=command(f.k,"siege_join",{"siege_id":f.siege_id,"campaign_id":f.campaigns.c},C)
	if not send(f.k,"wait",{},"base::actor.military.c.provider").ok: return
	var before: Dictionary=f.k.snapshot();var result: Dictionary=f.k.submit(request)
	check(not result.ok and result.get("code")=="StalePreview" and f.k.snapshot()==before,"旧显式expected_revision仍由真实Kernel拒绝")
	check(f.k.submit(request)==result and f.k.snapshot()==before,"旧显式请求重复失败不记序号或分ID")

## 独立正式作者只改玩家为原第三组织C；实际页面必须沿原非本方规则不提供加入按钮。
func foreign_entry() -> void:
	var f: Dictionary=entry_fixture()
	if f.is_empty(): return
	var before: Dictionary=f.k.snapshot();var params: Dictionary={"siege_id":f.siege_id,"campaign_id":f.campaigns.c}
	check(f.k.preview("siege_join",params).get("code")=="SiegeReinforcementSideUnauthorized","第三组织真实玩家保持原授权首错")
	var host=navigation(f)
	check(join_buttons(host).is_empty() and host.submissions.is_empty() and f.k.snapshot()==before,"非本方真实玩家页面无有效加入Button或隐式提交")
	host.free()

## 正式独立player=C锁，运行局部页面与真实公共命令，绝不宣称原生输入或全GUI验收。
func run() -> void:
	print("SIEGE_JOIN_ENTRY_STORAGE={file_backend:none,Codec:false,user_data_dir:",OS.get_user_data_dir(),"}")
	if not check(ProjectSettings.get_setting("application/config/siege_supply_isolated",false),"仅独立正式作者工程"): finish();return
	if not use_pin("siege-join-entry"): finish();return
	visible_entry();busy_entry();submitted_plan_entry();advancing_entry();stale_kernel_request()
	if use_pin("siege-join-entry-foreign"): foreign_entry()
	finish()

## 退出码只覆盖原navigation局部入口与真实Kernel，不把宿主装配声明为完整Main或视觉验收。
func finish() -> void:
	print("SIEGE_JOIN_ENTRY_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1)
