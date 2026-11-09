## 真Main/Screen/Host节点上的多岗位按钮短门；pressed进入原Kernel，不宣称OS视觉试玩。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Host=preload("res://ui/scene_host.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0

## 记录后完整委托真实Kernel，仅观察按钮实际提交的role上下文。
class RecordingKernel extends "res://core/world_kernel.gd":
	var requests: Array=[]
	var previews: Array=[]
	## 不替换preview或规则结果，记录与实际提交载荷采用同一原接口。
	func submit(command: Dictionary) -> Dictionary:
		requests.append(command.duplicate(true))
		return super.submit(command)
	## 先调用真正preview再记录其返回，确保禁用或换人拒绝来自当前规则而非测试伪值。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		var result: Dictionary=super.preview(kind,params,actor_id)
		previews.append({"kind":kind,"params":params.duplicate(true),"actor_id":actor_id,"result":result.duplicate(true)})
		return result

## 资源系统与SceneTree就绪后执行真实节点操作。
func _init() -> void:
	call_deferred("_run")

## 失败必须进入退出码，不以控件实例化成功冒充真实命令通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)
	return value

## 只等待真实布局和旧控件释放，不推进游戏时间或自动完成对白。
func settle() -> void:
	await process_frame
	await process_frame

## 根据生产按钮语义标记定位，不用坐标、标签或自行创建按钮替代实际入口。
func button(main: Control, role_id: String) -> Button:
	for node in main.find_children("*","Button",true,false):
		if node.get_meta("command_kind","")=="talk" and node.get_meta("target_id","")==role_id: return node
	return null

## 正常目录装载和move_local先进入店铺，再由真实Main.ready构造Screen与控件。
func mounted(path: String):
	var main=Main.new()
	main.kernel=RecordingKernel.new()
	var result: Dictionary=main.kernel.new_game("scenario.two_stories",path)
	if not check(result.ok,"菜单真实Kernel开局："+str(result)):
		main.free(); return null
	var state: Dictionary=main.kernel.snapshot()
	var actor: String=state.player_actor_id
	result=main.kernel.submit({"kind":"move_local","params":{"place_id":"west.grain"},"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	if not check(result.ok,"菜单正式move_local到店铺："+str(result)):
		main.free(); return null
	main._data=main.kernel.content().data
	main._started=true
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(main)
	return main

## 同人两个合法按钮均来自真实Screen，浏览及真实Host配置保持全部世界与RNG不变。
func check_explicit(path: String) -> void:
	var main=mounted(path)
	if main==null: return
	await settle()
	var before: Dictionary=main.kernel.snapshot()
	var content: Dictionary=main.kernel.content()
	var first: Button=button(main,"role.a_optional")
	var second: Button=button(main,"role.merchant")
	if not check(first!=null and second!=null and first!=second,"同人两个真实岗位按钮均存在"):
		main.queue_free(); await settle(); return
	check(first.is_visible_in_tree() and second.is_visible_in_tree() and not first.disabled and not second.disabled,"两个合法岗位按钮实际可见且可用")
	check(first.caption!=second.caption and "商会问事" in first.caption,"同人多岗位按钮保留各自岗位名称")
	var host=Host.new(); root.add_child(host)
	host.size=Vector2(1280,720)
	var configured: Dictionary=host.configure(before,main._data,"west.grain","interior",main.kernel.preview)
	check(configured.ok and configured.mode=="legacy" and host.is_inside_tree(),"真实Host识别本旧场所无模板，Screen沿合法legacy名单显示主题")
	seed(817); var expected_random: int=randi(); seed(817)
	for index in range(3):
		main._render()
		first=button(main,"role.a_optional"); second=button(main,"role.merchant")
		if first!=null: first.mouse_entered.emit()
		host.configure(before,main._data,"west.grain","interior",main.kernel.preview)
	check(randi()==expected_random,"浏览与悬停不消费全局RNG")
	check(main.kernel.snapshot()==before and main.kernel.content()==content,"浏览保持完整state、游戏RNG、实例号及目录不变")
	host.queue_free()
	var disabled: Dictionary=before.duplicate(true)
	disabled.roles["role.a_optional"].enabled=false
	check(main.kernel.restore(disabled).ok,"禁用首岗位候选经真实Kernel完整恢复")
	main._render(); await settle()
	first=button(main,"role.a_optional"); second=button(main,"role.merchant")
	if check(first!=null and second!=null and first.disabled and not second.disabled,"首岗位禁用按钮仍保留，第二合法按钮未被遮挡"):
		var request_count: int=main.kernel.requests.size()
		second.pressed.emit()
		await settle()
		var current: Dictionary=main.kernel.snapshot()
		var session: Dictionary=current.interactions.get(current.player_actor_id,{})
		check(main.kernel.requests.size()==request_count+1 and main.kernel.requests[-1].kind=="talk" and main.kernel.requests[-1].params=={"role_id":"role.merchant"},"实际第二按钮pressed只提交第二岗位的真实talk")
		check(session.get("dialogue_id")=="dialog.shop" and session.get("context",{}).get("role_id")=="role.merchant" and session.get("context",{}).get("provider_actor_id")=="actor.zheng","第二按钮进入旧对白及正确第二岗位context")
	check(main.kernel.restore(before).ok,"恢复浏览前真实世界以检查显式第一入口")
	main._render(); await settle()
	first=button(main,"role.a_optional")
	if check(first!=null and not first.disabled,"恢复后显式第一岗位按钮重新可用"):
		first.pressed.emit(); await settle()
		var current: Dictionary=main.kernel.snapshot()
		var session: Dictionary=current.interactions.get(current.player_actor_id,{})
		check(session.get("dialogue_id")=="dialog.optional" and session.get("context",{}).get("role_id")=="role.a_optional","显式第一按钮pressed进入岗位专属对白context")
	check(main.kernel.content().data.actors["actor.zheng"].dialogue_id=="dialog.shop","两次真实talk均不改actor旧对白")
	main.queue_free(); await settle()

## 首岗位没有对白仍绘为禁用按钮，第二岗位的生产pressed回调照常进入原Kernel。
func check_no_dialogue(path: String) -> void:
	var main=mounted(path)
	if main==null: return
	await settle()
	var first: Button=button(main,"role.a_optional")
	var second: Button=button(main,"role.merchant")
	if check(first!=null and second!=null and first.disabled and not second.disabled,"无对白第一按钮不可用且不遮住第二合法按钮"):
		second.pressed.emit(); await settle()
		var state: Dictionary=main.kernel.snapshot()
		var session: Dictionary=state.interactions.get(state.player_actor_id,{})
		check(session.get("dialogue_id")=="dialog.shop" and session.get("context",{}).get("role_id")=="role.merchant","无对白第一岗位存在时第二pressed仍选正确context")
	main.queue_free(); await settle()

## 沿当前真实状态提交一次本地移动，特意不调用Main重绘以保留屏幕上的旧按钮。
func move_without_render(main: Control, place_id: String) -> bool:
	var state: Dictionary=main.kernel.snapshot()
	var actor: String=state.player_actor_id
	var result: Dictionary=main.kernel.submit({"kind":"move_local","params":{"place_id":place_id},"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	return check(result.ok,"真实命令改变世界但不重绘："+place_id+" "+str(result))

## 真实pressed失败不能触达submit或改世界；资格失败必须能观察到当前preview的原错误码。
func refused_press(main: Control, target: Button, code: String="") -> void:
	var before: Dictionary=main.kernel.snapshot()
	var requests: int=main.kernel.requests.size()
	var previews: int=main.kernel.previews.size()
	var epoch: int=main._scene_counter
	target.pressed.emit()
	check(main.kernel.snapshot()==before and main.kernel.requests.size()==requests,"被拒按钮不改完整world/RNG/请求序号且不提交新命令")
	check(main._scene_counter==epoch,"失败不偷偷重绘或借用新页面代次")
	if code.is_empty():
		check(main.kernel.previews.size()==previews,"旧revision或旧页面在重新preview前即拒绝")
	else:
		check(main.kernel.previews.size()==previews+1 and main.kernel.previews[-1].result.get("code")==code,"当前资格失败沿真实preview返回："+code)

## 只测新增版本冻结边界；所有变化来自正常命令或完整restore，不重跑原22节点门。
func check_revision_boundaries(path: String) -> void:
	var main=mounted(path)
	if main==null: return
	await settle()
	var old: Button=button(main,"role.merchant")
	if not check(old!=null and not old.disabled,"取得实际生产按钮及其显示时版本绑定"):
		main.queue_free(); await settle(); return
	var initial: Dictionary=main.kernel.snapshot()
	var old_epoch: int=main._scene_counter
	if not move_without_render(main,"west.street"):
		main.queue_free(); await settle(); return
	check(main.kernel.snapshot().revision>initial.revision and main._scene_counter==old_epoch,"世界版本已变且旧画面仍未重绘")
	refused_press(main,old)
	if not move_without_render(main,"west.grain"):
		main.queue_free(); await settle(); return
	var baseline: Dictionary=main.kernel.snapshot()
	main._render()
	var same_revision_button: Button=button(main,"role.merchant")
	var same_revision_epoch: int=main._scene_counter
	main._render()
	if check(main.kernel.snapshot()==baseline and main._scene_counter>same_revision_epoch and is_instance_valid(same_revision_button),"同一world revision重绘仍令尚未释放的旧按钮页面代次失效"):
		refused_press(main,same_revision_button)
	await settle()
	var current: Button=button(main,"role.merchant")
	if not check(current!=null and not current.disabled,"新页面产生当前版本的真实可用按钮"):
		main.queue_free(); await settle(); return
	var disabled: Dictionary=baseline.duplicate(true)
	disabled.roles["role.merchant"].enabled=false
	if check(main.kernel.restore(disabled).ok,"真实完整restore接受当前岗位禁用状态"):
		check(main.kernel.snapshot().revision==baseline.revision,"资格测试保持已验证快照版本，不伪造新的revision")
		refused_press(main,current,"RoleNotAuthorized")
	var reassigned: Dictionary=baseline.duplicate(true)
	reassigned.roles["role.merchant"].actor_id="actor.chen"
	if check(main.kernel.restore(reassigned).ok,"真实完整restore接受岗位当前换为不在场人员"):
		check(main.kernel.snapshot().revision==baseline.revision,"换人候选保留合法快照原revision")
		refused_press(main,current,"PersonAbsent")
	check(main.kernel.restore(baseline).ok,"恢复合法当前世界后重新渲染")
	main._render(); await settle()
	current=button(main,"role.merchant")
	if check(current!=null and not current.disabled,"最终新按钮仍可发起真实talk"):
		var count: int=main.kernel.requests.size()
		current.pressed.emit(); await settle()
		var after: Dictionary=main.kernel.snapshot()
		var actor: String=after.player_actor_id
		var session: Dictionary=after.interactions.get(actor,{})
		check(main.kernel.requests.size()==count+1 and after.request_sequences[actor]==baseline.request_sequences[actor]+1,"新current只按一次pressed且只submit一次")
		check(main.kernel.requests[-1].expected_revision==baseline.revision and main.kernel.requests[-1].actor_id==actor and main.kernel.requests[-1].params=={"role_id":"role.merchant"},"提交保留该按钮真实actor/revision/role")
		check(session.get("dialogue_id")=="dialog.shop" and session.get("context",{}).get("role_id")=="role.merchant","最终talk进入正确第二岗位context")
	main.queue_free(); await settle()

## 只跑生产节点与pressed信号短门；不派发OS输入、不保存游戏、不执行无关套件。
func _run() -> void:
	var folder: String=""
	var args: PackedStringArray=OS.get_cmdline_user_args()
	for argument in args:
		if argument.begins_with("--topics-fixtures="): folder=argument.trim_prefix("--topics-fixtures=")
	check(Art.ready_result().ok,"真实表现注册表及资源可读取")
	if check(not folder.is_empty(),"显式提供正式prepare夹具") and failures==0:
		if "--revision-only" in args:
			await check_revision_boundaries(folder.path_join("explicit-roles.hmcatalog.json"))
		else:
			await check_explicit(folder.path_join("explicit-roles.hmcatalog.json"))
			await check_no_dialogue(folder.path_join("two-roles.hmcatalog.json"))
	print("MULTI_ROLE_MENU_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
