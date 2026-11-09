## 通过共享半日推进观察NPC交往、私产和有限知闻如何改变后续意愿，不从测试直接改关系。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Social=preload("res://core/social_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Codec=preload("res://core/save_codec.gd")
const Notes=preload("res://ui/journey_notes.gd")
var checks: int=0
var failures: int=0

## 等引擎和作者目录可读取后执行真实世界流程。
func _init() -> void:
	call_deferred("run_checks")

## 记录具体领域断言，任一失败最终返回非零进程状态。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 玩家在其他场所等候，社会变化必须由NPC自身策略和真实粮食产生。
func run_checks() -> void:
	var k=Kernel.new(); var loaded: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(loaded.ok,"社会配置通过正式内容和世界校验："+str(loaded))
	if not loaded.ok: finish(); return
	var params: Dictionary={"context":F.context(k,"role.carter")}
	check(k.preview("join_party",params,"actor.shen").ok,"尚未发生争端时车夫信任熟客，愿意约定同行")
	var initial: Dictionary=k.snapshot()
	check(initial.social.events.is_empty(),"新世界不补造已经发生的社会事件")
	for tick in range(4):
		check(F.send(k,"wait").ok and k.advance_one_tick().ok,"玩家在街头等候而NPC自主度过半日")
		var s: Dictionary=k.snapshot()
		check(s.social.last_tick==s.tick,"社会结算与唯一世界时钟一致")
		if tick==2:
			check(Social.trust(s,"actor.host","actor.shen")==10 and Social.trust(s,"actor.carter","actor.shen")==25,"受害者立即改变信任，尚未听闻者不自动全知")
	var state: Dictionary=k.snapshot(); var aid: int=0; var theft: int=0; var news: int=0
	for event in state.social.events.values():
		if event.kind=="aid": aid+=1
		if event.kind=="theft": theft+=1
		if event.kind=="news": news+=1
	check(aid==1 and theft==1 and news>=1,"同一人物之间发生实际援助、夺粮及后续转述")
	var total: int=0
	for actor_id in ["actor.host","actor.carter","actor.shen"]:
		total+=Inventory.count(state.inventories[state.actors[actor_id].inventory_id],"item.grain",[{"kind":"actor","id":actor_id}])
	check(total==4,"援助与冲突都转移原有私粮，不凭空增发或吞掉物资")
	check(Social.trust(state,"actor.carter","actor.shen")==15 and k.preview("join_party",params,"actor.shen").get("code")=="CompanionTrustRequired","实际消息使车夫改变之后的同行决定")
	check(not state.social.memories.has(state.player_actor_id),"远处玩家未听闻NPC私事，不能自动获得全部社会知识")
	check(Notes.social_notes(state,k.content().data,state.player_actor_id).is_empty() and not k.preview("ask_news").ok,"手记没有远方秘闻，没有当面对话不能远程问讯")
	var encoded: String=Codec.encode(state,0,"social-world",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==state,"关系、私粮和传播来源随完整世界保存恢复")
	check(F.send(k,"wait").ok and k.advance_one_tick().ok,"恢复后继续按原时间推进")
	var continued: Dictionary=k.snapshot()
	check(k.restore(state).ok and F.send(k,"wait").ok and k.advance_one_tick().ok and k.snapshot()==continued,"同一半日重启重放不会双重转粮或再次降低听闻者信任")
	var corrupt: Dictionary=continued.duplicate(true); corrupt.social.last_tick+=1
	check(not k.restore(corrupt).ok and k.snapshot()==continued,"损坏社会时钟不能靠读档补算或覆盖原世界")
	check(F.send(k,"move_local",{"place_id":"west.tavern"}).ok and F.send(k,"talk",{"role_id":"role.host"}).ok,"走进真实酒肆后与掌柜交谈")
	var before: Dictionary=k.snapshot()
	check(k.preview("ask_news").ok and k.snapshot()==before,"问讯预览不提前获取消息或改变信任")
	var heard: Dictionary=F.send(k,"ask_news")
	check(heard.ok and heard.details.news_ids.size()==1 and k.snapshot().tick==before.tick,"当面问讯只转述一条已有见闻，沿用对白时间规则")
	var memories: Dictionary=k.snapshot().social.memories[state.player_actor_id]
	check(memories.size()==1 and memories.values()[0].source_actor_id=="actor.host","首次听闻明确记录实际在场掌柜为来源")
	check(Notes.social_notes(k.snapshot(),k.content().data,state.player_actor_id).size()==2,"手记只显示实际听来的消息和来源")
	for _ask in range(4): check(F.send(k,"ask_news").ok,"继续问讯只查看说话者尚未转述的旧见闻")
	var exhausted: Dictionary=k.snapshot()
	var repeated: Dictionary=F.send(k,"ask_news")
	check(repeated.ok and repeated.details.news_ids.is_empty() and k.snapshot().social==exhausted.social and k.snapshot().relations==exhausted.relations,"重复打听已知事情不会刷关系或重复传播")
	check(F.send(k,"end_interaction").ok and not k.preview("ask_news").ok,"结束交谈后不能继续沿用旧人脉上下文")
	var saved_news: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"heard-news",k.content()),k.content())
	check(saved_news.ok and k.restore(saved_news.state).ok,"玩家问讯及每条转述来源通过完整读档校验")
	finish()

## 输出本脚本的规则与保存范围，不代替图形和长期经营验收。
func finish() -> void:
	print("SOCIAL_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
