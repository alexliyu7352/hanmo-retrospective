## 本人军团的地图语义只读原active_army结果；军令办理仍经既有军旅页面及真实preview。
extends RefCounted
const META: StringName=&"map_company_draft"

## 原页面屏障读取当前控制人的一份摘要，不查询任意军团或改变世界位置。
static func prepare(host) -> bool:
	if host.get("_resident_mode")!=true or not host.kernel.has_method("demand_view"):return true
	var source: Dictionary={"identity":host.kernel.source_identity(),"actor_id":host.kernel.command_context().get("actor_id","")}
	var page: Dictionary=host.kernel.demand_view("map_company",{})
	if page.get("code")=="NativeAsyncRequired":return false
	if page.get("ok",true)!=false:
		var checked: Dictionary=_validate(page,source)
		if not checked.ok:page=checked
	host.set_meta(META,{"source":source,"page":page.duplicate(true)});return true

## 对有限文本保4096字节边界，不将缺统帅或错误身份解释为可下令。
static func _bounded(value: Variant,required: bool=true) -> bool:
	return value is String and value.to_utf8_buffer().size()<=4096 and (not required or not value.is_empty())

## 只有当前完整来源的一份真实军团，统领布尔值须与本人身份一致。
static func _validate(page: Dictionary,source: Dictionary) -> Dictionary:
	if page.get("ok")!=true or page.get("version")!=1 or page.get("identity")!=source.identity or page.get("observer_actor_id")!=source.actor_id:return {"ok":false,"code":"StaleMapCompany"}
	if not page.get("army") is Dictionary:return {"ok":false,"code":"InvalidMapCompany"}
	var army: Dictionary=page.army
	if army.is_empty():return {"ok":true}
	if not _bounded(army.get("army_id")) or not _bounded(army.get("commander_actor_id")) or not _bounded(army.get("commander_name"),false) or not army.get("commanded_by_observer") is bool or army.commanded_by_observer!=(army.commander_actor_id==source.actor_id) or not army.get("observed_tick") is int or army.observed_tick!=source.identity.get("tick"):return {"ok":false,"code":"InvalidMapCompany"}
	return {"ok":true}

## 同一stage绑定清理代次；旧地图退出不清掉新世界的当前摘要。
static func bind(host,stage: Control,canvas) -> void:
	var draft: Dictionary=_current(host)
	if draft.is_empty():return
	draft.scene_epoch=host._scene_counter;host.set_meta(META,draft)
	var cleanup: Callable=_exit.bind(weakref(host),draft.source,draft.scene_epoch)
	if not stage.tree_exiting.is_connected(cleanup):stage.tree_exiting.connect(cleanup)
	if draft.page.get("ok",false) and not draft.page.army.is_empty():canvas.observer_label="所领军团" if draft.page.army.commanded_by_observer else "所随军团"

## 场景释放只持弱宿主引用，旧退出事件不能复活军团草稿。
static func _exit(host_ref: WeakRef,source: Dictionary,scene_epoch: int) -> void:
	var host=host_ref.get_ref()
	if host==null:return
	var current: Dictionary=host.get_meta(META,{})
	if not host.journey_view.opened and current.get("source")==source and current.get("scene_epoch")==scene_epoch:host.remove_meta(META)

## 延迟导航不捕获宿主节点，真实军令权限继续交原军旅页处理。
static func _manage(host_ref: WeakRef,source: Dictionary,guard: Callable) -> void:
	var host=host_ref.get_ref()
	if host==null or not guard.call():return
	if _current(host).get("source")!=source:return
	host._open_military_camp()

## 不同来源的摘要不可读，未登记query的旧运行保持原人物地图。
static func _current(host) -> Dictionary:
	if host.get("_resident_mode")!=true:return {}
	var draft: Dictionary=host.get_meta(META,{})
	if draft.is_empty():return {}
	return draft if draft.get("source",{}).get("identity")==host.kernel.source_identity() else {}

## 地点详情展示本人的军旅身份；打开军旅页只复用原入口，所有命令由军事实时核验。
static func append(host,parent: Control,guard: Callable) -> void:
	var draft: Dictionary=_current(host)
	if draft.is_empty():return
	if not draft.page.get("ok",false):host._text(parent,"军旅位置资料暂不可用",16);return
	var army: Dictionary=draft.page.army
	if army.is_empty():return
	var label: Label=host._text(parent,("本人统领军团" if army.commanded_by_observer else "随军 · 统帅 "+(army.commander_name if not army.commander_name.is_empty() else "本军统帅"))+" · 半日%d所知"%army.observed_tick,17)
	label.set_meta("map_company_army_id",army.army_id)
	host._button(parent,"军旅事务" if army.commanded_by_observer else "查看军旅","map_company_manage","",_manage.bind(weakref(host),draft.source,guard))
