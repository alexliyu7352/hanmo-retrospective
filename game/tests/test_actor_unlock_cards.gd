## 原收藏页的人物卡区域与迟到分页门；有限控件验证，不代正式World解锁。
extends "./test_growth_cards_screen.gd"
func actor_view() -> Dictionary:
	return {"actor_id":"person","cards":[],"offset":0,"limit":32,"total":0,"next_offset":null,"actor_cards":{"cards":[
		{"card_id":"first","name":"采办熟手","description":"以采购委托积累见识。","hint":"完成两次采购委托并回报。","completed_count":1,"acquired":null,"unlocked":false},
		{"card_id":"earned","name":"经办有方","description":"","hint":"","completed_count":2,"acquired":{"acquired_tick":8},"unlocked":false},
		{"card_id":"normal","name":"熟悉仓务","description":"","hint":"","completed_count":0,"acquired":null,"unlocked":true,"unlocked_free":false,"unlock_source":{"acquired_tick":8}},
		{"card_id":"free","name":"自由人物","description":"","hint":"","completed_count":0,"acquired":null,"unlocked":true,"unlocked_free":true,"unlock_source":{"acquired_tick":8}}
	],"offset":32,"limit":32,"total":96,"next_offset":64,"unlock_notice":"记录暂不可用"}}
func run() -> void:
	var host=Host.new(); root.add_child(host); host.size=Vector2(1280,800)
	var pages: Array=[]
	var panel: Control=Screen.build(host,host,actor_view(),func(): pass,func(_offset: int): pass,func(offset: int): pages.append(offset))
	await process_frame
	var rows=panel.find_children("ActorUnlockState","Label",true,false)
	check(rows.size()==4,"actor cards use same collection panel with distinct states")
	if rows.size()!=4:
		host.queue_free(); await process_frame; print("ACTOR_UNLOCK_CARDS %d checks %d failures"%[checks,failures]); quit(1); return
	check(panel.find_child("ActorCard_first",true,false).find_child("ActorUnlockProgress",true,false).text.contains("1 / 2"),"real current procurement progress remains visible")
	check(rows[1].text.contains("尚未保存"),"earned world condition does not pretend profile persistence")
	check(rows[2].text.contains("已解锁") and not rows[2].text.contains("自由"),"normal source is separate from current progress")
	check(rows[3].text.contains("自由创建"),"free source is clearly marked")
	check(panel.find_children("CardConfigureHeart","Button",true,false).is_empty() and panel.find_children("CardPractice*","Button",true,false).is_empty() and host.calls.is_empty(),"actor unlock does not offer current ability or control commands")
	var previous: Button=panel.find_child("ActorCardsPrevious",true,false)
	var next: Button=panel.find_child("ActorCardsNext",true,false)
	var late: Callable=next.pressed.get_connections()[0].callable
	previous.pressed.emit(); next.pressed.emit()
	check(pages==[0,64],"actor pagination remains independently bounded")
	host.kernel.revision+=1; next.pressed.emit()
	check(pages.size()==2,"source revision change rejects old actor page")
	host.kernel.revision-=1; panel.find_child("GrowthCardsClose",true,false).pressed.emit(); next.pressed.emit()
	check(pages.size()==2,"closed collection refuses old actor pagination")
	panel.queue_free(); await process_frame; late.call()
	check(pages.size()==2,"freed actor panel safely ignores captured callback")
	panel=Screen.build(host,host,{"actor_id":"person","cards":[],"offset":0,"limit":32,"total":0,"next_offset":null},func(): pass,func(_offset: int): pass)
	check(panel.find_children("ActorUnlockState","Label",true,false).is_empty(),"legacy card page needs no actor-card data")
	host.queue_free(); await process_frame
	print("ACTOR_UNLOCK_CARDS %d checks %d failures"%[checks,failures]); quit(1 if failures else 0)
