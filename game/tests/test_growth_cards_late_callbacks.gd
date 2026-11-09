## 已释放卡片页的迟到Callable门；继承既有有限宿主，不重跑学习/Main矩阵。
extends "res://tests/test_growth_cards_screen.gd"

## 实际保存四个生产信号回调，在原Control完成释放后逐个触发，日志也必须零脚本错误。
func run() -> void:
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,800)
	var pages: Array=[]; var closed: Array=[]
	var panel: Control=Screen.build(host,host,view(),func(): closed.append(true),func(offset: int): pages.append(offset))
	await process_frame
	var delayed: Array[Callable]=[]
	for node_name in ["CardConfigureHeart","GrowthCardsPrevious","GrowthCardsNext","GrowthCardsClose"]:
		var control: Button=panel.find_child(node_name,true,false)
		check(control!=null and not control.disabled,"真实已绑定控件 "+node_name)
		delayed.append(control.get_signal_connection_list("pressed")[0].callable)
	delayed[0].call()
	check(host.calls.size()==1 and host.calls[0].params.skill_id=="heart.skill","活页面弱引用仍能提交一次合法配置")
	panel.queue_free(); await process_frame
	check(not is_instance_valid(panel),"原页面及子控件已实际释放")
	for callback in delayed: callback.call()
	check(host.calls.size()==1 and pages.is_empty() and closed.is_empty(),"释放后的配置/翻页/关闭回调均不执行")
	host.queue_free(); await process_frame
	for callback in delayed: callback.call()
	check(pages.is_empty() and closed.is_empty(),"宿主释放后回调继续安全失效")
	print("GROWTH_CARDS_LATE_CALLBACKS ",checks," checks ",failures," failures; stdout/stderr must contain no SCRIPT ERROR or ERROR")
	quit(1 if failures else 0)
