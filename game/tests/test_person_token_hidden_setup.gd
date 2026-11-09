## 真实Main按钮建树顺序窄门；复用有限caller输入，缺纹理原样失败，不代表GUI或世界装载验收。
extends "res://tests/test_character_input_callers.gd"
const PersonToken=preload("res://ui/person_token.gd")
var clicked: int=0

## 只记录测试按钮的实际pressed信号，不提交游戏命令。
func pressed_probe() -> void: clicked+=1

## 新尾参必须是默认true，保证现有六参数调用保留既有图片行为。
func hidden_flag_available(main) -> bool:
	for method in main.get_method_list():
		if method.name!="_button": continue
		var available: bool=method.args.size()==7 and method.args[-1].name=="show_portrait" and method.default_args.size()==2 and method.default_args[-1]==true
		check(available,"Main._button提供尾部默认true的show_portrait开关")
		return available
	check(false,"真实Main按钮入口存在")
	return false

## 不传新参数仍创建原人物按钮、原默认分辨率并请求同一资源；只读输入和按钮回调保持。
func default_button() -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true); var main=main_for(f)
	var parent=Control.new(); root.add_child(parent)
	Character.clear_cache(); registry.reads.clear(); clicked=0
	var button: Button=main._button(parent,"原人物按钮","talk","role.a",pressed_probe)
	check(button is PersonToken and button.show_portrait and button.render_descriptor.get("actor_id")=="npc" and button.render_descriptor.get("resolution")==[256,320],"全部旧caller默认保留人物描述")
	check(registry.reads==[["portraits","adult.body.slender"]],"默认true仍按原路径请求本人纹理")
	button.pressed.emit()
	check(clicked==1 and button.caption=="原人物按钮" and button.get_meta("target_id")=="role.a" and button.mouse_entered.is_connected(button.queue_redraw),"字幕/身份/pressed与hover刷新保留")
	check(f==before and not main.is_inside_tree() and not main.storage._busy,"默认按钮不写世界或触发Native")
	parent.free(); main.free()

## 入树前关闭人物绘制时，已有分层描述和空描述的merchant旧fallback都不能取图。
func hidden_token_before_tree() -> void:
	var f: Dictionary=fixture()
	var prepared: Dictionary=Art.character("npc",f.state,f.data,"half_body")
	for descriptor in [{},prepared]:
		registry.reads.clear(); Character.clear_cache()
		var token=PersonToken.new(); token.show_portrait=false; token.caption="只显示字幕"; token.render_descriptor=descriptor.duplicate(true)
		root.add_child(token)
		check(registry.reads.is_empty() and token._portrait==null,"PersonToken入树前false阻止取纹理，空描述="+str(descriptor.is_empty()))
		check(token.caption=="只显示字幕" and token.mouse_entered.is_connected(token.queue_redraw) and token.mouse_filter==Control.MOUSE_FILTER_STOP,"隐藏画像仍保字幕和鼠标交互")
		token.free()

## 真实Main在查询和add_child之前接到false时，不制造旧256x320描述或隐形纹理。
func hidden_button() -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true); var main=main_for(f)
	if not hidden_flag_available(main): main.free(); return
	var parent=Control.new(); root.add_child(parent)
	Character.clear_cache(); registry.reads.clear(); clicked=0
	var button: Button=main.callv("_button",[parent,"名单人物","talk","role.a",pressed_probe,false,false])
	check(button is PersonToken and not button.show_portrait and button.render_descriptor.is_empty() and button._portrait==null,"Main在入树前设置false且不构造人物描述")
	check(registry.reads.is_empty() and Character._descriptors.is_empty(),"隐藏按钮不请求纹理或占人物描述缓存")
	button.pressed.emit()
	check(clicked==1 and button.caption=="名单人物" and button.get_meta("command_kind")=="talk" and button.get_meta("target_id")=="role.a" and button.mouse_entered.is_connected(button.queue_redraw),"隐藏按钮字幕、点击、身份及hover均保留")
	check(f==before,"隐藏按钮不改输入")
	parent.free(); main.free()

## 实际Composer创建两岗位名单：只为主焦点取图，按钮仍连接原当前人物的焦点切换。
func composer_hidden_roster() -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true); var main=main_for(f)
	var stage=Control.new(); root.add_child(stage); stage.size=Vector2(1280,720)
	Character.clear_cache(); registry.reads.clear()
	Screen.inhabitants(main,stage,main.kernel.place_sidebar(),f.data.places.room,{"phase":"scene"})
	var buttons: Array=[]
	for node in stage.find_children("*","Button",true,false):
		if node.get_meta("command_kind","")=="talk": buttons.append(node)
	check(buttons.size()==2,"同一NPC的两岗位仍均可列出")
	check(registry.reads==[["portraits","adult.body.slender"]],"名单建树只请求一次主焦点纹理，隐藏按钮零请求")
	var resolutions: Array=[]
	for value in cached_descriptors(): resolutions.append(value.resolution)
	check(resolutions==[[513,497]],"名单只留下实际焦点描述，不制造256x320旧按钮描述")
	for button in buttons:
		check(not button.show_portrait and button.render_descriptor.is_empty() and not button.caption.is_empty(),"隐藏名单项保字幕且没有不使用的画像 "+str(button.get_meta("target_id")))
		var focused: bool=false
		for connection in button.mouse_entered.get_connections():
			var args: Array=connection.callable.get_bound_arguments()
			if args.size()==4 and args[2] is Dictionary and args[2].get("actor_id")=="npc": focused=true
		check(focused and button.mouse_entered.is_connected(button.queue_redraw) and button.pressed.get_connections().size()==1,"原焦点hover、刷新与点击绑定保留")
	if not buttons.is_empty():
		registry.reads.clear(); buttons[0].mouse_entered.emit()
		var portrait: Control=stage.find_child("ScenePersonPortrait",true,false)
		check(registry.reads==[["portraits","adult.body.slender"]] and portrait.get_meta("render_result").get("code")=="MissingRenderTexture" and not portrait.visible,"实际hover仍请求主焦点，缺图隐藏旧脸并保原诊断")
	check(f==before and main.kernel.snapshot()==before.state and not main.is_inside_tree(),"真实名单和hover不改变世界或启动Main")
	stage.free(); main.free()

## 独立入口仅运行按钮初始模式和实际名单顺序，旧8b45 oracle门保持原件不更新。
func _run() -> void:
	root.size=Vector2i(1280,720)
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string(MANIFEST)).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	default_button(); hidden_token_before_tree(); hidden_button(); composer_hidden_roster()
	Art._registry=previous; Character.clear_cache()
	print("PERSON_TOKEN_HIDDEN_SETUP_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
