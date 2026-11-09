## 生命终点只呈现正式事实与保留存档入口；控制接续和遗产权利仍须各自具名程序。
extends RefCounted

## 保存沿当前内核身份；切换世界、人物或冷恢复后旧按钮失效。
static func _source(host) -> Dictionary:
	var context: Dictionary=host.kernel.command_context()
	var identity: Dictionary=host.kernel.source_identity()
	var exact: Dictionary={}
	for key in ["world_id","execution_epoch","revision","tick"]: exact[key]=identity.get(key)
	return {"actor_id":context.get("actor_id",""),"kernel_id":host.kernel.get_instance_id(),"identity":exact}

## 页面和主场景均存活且来源未变才执行保存，不用已死亡人物的行动资格拦截终局存档。
static func _save(host_ref: WeakRef,panel_ref: WeakRef,source: Dictionary,callback: Callable) -> void:
	var host=host_ref.get_ref(); var panel: Control=panel_ref.get_ref()
	if not is_instance_valid(host) or not is_instance_valid(panel) or not panel.is_inside_tree() or not panel.is_visible_in_tree() or panel.is_queued_for_deletion(): return
	if host._advancing or _source(host)!=source or not callback.is_valid(): return
	callback.call()

## 缺来源明确拒绝接续；已记录时保留原钱货、预留、工程和公职，不声称本页已完成遗产清算。
static func build(host,parent: Control,view: Dictionary,on_save: Callable,on_close: Callable) -> Control:
	var panel=PanelContainer.new(); panel.name="FamilyDeathScreen"; parent.add_child(panel)
	panel.anchor_left=.06; panel.anchor_right=.94; panel.anchor_top=.15; panel.anchor_bottom=.91
	var column=VBoxContainer.new(); column.add_theme_constant_override("separation",14); panel.add_child(column)
	host._text(column,"人生与遗留事务",27)
	var state: String=view.get("status","")
	if state=="awaiting_legacy_review":
		var death: Dictionary=view.get("death",{})
		host._text(column,"此人的生命已经结束。",22)
		host._text(column,"已核验军旅伤亡，发生于世界时刻 %d。"%int(death.get("death_tick",0)),18)
		host._text(column,"遗留事务待清点。原有私产、预留与未结责任仍保留来源；公职和公共资产另行办理。",18)
		host._text(column,"当前尚无可在此办理的合法成年接续方案，可以先保存此刻的世界。",18)
	elif state=="living": host._text(column,"此人仍然存活，没有本人的生命终点记录。",20)
	else: host._text(column,"尚无已核验的生命终点来源，不能据此办理接续或处分遗产。",20)
	var footer=HBoxContainer.new(); column.add_child(footer)
	var source: Dictionary=_source(host); var valid: bool=view.get("actor_id")==source.actor_id and view.get("source_identity")==source.identity
	var host_ref: WeakRef=weakref(host); var panel_ref: WeakRef=weakref(panel)
	var save: Button=host._button(footer,"保存此刻的世界","save","family_death",func(): _save(host_ref,panel_ref,source,on_save))
	save.name="FamilyDeathSave"; save.disabled=not valid or host._advancing
	host._button(footer,"返回标题","family_death_close","",func():
		var current: Control=panel_ref.get_ref()
		if is_instance_valid(current) and current.is_inside_tree() and current.is_visible_in_tree() and not current.is_queued_for_deletion() and on_close.is_valid(): current.hide(); on_close.call()).name="FamilyDeathClose"
	return panel
