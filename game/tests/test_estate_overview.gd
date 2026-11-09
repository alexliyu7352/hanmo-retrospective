## 庄务真实有限投影和规则报价的headless控件门；回调录参不冒充整态结算或GUI。
extends SceneTree
const Screen=preload("res://ui/estate_screen.gd")
const Page=preload("res://core/views/estate_page.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const F=preload("res://tests/family_estate_view_fixture.gd")
var checks: int=0
var failures: int=0
var samples: Array=[]
## 仅复用Main控件合同和真实页查询，提交出口留原形参数供核对。
class Host extends Control:
	var _data: Dictionary
	var kernel: RefCounted
	var _estate_id: String="estate.z"
	var _scene_counter: int=0
	var _message: String=""
	var _advancing: bool=false
	var status_label: Label
	var submissions: Array=[]
	var stage: Control
	## 与Main相同的换行和字号。
	func _text(parent: Node, value: String, font_size: int=20) -> Label:
		var label=Label.new(); label.text=value; label.autowrap_mode=TextServer.AUTOWRAP_WORD_SMART
		label.add_theme_font_size_override("font_size",font_size); parent.add_child(label); return label
	## 与Main相同的语义、禁用和信号入口。
	func _button(parent: Node, value: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var result=Button.new(); result.text=value; result.custom_minimum_size=Vector2(90,40)
		result.set_meta("command_kind",kind); result.set_meta("target_id",target); result.disabled=disabled
		result.pressed.connect(callback); parent.add_child(result); return result
	## 姓名只读原作者表。
	func _name(record: Dictionary) -> String: return _data.texts.get(record.get("name_key",""),record.get("id",""))
	## 原拒绝码原样留证。
	func _reason(code: String) -> String: return code
	## 只记录生产页交给Main的原形命令，不伪装完整World提交。
	func _perform(kind: String, params: Dictionary={}) -> void: submissions.append({"kind":kind,"params":params.duplicate(true)})
	## 关闭后旧来源不再可用。
	func _close_estate() -> void: _estate_id=""; _render()
	## 每次只从当前有限页取数，完全移除前一批控件。
	func _render() -> void:
		_scene_counter+=1
		for child in stage.get_children(): stage.remove_child(child); child.queue_free()
		Screen.render(self,stage,Page.query(kernel._state,_data,_estate_id,get_meta("estate_tab","build")))
## 引擎树就绪后执行。
func _init() -> void: call_deferred("run")
## 所有失败进入退出码。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",message)
## 等待布局和递延释放。
func settle() -> void:
	for _frame in range(4): await process_frame
## 只搜索当前显示树。
func control(host: Host, id: String) -> Control: return host.stage.find_child(id,true,false)
## 使用原庄务夹具和真实预览注册，未新增任何设施类型。
func make_host(extent: Vector2) -> Host:
	var f: Dictionary=F.create(); check(f.ok,"原领域夹具初始化"); f.state.estates["estate.z"].manager_actor_id=""
	var host=Host.new(); host.size=extent; root.add_child(host)
	host._data=f.data; host.kernel=Kernel.new(); host.kernel._state=f.state; host.kernel._catalog={"data":f.data}; host.kernel._registry=Registry.build(Builtin.descriptors())
	host.stage=Control.new(); host.add_child(host.stage); host.stage.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	host.set_meta("estate_tab","build"); host._render(); return host
## 三档布局、零级、选中留存及同源回调都经过实际生产Screen。
func run() -> void:
	for extent in [Vector2(1672,941),Vector2(1280,800),Vector2(800,800)]:
		var host: Host=make_host(extent); await settle()
		var before: Dictionary=host.kernel._state.duplicate(true)
		var selector: Button=control(host,"EstateFacility_granary")
		check(selector!=null,"真实设施查看入口 "+str(extent))
		if selector!=null:
			check(selector.text.contains("尚未营造"),"零级不冒充已建")
			selector.pressed.emit(); await settle()
			check(host.submissions.is_empty() and host.kernel._state==before,"选设施不建设或推进时间")
			var build: Button=control(host,"EstateBuild_granary")
			check(build!=null and not build.disabled,"原规则允许选中仓廪营造")
			if build!=null:
				build.pressed.emit(); check(host.submissions==[{"kind":"estate_build","params":{"estate_id":"estate.z","facility":"granary"}}],"绑定本庄原设施ID")
				host._advancing=true; build.pressed.emit(); check(host.submissions.size()==1,"忙碌不能重交")
				host._advancing=false; host._estate_id="estate.a"; build.pressed.emit(); check(host.submissions.size()==1,"换庄拒绝旧来源")
				host._estate_id="estate.z"; host._scene_counter+=1; build.pressed.emit(); check(host.submissions.size()==1,"换页拒绝旧代数")
			host._render(); await settle(); check(control(host,"EstateFacility_granary").button_pressed,"同庄重绘保选中")
		for tab in ["build","people","storage"]:
			host.set_meta("estate_tab",tab); host._render(); await settle()
			var frame: Control=control(host,"EstateScreen")
			var scroll: ScrollContainer=control(host,"EstatePageScroll")
			check(scroll!=null,"整页可滚动 "+tab)
			if frame!=null:
				var overflow: Array=[]
				for button in frame.find_children("*","Button",true,false):
					var rect: Rect2=button.get_global_rect(); var area: Rect2=frame.get_global_rect()
					if rect.position.x<area.position.x-1 or rect.end.x>area.end.x+1: overflow.append(button.name)
					check(button.size.y>=44,"点击目标至少44高 "+str(button.name))
				check(overflow.is_empty(),"水平不溢出 "+str(extent)+" "+tab+" "+str(overflow))
				samples.append({"viewport":[extent.x,extent.y],"tab":tab,"frame":[frame.position.x,frame.position.y,frame.size.x,frame.size.y],"horizontal_overflow":overflow})
				var last_button: Control=frame.find_children("*","Button",true,false).back()
				if scroll!=null:
					scroll.ensure_control_visible(last_button); await settle()
					check(scroll.get_global_rect().intersects(last_button.get_global_rect()),"滚动可抵达末尾操作 "+tab)
		# 非庄主仍能查看，但原preview拒绝营造，强制旧信号也不透过禁用边界。
		var state: Dictionary=host.kernel._state
		state.estates["estate.z"].owner_actor_id="provider"; host._data.estates["estate.z"].owner_actor_id="provider"
		host.set_meta("estate_tab","build"); host._render(); await settle()
		var denied: Button=control(host,"EstateBuild_granary")
		check(denied!=null and denied.disabled and denied.tooltip_text=="EstateAuthorityRequired","非庄主保原拒绝原因")
		if denied!=null:
			var submitted: int=host.submissions.size(); denied.pressed.emit(); check(host.submissions.size()==submitted,"禁用报价不能借强制信号提交")
		var previous_meta: String=host.get_meta("estate_facility","")
		var old_selector: Button=control(host,"EstateFacility_farm")
		host._estate_id="estate.a"; old_selector.pressed.emit(); check(host.get_meta("estate_facility","")==previous_meta,"旧庄选择不能写新庄界面选择")
		host._estate_id="estate.z"; host.kernel._state.actors.player.location.place_id="away"; host._render(); await settle()
		check(control(host,"EstateScreen")==null,"离庄后页面不存在，不远程办理")
		host.free(); await settle()
	print("ESTATE_OVERVIEW_LAYOUT=",JSON.stringify(samples)); print("ESTATE_OVERVIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
