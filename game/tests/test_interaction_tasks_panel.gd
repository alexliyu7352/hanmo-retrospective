## 独立控件门验证同源提交、两步清算与失效按钮；不替代真实原生任务结算或GUI体验。
extends SceneTree
const TaskPanel=preload("res://ui/interaction_tasks_panel.gd")

## 测试内核只提供有限读回执，统计提交边界，不模拟世界结算。
class TestKernel extends RefCounted:
	var revision: int=7
	var terminal: bool=false
	var interaction: Dictionary={"phase":"choice","context":{"place_id":"office","provider_actor_id":"giver","role_id":"clerk"},"node_id":"choice"}
	## 来源版本改变时原控件必须失效。
	func source_identity() -> Dictionary: return {"world_id":"fixture","revision":revision}
	## 当前会话返回副本，控件不能通过展示引用篡改来源。
	func interaction_view() -> Dictionary: return interaction.duplicate(true)
	## 返回唯一当前观察者及请求序号。
	func command_context() -> Dictionary: return {"ok":true,"actor_id":"player","revision":revision,"next_request_seq":4}
	## 固定领域拒绝用于检查失败文字；成功报价显式携带版本。
	func preview(kind: String, _params: Dictionary={}, _actor: String="") -> Dictionary:
		if kind=="report_offer": return {"ok":false,"code":"InsufficientGoods","revision":revision}
		return {"ok":true,"duration":0,"revision":revision}
	## 本门只提供一份已受领任务以及原具名命令参数。
	func demand_view(_query: String, _args: Dictionary={}) -> Dictionary:
		var params: Dictionary={"offer_id":"grain","context":interaction.context.duplicate(true)}
		if terminal: return {"ok":true,"actor_id":"player","rows":[{"id":"grain","task_id":"task:grain","display_name":"粮食委托","status_text":"已经履约结算","source_text":"第2日得知 · 告示","objective_text":"交付粮食 × 3","report_text":"官署","result_text":"这份委托已经履约并完成结算。","actions":[]}]}
		return {"ok":true,"actor_id":"player","rows":[{"id":"grain","display_name":"粮食委托","status_text":"正在履行","source_text":"第2日得知 · 告示","objective_text":"交付粮食 × 3","report_text":"官署","deadline_tick":11,"actions":[{"command":"report_offer","text":"交付并回报","params":params,"available":false,"code":"InsufficientGoods"},{"command":"cancel_task","text":"放弃并按约清算","params":params,"available":true}]}]}

## 测试宿主只建立真实Godot控件并保留提交数，不载入第二套游戏规则。
class TestMain extends Control:
	var kernel=TestKernel.new()
	var _resident_mode: bool=true
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _scene_counter: int=1
	var _message: String=""
	var submissions: Array[Dictionary]=[]
	var paper: VBoxContainer
	## 保留与正式Main相同的公开会话身份范围。
	func _dialogue_identity(view: Dictionary) -> Dictionary: return view.duplicate(true)
	## 中文错误占位使断言能区分原码传递与静默禁用。
	func _reason(code: String) -> String: return "原因："+code
	## 建立实际Label用于检查可见条款。
	func _text(parent: Node, value: String, _size: int=16) -> Label:
		var label=Label.new(); label.text=value; parent.add_child(label); return label
	## 建立实际Button，保留脚本在正式UI中的连线方式。
	func _button(parent: Node, label: String, _kind: String, _target: String, callback: Callable, disabled: bool=false, _auto: bool=true) -> Button:
		var button=Button.new(); button.text=label; button.disabled=disabled; parent.add_child(button); button.pressed.connect(callback); return button
	## 页面重绘增加代次，旧按钮不能重放到新页面。
	func _render() -> void:
		_scene_counter+=1
		if is_instance_valid(paper): remove_child(paper); paper.queue_free()
		paper=VBoxContainer.new(); add_child(paper); TaskPanel.conversation(self,paper,kernel.interaction_view())
	## 唯一提交口仅记录请求，让本门检查没有自动接受或重复清算。
	func _resident_submit_frozen(request: Dictionary) -> Dictionary:
		submissions.append(request.duplicate(true)); return {"ok":true,"pending":true}

## 深度收集控件只用于断言，产品不依赖场景树搜索状态。
func _collect(node: Node, target: String) -> Array:
	var rows: Array=[]
	if node.is_class(target): rows.append(node)
	for child in node.get_children(): rows.append_array(_collect(child,target))
	return rows

## 业务按钮与折叠/翻页控件分开，只核真正会提交命令的入口。
func _actions(node: Node) -> Array:
	return _collect(node,"Button").filter(func(button): return button.has_meta("task_action"))

## 正常按钮、两步放弃、重复点击与旧会话请求都在同一次真实控件运行中核验。
func _initialize() -> void:
	call_deferred("_run")

## 等一帧完成控件挂接后再模拟操作，不用截图或文本替代输入边界。
func _run() -> void:
	var main=TestMain.new(); root.add_child(main); main._render(); await process_frame
	assert(_actions(main.paper).is_empty()); assert(main.submissions.is_empty())
	_collect(main.paper,"Button")[0].pressed.emit(); await process_frame
	var buttons: Array=_actions(main.paper); assert(buttons.size()==2)
	assert(buttons[0].disabled); assert(not buttons[1].disabled); assert(main.submissions.is_empty())
	var labels: Array=_collect(main.paper,"Label"); var failure_found: bool=false
	for label in labels:
		if "InsufficientGoods" in label.text: failure_found=true
	assert(failure_found)
	var old_button: Button=buttons[1]; var old_source: Dictionary=main.kernel.source_identity(); var old_dialogue: Dictionary=main.kernel.interaction_view(); var old_epoch: int=main._scene_counter; var action: Dictionary=old_button.get_meta("task_action")
	buttons[1].pressed.emit(); assert(main.submissions.is_empty()); await process_frame
	buttons=_actions(main.paper); assert(buttons[1].text=="确认放弃并清算")
	buttons[1].pressed.emit(); assert(main.submissions.size()==1); assert(main.submissions[0].kind=="cancel_task"); assert(main.submissions[0].expected_revision==7); assert(buttons[1].disabled)
	var repeated: Dictionary=TaskPanel._request(main,buttons[1],"grain",action,old_source,old_dialogue,main._scene_counter,"player"); assert(not repeated.ok); assert(main.submissions.size()==1)
	main._render(); await process_frame; buttons=_actions(main.paper); main.kernel.revision=8
	var stale: Dictionary=TaskPanel._request(main,buttons[1],"grain",action,old_source,old_dialogue,main._scene_counter,"player"); assert(not stale.ok and stale.code=="StalePreview"); assert(main.submissions.size()==1)
	main.kernel.revision=7; main.kernel.interaction.context.provider_actor_id="other"
	stale=TaskPanel._request(main,buttons[1],"grain",action,old_source,old_dialogue,main._scene_counter,"player"); assert(not stale.ok and stale.code=="StaleDialogueChoice"); assert(main.submissions.size()==1)
	assert(not is_instance_valid(old_button) or old_epoch!=main._scene_counter)
	main.kernel.interaction.context.provider_actor_id="giver"; main.kernel.terminal=true; main._render(); await process_frame
	var result_found: bool=false
	for label in _collect(main.paper,"Label"):
		if "这份委托已经履约并完成结算" in label.text: result_found=true
		assert("约定在半日" not in label.text and "受领办完后起算" not in label.text)
	assert(result_found and _actions(main.paper).is_empty())
	print("interaction_tasks_panel: real controls, failure feedback, two-step cancellation, duplicate/revision/dialogue guards passed")
	main.queue_free(); quit(0)
