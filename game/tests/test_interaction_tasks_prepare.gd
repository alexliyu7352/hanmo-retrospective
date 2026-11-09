## 只测纯读准备门，不重跑既有控件和原生业务门，不模拟提交或世界推进。
extends SceneTree
const TaskPanel=preload("res://ui/interaction_tasks_panel.gd")

## 假后端记录有限读取请求，支持真正异步尚未就绪与普通领域拒绝两种结果。
class ReadKernel extends RefCounted:
	var revision: int=1
	var page_ready: bool=false
	var quotes_ready: bool=false
	var queries: Array=[]
	var previews: Array=[]
	## 返回版本身份，便于核对旧选择不能跨源沿用。
	func source_identity() -> Dictionary: return {"world_id":"read_fixture","revision":revision}
	## 页面尚未返回时不能提前读取不存在的actions。
	func demand_view(query: String, args: Dictionary={}) -> Dictionary:
		queries.append({"query":query,"args":args.duplicate(true)})
		if not page_ready: return {"ok":false,"code":"NativeAsyncRequired"}
		return {"ok":true,"actor_id":"player","rows":[{"actions":[{"command":"accept_offer","params":{"offer_id":"first"}},{"command":"report_offer","params":{"offer_id":"second"},"available":false}]}]}
	## 就绪的规则拒绝不阻塞绘制，按钮需要显示原失败原因。
	func preview(command: String, params: Dictionary={}, actor: String="") -> Dictionary:
		previews.append({"command":command,"params":params.duplicate(true),"actor":actor})
		return {"ok":false,"code":"InsufficientGoods" if quotes_ready else "NativeAsyncRequired"}

## 宿主只有本地显示选择与会话身份；没有提交入口可供准备门误用。
class ReadHost extends RefCounted:
	var kernel=ReadKernel.new()
	var _resident_mode: bool=true
	## 会话对照保留来源人及当前继续点。
	func _dialogue_identity(view: Dictionary) -> Dictionary: return view.duplicate(true)

## 检查延迟页读、折叠省读、全部按钮预读、普通失败可见和跨会话选择失效。
func _initialize() -> void:
	var host=ReadHost.new(); var scene: Dictionary={"phase":"scene"}
	assert(TaskPanel.prepare(host,scene)); assert(host.kernel.queries.is_empty())
	var interaction: Dictionary={"phase":"choice","context":{"provider_actor_id":"giver"},"node_id":"root"}
	assert(not TaskPanel.prepare(host,interaction)); assert(host.kernel.previews.is_empty())
	host.kernel.page_ready=true
	assert(TaskPanel.prepare(host,interaction)); assert(host.kernel.previews.is_empty())
	var draft: Dictionary=host.get_meta("interaction_tasks_draft").duplicate(true); draft.expanded=true; draft.cursor=7; host.set_meta("interaction_tasks_draft",draft)
	assert(not TaskPanel.prepare(host,interaction)); assert(host.kernel.queries.back().args.cursor==7); assert(host.kernel.previews.size()==2)
	assert(host.kernel.previews[0].params.offer_id=="first" and host.kernel.previews[1].params.offer_id=="second")
	host.kernel.quotes_ready=true
	assert(TaskPanel.prepare(host,interaction)); assert(host.kernel.previews.size()==4)
	interaction.context.provider_actor_id="other"
	assert(TaskPanel.prepare(host,interaction)); assert(host.kernel.queries.back().args.cursor==0); assert(host.kernel.previews.size()==4)
	var count: int=host.kernel.queries.size(); interaction.phase="waiting"
	assert(TaskPanel.prepare(host,interaction)); assert(host.kernel.queries.size()==count)
	print("interaction_tasks_prepare: bounded page demand, expanded quote prefetch, pending gate and visible rule failures passed")
	quit(0)
