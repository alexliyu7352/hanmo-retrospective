## 保存实际静态bind回调后释放Node，再执行迟到回调；日志中的任何SCRIPT ERROR都算失败。
extends SceneTree
const Economic=preload("res://ui/economic_world_panel.gd")
const Dynamic=preload("res://ui/dynamic_world_panel.gd")
const Navigation=preload("res://tests/test_economic_world_navigation.gd")
func _initialize(): call_deferred("run")
## 活host保留时释放控件，再释放host；两种失效对象都必须在进入业务前被挡住。
func run():
	create_timer(5).timeout.connect(func(): quit(2))
	var host=Navigation.Host.new();root.add_child(host)
	var button=Button.new();host.add_child(button)
	var spin=SpinBox.new();host.add_child(spin)
	var callbacks=[
		Economic._page.bind(host,button,{},{},{},1),
		Economic._choose_authority.bind(host,button,{},{},1),
		Economic._configure.bind(host,button,{},"configure",{},1),
		Economic._configure.bind(host,button,{},"stop",{},1),
		Economic._configure.bind(host,button,{},"cancel_pending",{},1),
		Economic._remember_workers.bind(3.0,host,spin,"unit","labor",{},1),
		Dynamic._choose_page.bind(host,button,"economy",{},1),
		Dynamic._submit.bind(host,button,{},{},1)
	]
	button.free();spin.free()
	for callback in callbacks: callback.call()
	assert(host.commands.is_empty() and host.rendered==0)
	var selections=[Dynamic._select.bind(host,"task_id","old",{}),Dynamic._remember_quantity.bind(3.0,host,spin,"old",{},1)]
	host.free()
	for callback in callbacks+selections: callback.call()
	var live=Navigation.Host.new();root.add_child(live)
	live.kernel.actor_id="player";live.kernel.identity={"revision":0};live.kernel.view={"place_id":"town"}
	assert(Dynamic.prepare(live));Dynamic.build(live,live)
	var actual_spin=live.find_child("DynamicQuantity",true,false)
	var actual_quantity: Callable=actual_spin.get_signal_connection_list("value_changed")[0]["callable"]
	var choice=Dynamic._selection_button(live,live,"合同详情","dynamic_contract","old","task_id","old",live.get_meta(Dynamic.DRAFT_META)["source"])
	var actual_choice: Callable=choice.get_signal_connection_list("pressed")[-1]["callable"]
	actual_quantity.call(1.0)
	assert(live.get_meta(Dynamic.DRAFT_META)["quantities"]["need"]==1 and live.rendered==1)
	live._scene_counter+=1;actual_choice.call();assert(live.rendered==1)
	var draft_before=live.get_meta(Dynamic.DRAFT_META).duplicate(true)
	actual_spin.free();choice.free();actual_quantity.call(1.0);actual_choice.call()
	assert(live.get_meta(Dynamic.DRAFT_META)==draft_before and live.rendered==1 and live.commands.is_empty())
	live.free();actual_quantity.call(1.0);actual_choice.call()
	print("economic_freed_callbacks: REACHED_END (require zero engine errors)")
	quit()
