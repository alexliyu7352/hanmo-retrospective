## 生活安排输入的真实控件门；替身仅记录提交，不伪造原生玩法、占用或保存成功。
extends SceneTree
const Controls=preload("res://ui/player_plan_controls.gd")
var checks: int=0
var failures: Array=[]

## 替身只有有界报价和身份，让用例观察是否错误带上许可或重放旧控件。
class KernelStub extends RefCounted:
	var revision: int=1
	var terms: Dictionary={"provider_actor_id":"actor.teacher","place_id":"place.school","role_id":"role.teacher","duration":3,"cost":90,"wage":0,"skill_id":"rites","target_level":1,"xp_limit":100,"challenge_mode":"automatic","qualification_key":"原教师资格"}
	## 同一字段同时作为原报价回执，不在替身里计算价格或批准动作。
	func preview(_kind: String, _params: Dictionary={}, _actor: String="") -> Dictionary: return {"ok":true,"terms":terms.duplicate(true)}
	## 真实UI Session用来源身份拒绝旧页；测试变化只模拟收到不同revision。
	func activity_identity() -> Dictionary: return {"actor_id":"actor.player","revision":revision}
	## 世界身份进入本次勾选草案，换世界或修订不能沿用旧勾选。
	func source_identity() -> Dictionary: return {"world_id":"ui-control-test","execution_epoch":1,"revision":revision,"tick":0}
	## 命令上下文只给有限提交所需身份，不提供任何世界实体查询。
	func command_context(_actor: String="") -> Dictionary: return {"ok":true,"actor_id":"actor.player","next_request_seq":1}

## 最小控件宿主保留Main的Session边界，提交仅进入测试记录。
class HostStub extends Control:
	var kernel=KernelStub.new()
	var _resident_mode: bool=true
	var _started: bool=true
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _scene_counter: int=1
	var _data: Dictionary={}
	var _message: String=""
	var requests: Array=[]
	## 本例明确使用新profile，旧profile仍由单独断言隐藏入口。
	func _player_plan_enabled() -> bool: return _resident_mode
	## 名称回退只是显示，不读取隐藏角色记录。
	func _name(value: Dictionary) -> String: return value.get("id","未知")
	## 拒绝文本直接保原代码，使断言不依赖中文词典。
	func _reason(code: String) -> String: return code
	## 当前测试使用真实Label承载文字，未知值不会转换为零。
	func _text(parent: Control, value: String, _size: int=16) -> Label:
		var label=Label.new(); label.text=value; parent.add_child(label); return label
	## 信号连接与产品按钮相同，保留callback以测试重绘后的旧输入。
	func _button(parent: Control, label: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var button=Button.new(); button.text=label; button.disabled=disabled; button.set_meta("command_kind",kind); button.set_meta("target_id",target); parent.add_child(button); button.pressed.connect(callback); return button
	## 只有已通过Session的命令会被记录；不推进世界或假装外部提交完成。
	func _resident_submit_frozen(command: Dictionary, _completed: Callable=Callable()) -> Dictionary:
		requests.append(command.duplicate(true)); return {"ok":true}
	## 测试拒绝路径不重建宿主；控件生命周期由具体用例明确驱动。
	func _render() -> void: pass

## 仅显式运行此脚本时测试；check-only不会建立输入会话。
func _initialize() -> void: call_deferred("run")

## 失败逐条保留，不用之后的成功覆盖同一用例。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures.append(label); printerr("FAIL ",label)

## 默认未授权、异步重绘保留显式勾选、换身份清许可、旧按钮去重均由真实控件检查。
func run() -> void:
	var host=HostStub.new(); root.add_child(host)
	var params: Dictionary={"context":{"place_id":"place.school","provider_actor_id":"actor.teacher","role_id":"role.teacher"},"skill_id":"rites","ticks":3,"challenge_mode":"automatic"}
	var box: Control=Controls.append(host,host,"course",params)
	await process_frame
	var consent: CheckBox=box.find_child("PlayerPlanPreauthorize",true,false)
	check(consent!=null and not consent.button_pressed,"unchecked_by_default")
	var add: Button=box.find_child("PlayerPlanAdd",true,false)
	add.pressed.emit(); add.pressed.emit()
	check(host.requests.size()==1 and host.requests[0].params.authorized==false and not host.requests[0].params.has("terms"),"unapproved_add_and_replay_no_double_submit")
	host.remove_child(box); box.queue_free(); box=Controls.append(host,host,"course",params); await process_frame
	consent=box.find_child("PlayerPlanPreauthorize",true,false); consent.button_pressed=true
	var stored: Dictionary=host.kernel.terms.duplicate(true)
	host.remove_child(box); box.queue_free(); box=Controls.append(host,host,"course",params); await process_frame
	consent=box.find_child("PlayerPlanPreauthorize",true,false)
	check(consent.button_pressed,"explicit_selection_survives_read_only_refresh")
	add=box.find_child("PlayerPlanAdd",true,false); add.pressed.emit()
	check(host.requests.size()==2 and host.requests[1].params.authorized and host.requests[1].params.terms==stored,"same_provider_price_params_reviewed")
	var old: Array=add.pressed.get_connections(); host.kernel.revision+=1
	host.remove_child(box); box.queue_free(); box=Controls.append(host,host,"course",params); await process_frame
	check(not box.find_child("PlayerPlanPreauthorize",true,false).button_pressed,"new_revision_defaults_unchecked")
	for connection in old:
		if connection.callable.is_valid(): connection.callable.call()
	check(host.requests.size()==2,"stale_page_cannot_authorize_or_submit")
	host.kernel.terms={}; host.remove_child(box); box.queue_free(); box=Controls.append(host,host,"course",params); await process_frame
	check(box.find_child("PlayerPlanPreauthorize",true,false).disabled,"unknown_terms_cannot_authorize")
	host._resident_mode=false; check(Controls.append(host,host,"course",params)==null,"old_profile_no_plan_entry")
	host.queue_free(); await process_frame
	print("PLAYER_PLAN_CONTROLS_CHECKS=",checks," FAILURES=",failures.size())
	print("PLAYER_PLAN_CONTROLS ","OK" if failures.is_empty() else JSON.stringify(failures)); quit(0 if failures.is_empty() else 1)
