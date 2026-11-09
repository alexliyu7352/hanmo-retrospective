## 有限收藏页的真实控件门；不依赖原生整世界，不把headless回调称为GUI验收。
extends SceneTree
const Screen=preload("res://ui/growth_cards_screen.gd")
var checks: int=0
var failures: int=0

## 只记录原命令接口的调用，世界身份与revision可显式改变以验证旧控件失效。
class Kernel extends RefCounted:
	var revision: int=1
	var actor: String="person"
	var world: String="world-a"
	var allow: bool=true
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":revision}
	func source_identity() -> Dictionary: return {"world_id":world,"execution_epoch":1}
	func preview(_kind: String, _params: Dictionary) -> Dictionary: return {"ok":allow,"revision":revision}

## 宿主只提供控件和正常命令入口，收藏页不能从这里读整世界或改写掌握度。
class Host extends Control:
	var kernel=Kernel.new()
	var _advancing: bool=false
	var calls: Array=[]
	func _text(parent: Control, value: String, _size: int) -> Label:
		var label=Label.new(); label.text=value; parent.add_child(label); return label
	func _button(parent: Control, value: String, _kind: String, _id: String, callback: Callable) -> Button:
		var button=Button.new(); button.text=value; parent.add_child(button); button.pressed.connect(callback); return button
	func _perform(kind: String, params: Dictionary) -> void: calls.append({"kind":kind,"params":params})

## 每项检查均执行，失败保留明确控件语义。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+message)

## 三行分别为未知、仅跨局发现、实际本局心法，不能把图鉴状态当可配置能力。
func view() -> Dictionary:
	var rows: Array=[]
	for index in range(3):
		rows.append({"card_id":"heart"+str(index),"category":"heart","name":"养气" if index>0 else "尚未发现","description":"修行所得" if index>0 else "","hint":"寻访师傅","discovered":index>0,"discovered_free":index==1,"acquired_this_world":index==2,"learned":index==2,"personal_skill_id":"heart.skill" if index==2 else "","mastery":30 if index==2 else null,"configured":false,"acquired_tick":4 if index==2 else null})
	return {"actor_id":"person","cards":rows,"offset":32,"limit":32,"total":96,"next_offset":64}

## 让控件进入真实SceneTree再触发输入，关闭和换世界后的旧信号均不得提交。
func _initialize() -> void: call_deferred("run")
func run() -> void:
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,800)
	var pages: Array=[]; var closed: Array=[]
	var panel: Control=Screen.build(host,host,view(),func(): closed.append(true),func(offset: int): pages.append(offset))
	await process_frame
	check(panel.find_children("CardConfigureHeart","Button",true,false).size()==1,"only current learned heart gets configuration button")
	check(panel.find_children("CardState","Label",true,false).size()==3,"every card has explicit knowledge state")
	check(panel.find_child("Card_heart1",true,false).find_child("CardState",true,false).text.contains("自由创建记录"),"free-only discovery is visibly marked")
	var button: Button=panel.find_child("CardConfigureHeart",true,false)
	button.pressed.emit(); button.pressed.emit()
	check(host.calls.size()==1 and host.calls[0].params.skill_id=="heart.skill","repeat click submits at most once")
	panel.find_child("GrowthCardsPrevious",true,false).pressed.emit(); panel.find_child("GrowthCardsNext",true,false).pressed.emit()
	check(pages==[0,64],"pagination requests bounded adjacent offsets")
	panel.find_child("GrowthCardsClose",true,false).pressed.emit(); button.disabled=false; button.pressed.emit()
	check(closed.size()==1 and host.calls.size()==1,"closed page cannot configure")
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view(),func(): closed.append(true),func(_offset: int): pass)
	await process_frame
	button=panel.find_child("CardConfigureHeart",true,false); host.kernel.world="world-b"; button.pressed.emit()
	check(host.calls.size()==1 and button.disabled,"old page cannot submit to another world")
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,view(),func(): pass,func(_offset: int): pass)
	await process_frame
	button=panel.find_child("CardConfigureHeart",true,false); host.kernel.revision+=1; button.pressed.emit()
	check(host.calls.size()==1 and button.disabled,"revision change rejects stale card request")
	panel.queue_free(); await process_frame
	host.kernel.allow=false; panel=Screen.build(host,host,view(),func(): pass,func(_offset: int): pass)
	await process_frame
	check(panel.find_child("CardConfigureHeart",true,false).disabled,"failed normal preview disables configuration")
	panel.queue_free(); await process_frame
	host.kernel.allow=true; host.kernel.actor="different-person"
	panel=Screen.build(host,host,view(),func(): pass,func(_offset: int): pass)
	await process_frame
	check(panel.find_child("CardConfigureHeart",true,false).disabled,"view belonging to another actor cannot bind current command identity")
	host.queue_free(); await process_frame
	print("GROWTH_CARDS_SCREEN %d checks %d failures"%[checks,failures]); quit(1 if failures else 0)
