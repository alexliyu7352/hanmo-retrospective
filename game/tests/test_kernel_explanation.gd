## 正式Kernel解释入口回归：同一世界只读投影、明确观察者来源和保存重载，不替代外部身份认证。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Explain=preload("res://core/runtime/explanation_query.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 延迟到引擎启动后通过正式入口执行局部集成回归。
func _init() -> void:
	call_deferred("run_checks")

## 记录有语义的检查；缺入口和引擎运行失败不能变成空测试通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 用现有玩家等待与NPC规则制造实际亲历、转述和未知差异。
func social_world():
	var kernel=Kernel.new()
	var opened: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(opened.ok,"原Alpha世界通过正式创建入口")
	if not opened.ok: return null
	for _tick in range(4): check(F.send(kernel,"wait").ok and kernel.advance_one_tick().ok,"原世界半日实际生成社会事实和消息")
	return kernel

## 正式方法须与唯一纯投影保持相同结果；query载荷不能替代受信调用方指定的观察者。
func check_visibility(kernel, state: Dictionary, queries: Array) -> void:
	var data: Dictionary=kernel.content().data
	for query in queries:
		for observer in ["",state.player_actor_id,"actor.shen","actor.carter"]:
			check(kernel.explain(query,observer)==Explain.explain(state,data,query,observer),"Kernel仅复用唯一解释规则："+str(query.kind)+"/"+observer)
	var unknown: Dictionary=kernel.explain(queries[0])
	var witnessed: Dictionary=kernel.explain(queries[0],"actor.shen")
	var heard: Dictionary=kernel.explain(queries[0],"actor.carter")
	check(unknown.known_causes.is_empty() and witnessed.known_causes.size()==1 and heard.known_causes.size()==1,"正式入口保留未知、亲历和听闻差异")
	if not witnessed.known_causes.is_empty() and not heard.known_causes.is_empty():
		check(witnessed.known_causes[0].knowledge=="witnessed" and heard.known_causes[0].knowledge=="reported","显式观察者只看该人物已有来源")
	for field in ["observer_id","actor_id","player_actor_id"]:
		var injected: Dictionary=queries[0].duplicate(true)
		injected[field]="actor.shen"
		var refused: Dictionary=kernel.explain(injected)
		check(refused.get("code")=="InvalidExplanationQuery" and refused.known_causes.is_empty(),"查询不能覆盖观察者："+field)
	check(kernel.explain(queries[0],"actor.missing").get("code")=="UnknownObserver","无效显式观察者不能回落玩家视角")
	check(kernel.explain({"kind":"route","route_id":"route.unknown"}).get("code")=="UnsupportedExplanationQuery","Kernel不增加路网枚举或知识获取旁路")

## 修改所有返回容器和重复100轮各类查询都不能改变正式Kernel持有的状态、内容、RNG或知识。
func check_purity(kernel, queries: Array) -> void:
	var before: Dictionary=kernel.snapshot()
	var content_before: Dictionary=kernel.content()
	var witnessed: Dictionary=kernel.explain(queries[0],"actor.shen")
	if not witnessed.known_causes.is_empty(): witnessed.known_causes[0].fact.quantity=999
	witnessed.known_causes.append({"kind":"forged"})
	witnessed.unknown_parts.append({"code":"forged"})
	witnessed.next_options.append({"kind":"query","query":{"kind":"world"}})
	var command: Dictionary=kernel.explain(queries[1])
	if not command.next_options.is_empty(): command.next_options[0].query.result_id="result:forged"
	if not command.known_causes.is_empty(): command.known_causes[0].command.kind="forged"
	check(kernel.snapshot()==before and kernel.content()==content_before,"返回的嵌套事实、命令和下一查询不是世界引用")
	check(kernel.explain(queries[0],"actor.shen")!=witnessed and kernel.explain(queries[1])!=command,"改坏一次解释不污染后续读取")
	for _repeat in range(100):
		for query in queries:
			for observer in ["","actor.shen","actor.carter"]: kernel.explain(query,observer)
		kernel.explain({"kind":"world"})
	check(kernel.snapshot()==before and kernel.content()==content_before,"100轮正式查询保持完整状态、五条RNG、实例号、知识和内容")

## 存档重载保留实际来源和可见性；之后的相同合法推进仍得到同一世界而非重放查询副作用。
func check_reload(kernel, queries: Array) -> void:
	var before: Dictionary=kernel.snapshot()
	var encoded: String=Codec.encode(before,0,"kernel-explanation",kernel.content())
	var decoded: Dictionary=Codec.decode(encoded,kernel.content())
	check(decoded.ok and decoded.state==before,"正式解释后字节存档仍保留完整状态")
	if not decoded.ok: return
	var resumed=Kernel.new()
	check(resumed.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok and resumed.restore(decoded.state).ok,"另一Kernel通过正式恢复入口载入同一世界")
	if resumed.snapshot()!=before: check(false,"恢复状态必须完全一致"); return
	for query in queries:
		for observer in ["","actor.shen","actor.carter"]:
			check(resumed.explain(query,observer)==kernel.explain(query,observer),"重载保留每类查询的原知识与来源："+str(query.kind)+"/"+observer)
	check(F.send(kernel,"wait").ok and F.send(resumed,"wait").ok and kernel.advance_one_tick().ok and resumed.advance_one_tick().ok and kernel.snapshot()==resumed.snapshot(),"查询及保存重载不改变之后实际半日的结果")

## 测试作者目录合法指定另一个开局人物，避免硬编码actor.player冒充当前玩家；不伪造世代继任。
func check_current_player() -> void:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json"))
	check(parsed.ok,"独立作者夹具读取真实准备目录")
	if not parsed.ok: return
	var header: Dictionary=parsed.value
	header.catalog.scenarios["scenario.alpha_life"].player_actor="actor.carter"
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://test-kernel-explanation-current-player.json"
	var file=FileAccess.open(path,FileAccess.WRITE)
	check(file!=null,"独立测试目录可写，不改正式作者内容")
	if file==null: return
	file.store_string(JSON.stringify(header)); file.close()
	var kernel=Kernel.new()
	var opened: Dictionary=kernel.new_game("scenario.alpha_life",path)
	check(opened.ok,"原加载器接受作者明确指定的另一开局人物："+str(opened))
	if not opened.ok: return
	check(F.send(kernel,"wait").ok,"新当前玩家通过原命令入口产生自己的收据")
	var state: Dictionary=kernel.snapshot()
	var query: Dictionary={"kind":"command","request_seq":state.request_sequences[state.player_actor_id]}
	var answer: Dictionary=kernel.explain(query)
	check(state.player_actor_id=="actor.carter" and answer==kernel.explain(query,"actor.carter"),"默认观察者使用真实当前玩家而非固定人物ID")
	check(answer.known_causes.size()==1 and kernel.explain(query,"actor.player").known_causes.is_empty(),"默认收据不借用原actor.player的命令知识")

## 检查空世界再运行已提交状态；明确该内部API的observer参数由受信调用方选择，不提供外部调用者认证。
func run_checks() -> void:
	var empty=Kernel.new()
	check(empty.has_method("explain"),"Kernel提供正式解释入口")
	if not empty.has_method("explain"): finish(); return
	var empty_result: Dictionary=empty.explain({"kind":"fact","fact_id":"fact.unknown"})
	check(empty_result.get("code")=="NoWorld" and empty_result.known_causes.is_empty() and empty_result.revision==0,"无世界时不访问缺失catalog.data且明确返回NoWorld")
	check(empty.explain({}).get("code")=="NoWorld" and empty.snapshot().is_empty() and empty.content().is_empty(),"空世界失败不生成隐式状态或内容")
	var kernel=social_world()
	if kernel==null: finish(); return
	var state: Dictionary=kernel.snapshot()
	var fact_id: String=""
	for event in state.social.events.values():
		if event.kind=="theft": fact_id=event.id; break
	check(not fact_id.is_empty(),"专属集成测试只解释真实发生的事件")
	if fact_id.is_empty(): finish(); return
	var seq: int=state.request_sequences[state.player_actor_id]
	var receipt: Dictionary=state.results[state.player_actor_id+":"+str(seq)]
	var queries: Array=[{"kind":"fact","fact_id":fact_id},{"kind":"command","request_seq":seq},{"kind":"result","result_id":receipt.result_id},{"kind":"offer","offer_id":"offer.grain"}]
	check_visibility(kernel,state,queries)
	check_purity(kernel,queries)
	check_reload(kernel,queries)
	check_current_player()
	finish()

## 仅汇报该薄入口的实际检查，不将局部通过当作完整A1.8或外部身份访问控制验收。
func finish() -> void:
	print("KERNEL_EXPLANATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
