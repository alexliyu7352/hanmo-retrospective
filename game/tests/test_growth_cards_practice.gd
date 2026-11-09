## 新修炼入口的来源、异步时长选择与迟到回调门，不重跑原收藏/学习矩阵。
extends "res://tests/test_growth_cards_screen.gd"
const Practice=preload("res://ui/growth_cards_practice.gd")

## 报价给出明确不同的掌握与疲劳结果，确保界面展示原返回值而非重算规则。
class PracticeKernel extends RefCounted:
	var revision: int=1
	var actor: String="person"
	var world: String="world-a"
	var allow: bool=true
	var requests: Array=[]
	func command_context() -> Dictionary: return {"ok":true,"actor_id":actor,"revision":revision}
	func source_identity() -> Dictionary: return {"world_id":world,"execution_epoch":1}
	func preview(kind: String, params: Dictionary) -> Dictionary:
		requests.append({"kind":kind,"params":params.duplicate(true)})
		if kind!="practice_skill": return {"ok":true,"revision":revision}
		if not allow: return {"ok":false,"code":"TrainingNeedsRest","revision":revision}
		return {"ok":true,"revision":revision,"duration":params.ticks,"cost":0,"training":{"skill_id":params.skill_id,"student_id":actor,"duration":params.ticks,"mastery_before":30,"mastery_after":73 if params.ticks==4 else 55,"student_fatigue":19 if params.ticks==4 else 6}}

## 只给本局已学卡片修炼；选择经重绘保留，正式再报价后同页只提交一次。
func run() -> void:
	var host=Host.new(); host.kernel=PracticeKernel.new(); root.add_child(host); host.size=Vector2(1280,800)
	var dto: Dictionary=view(); var panel: Control=Screen.build(host,host,dto,func(): pass,func(_offset): pass)
	await process_frame
	check(panel.find_children("CardPractice","Button",true,false).size()==1,"只有本局已学未满掌握的卡有修炼入口")
	check(Practice.requests(host,dto.cards)==[{"skill_id":"heart.skill","ticks":1}],"prepare只请求当前已学本领和所选时长")
	var duration: OptionButton=panel.find_child("CardPracticeDuration",true,false); var practice: Button=panel.find_child("CardPractice",true,false)
	var status: Label=panel.find_child("CardPracticeStatus",true,false)
	check(duration.item_count==8 and not practice.disabled and status.text.contains("30 → 55"),"1至8半日和原报价掌握结果可见")
	duration.select(3); duration.item_selected.emit(3)
	check(Practice.requests(host,dto.cards)==[{"skill_id":"heart.skill","ticks":4}] and status.text.contains("30 → 73") and status.text.contains("疲劳增加19"),"选择改变原preview参数，展示原收益和疲劳，不自行推算")
	var late_practice: Callable=practice.get_signal_connection_list("pressed")[0].callable
	var late_duration: Callable=duration.get_signal_connection_list("item_selected")[0].callable
	panel.queue_free(); await process_frame; late_practice.call(); late_duration.call(0)
	check(host.calls.is_empty(),"释放后的修炼和选择回调不执行")
	panel=Screen.build(host,host,dto,func(): pass,func(_offset): pass); await process_frame
	practice=panel.find_child("CardPractice",true,false); duration=panel.find_child("CardPracticeDuration",true,false)
	check(duration.selected==3,"同一世界人物异步重绘保留所选4半日")
	practice.pressed.emit(); practice.pressed.emit(); panel.find_child("CardConfigureHeart",true,false).pressed.emit()
	check(host.calls.size()==1 and host.calls[0].kind=="practice_skill" and host.calls[0].params=={"skill_id":"heart.skill","ticks":4},"同页重复修炼或配置不能并发追加命令")
	panel.queue_free(); await process_frame
	panel=Screen.build(host,host,dto,func(): pass,func(_offset): pass); await process_frame
	host.kernel.revision+=1; panel.find_child("CardPractice",true,false).pressed.emit(); check(host.calls.size()==1,"旧revision报价失效后不提交修炼")
	panel.queue_free(); await process_frame
	host.kernel.revision=1; host.kernel.allow=false
	panel=Screen.build(host,host,dto,func(): pass,func(_offset): pass); await process_frame
	check(panel.find_child("CardPractice",true,false).disabled and panel.find_child("CardPracticeStatus",true,false).text.contains("恢复伤势或疲劳"),"原规则拒绝保留明确恢复出口")
	panel.queue_free(); await process_frame
	host.kernel.world="world-b"; check(Practice.requests(host,dto.cards)[0].ticks==1,"另一世界不继承旧页面时长选择")
	var capped: Dictionary=view(); capped.cards[2].mastery=100
	panel=Screen.build(host,host,capped,func(): pass,func(_offset): pass); await process_frame
	check(panel.find_children("CardPractice","Button",true,false).is_empty() and Practice.requests(host,capped.cards).is_empty(),"已满掌握不会继续展示或准备修炼")
	host.queue_free(); await process_frame; late_practice.call(); late_duration.call(1)
	check(true,"宿主释放后迟到回调可安全结束；日志另核零引擎错误")
	print("GROWTH_CARDS_PRACTICE %d checks %d failures; engine diagnostics must be empty"%[checks,failures]); quit(1 if failures else 0)
