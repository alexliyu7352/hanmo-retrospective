## 用另一份真实内容目录验证路况时段、配置消费与封路等待，不依赖城市显示名称。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0
## 延后执行，确保临时测试目录与资源读取可用。
func _init() -> void:
	call_deferred("_run")
## 记录具体失败并让进程以非零状态结束。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ",label)
## 通过公开版本化命令驱动真实世界，不直接给测试增加时间或奖励。
func cmd(k, kind: String, params: Dictionary = {}) -> Dictionary:
	var s: Dictionary=k.snapshot()
	return k.submit({"kind":kind,"params":params,"request_seq":s.request_sequences[s.player_actor_id]+1,"expected_revision":s.revision})
## 生成第二份索引并交给完整运行时校验；原作者文件和原索引不被修改。
func fixture(cost: int) -> String:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/two_stories.hmcatalog.json"))
	var index: Dictionary=parsed.value
	var edge: Dictionary=index.catalog.routes["road.west_east"]
	edge.duration_ticks=4; edge.cost_per_tick=cost; edge.fatigue_per_tick=7
	edge.blocked_from_tick=1; edge.blocked_until_tick=3
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	var path: String=OS.get_user_data_dir().path_join("road-conditions-"+str(cost)+".json")
	var file=FileAccess.open(path,FileAccess.WRITE)
	file.store_string(JSON.stringify(index,"",true,true)); file.close()
	return path
## 验证已上路的人遇到封锁可等待再开通，零费用道路不得错误触发缺钱救助。
func _run() -> void:
	var k=Kernel.new()
	check(k.new_game("scenario.two_stories",fixture(11)).ok,"完整目录装载器接受道路配置")
	var quote: Dictionary=k.preview("travel",{"destination":"village.east"})
	check(quote.ok and quote.cost==44 and quote.duration==4,"路线报价使用配置费用与时耗")
	check(cmd(k,"travel",{"destination":"village.east"}).ok and cmd(k,"advance").ok,"封锁前实际行进一次")
	var s: Dictionary=k.snapshot(); var hero: String=s.player_actor_id
	check(s.tick==1 and s.wallets[s.actors[hero].wallet_id].balance==789 and s.actors[hero].fatigue==7,"消费11钱和7疲劳，不使用旧常量")
	check(k.preview("advance").get("code")=="RoadBlocked","封锁生效时不继续穿越")
	check(cmd(k,"travel_wait").ok and cmd(k,"travel_wait").ok,"封锁期间可等到开放边界")
	var waited: Dictionary=k.snapshot()
	check(waited.tick==3 and waited.actors[hero].location==s.actors[hero].location,"等待不移动、不刷新起点")
	check(waited.wallets==s.wallets,"封路等待不扣额外行旅费用")
	check(cmd(k,"resume_travel").ok and cmd(k,"advance").ok,"开放边界恢复正常行走")
	var before: Dictionary=k.snapshot()
	check(cmd(k,"return").ok and cmd(k,"advance").ok and cmd(k,"advance").ok,"改为沿实走路段返回")
	s=k.snapshot()
	check(s.actors[hero].location.get("settlement_id")=="city.west" and s.wallets[s.actors[hero].wallet_id].balance==756,"四次实际步进各收11钱，等待没有收费")
	var free=Kernel.new(); free.new_game("scenario.two_stories",fixture(0))
	s=free.snapshot(); s.wallets[s.actors[hero].wallet_id].balance=0
	check(free.restore(s).ok,"合法无钱测试状态")
	check(cmd(free,"travel",{"destination":"village.east"}).ok,"零费用道路不要求八钱")
	check(not free.preview("rescue").ok,"没有费用缺口时不应显示缺钱救助")
	print("ROAD_CONDITION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
