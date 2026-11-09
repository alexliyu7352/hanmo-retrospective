## 礼议课程局部集成门：复用真实ActionRuntime及学习事实，只检查新实例、存读和唯一奖励边界。
## 两个即时入口直接调用课程owner，正式Kernel注册和共享Schema由中央另外验证。
extends "res://tests/test_skill_study.gd"
const Etiquette=preload("res://core/domains/cultivation/study_etiquette.gd")

## 现有真实课程加显式题面及独立随机流；沿用已审三节点作者文件，不自造分数或修改游戏素材。
func challenge_fixture(mode: String="manual") -> Dictionary:
	var f: Dictionary=fixture()
	var definition: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/definition.json")).value
	f.data.etiquette_challenges={definition.id:definition}
	f.data.texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/etiquette_challenge/texts.json")).value
	f.data.roles["role.school"].skill_courses[0].etiquette_challenge_id=definition.id
	f.state.actors.student.attributes={"administration":60,"intellect":55}
	var random=RandomNumberGenerator.new(); random.seed=4319
	f.state.rng={"minigame":{"seed":str(random.seed),"state":str(random.state)},"world":{"seed":"12","state":"23"}}
	f.params.challenge_mode=mode
	return f

## 通过已绑定行动和当前节点的私有报价提交选择；源变动和旧报价沿原领域事务拒绝。
func choose(f: Dictionary, action: Dictionary, node_id: String, option_id: String) -> Dictionary:
	var params: Dictionary={"action_id":action.id,"node_id":node_id,"option_id":option_id}
	var offer: Dictionary=Study.preview_etiquette(f.state,f.data,"student",Etiquette.CHOOSE_KIND,params)
	if not check(offer.ok,"真实课程可预览当前礼议选项 "+str(offer)): return offer
	var applied: Dictionary=Study.apply_etiquette(f.state,f.data,"student",Etiquette.CHOOSE_KIND,params,offer)
	check(applied.ok and Study.validate_state(f.state,f.data).ok,"提交选择后仍是合法课程存档 "+str(applied))
	return applied

## 三条相互一致的作者选项只影响稍后课程倍率，不直接授奖。
func solve(f: Dictionary, action: Dictionary) -> void:
	for node in f.data.etiquette_challenges.values()[0].nodes:
		choose(f,action,node.id,node.options[0].id)

## 从正在进行的半日课程取得公开当前话题，不读取其他人物课题或未到轮次。
func instance_lifecycle() -> void:
	var f: Dictionary=challenge_fixture()
	var before: Dictionary=f.state.duplicate(true)
	var quoted: Dictionary=Study.preview(f.state,f.data,"student",f.params)
	check(quoted.ok and f.state==before and quoted.study.etiquette.actor_snapshot=={"skill_level":0,"auxiliary_attribute":60},"报价从真实礼学和政务冻结评分快照且不抽种")
	var random=RandomNumberGenerator.new(); random.seed=before.rng.minigame.seed.to_int(); random.state=before.rng.minigame.state.to_int()
	var expected_seed: String=str(random.randi())
	var action: Dictionary=begin(f)
	if action.is_empty(): return
	var original_stream: Dictionary=f.state.rng.duplicate(true)
	var fact: Dictionary=f.state.facts[action.id]
	check(fact.etiquette.challenge.seed==expected_seed and f.state.rng.minigame.state==str(random.state) and f.state.rng.world==before.rng.world,"只推进minigame流一次，其他流原字节语义不动")
	check(fact.etiquette.challenge.binding=={"actor_id":"student","source_id":action.id} and fact.etiquette.seed_origin==before.rng.minigame and not fact.etiquette.consumed,"实例固定真实人物/行动与一次seed来源")
	var view: Dictionary=Study.etiquette_view(f.state,f.data,"student",action.id)
	check(view.ok and view.phase=="active" and view.round==1 and view.options.size()==3,"只读第一轮公开题面")
	check(Study.etiquette_view(f.state,f.data,"teacher",action.id).get("code")=="NotStudying","参与授课不授予教师替学生答题权限")
	check(Study.etiquette_view(f.state,f.data,"student","action:old").get("code")=="StaleStudyAction","旧action不能引用当前课题")
	var nodes: Array=f.data.etiquette_challenges.values()[0].nodes
	choose(f,action,nodes[0].id,nodes[0].options[0].id)
	var restored: Dictionary=Strict.parse_text(JSON.stringify(f.state))
	check(restored.ok and restored.value==f.state and Study.validate_state(restored.value,f.data).ok,"第一轮已选实例严格JSON存读合法")
	var twin: Dictionary=f.duplicate(true); twin.state=restored.value
	for index in range(1,3):
		choose(f,action,nodes[index].id,nodes[index].options[0].id)
		choose(twin,twin.state.actions[action.id],nodes[index].id,nodes[index].options[0].id)
	check(f.state==twin.state and f.state.rng==original_stream,"存读后同选择得到同状态，选择不再次抽seed")
	check(fact.etiquette.challenge.result.performance_bp==12000 and not fact.etiquette.consumed and f.state.actors.student.skills.rites==80 and f.state.wallets["wallet.teacher"].balance==0,"答题满分只冻结≤120%倍率，未上课不领XP或费用")
	before=f.state.duplicate(true)
	check(not Study.preview_etiquette(f.state,f.data,"student",Etiquette.CHOOSE_KIND,{"action_id":action.id,"node_id":nodes[0].id,"option_id":nodes[0].options[0].id}).ok and f.state==before,"终态不能再次作答或重开")
	advance(f,action); advance(twin,twin.state.actions[action.id])
	var outcome: Dictionary=advance(f,action); advance(twin,twin.state.actions[action.id])
	check(f.state==twin.state and outcome.outcome.xp_gained==48 and f.state.actors.student.skills.rites==128,"同存档唯一结课写48XP，手动为正常40的120%")
	check(fact.etiquette.consumed and outcome.outcome.etiquette.score==100 and f.state.rng==original_stream,"结课消费位和原成绩保存，完成不抽新seed")
	before=f.state.duplicate(true)
	check(Study.finish(f.state,f.data,action,f.state.tick)==outcome.outcome and f.state==before,"重复完成只回原回执，不再次发奖或付款")
	var forged: Dictionary=f.state.duplicate(true); forged.facts[action.id].etiquette.consumed=false
	check(not Study.validate_state(forged,f.data).ok,"完成后清除消费位不能通过存档验证")
	forged=f.state.duplicate(true); forged.facts[action.id].etiquette.challenge.binding.source_id="action:other"
	check(not Study.validate_state(forged,f.data).ok,"实例不能搬到另一课程行动")
	forged=f.state.duplicate(true); forged.facts[action.id].etiquette.seed_origin.state="2"
	check(not Study.validate_state(forged,f.data).ok,"seed必须由保存的原流一次派生")

## 自动、主动退出和未答完直接走课时均保正常基线；取消/教师失效只消费实例不奖完整课程。
func baseline_and_interruption() -> void:
	for mode in ["automatic","manual"]:
		var f: Dictionary=challenge_fixture(mode); var action: Dictionary=begin(f)
		if action.is_empty(): continue
		if mode=="manual":
			var node: Dictionary=f.data.etiquette_challenges.values()[0].nodes[0]
			choose(f,action,node.id,node.options[0].id)
			var params: Dictionary={"action_id":action.id}
			var offer: Dictionary=Study.preview_etiquette(f.state,f.data,"student",Etiquette.EXIT_KIND,params)
			check(offer.ok and Study.apply_etiquette(f.state,f.data,"student",Etiquette.EXIT_KIND,params,offer).ok,"中途退出按原实例自动基线")
		advance(f,action)
		var outcome: Dictionary=advance(f,action)
		check(outcome.outcome.xp_gained==40 and f.state.facts[action.id].etiquette.consumed,"自动及退出完整课程均保留正常40XP")
	var skipped: Dictionary=challenge_fixture(); var skip_action: Dictionary=begin(skipped)
	if not skip_action.is_empty():
		advance(skipped,skip_action)
		var result: Dictionary=advance(skipped,skip_action)
		check(result.outcome.xp_gained==40 and result.outcome.etiquette.completion=="exited","未操作小游戏也可正常结课，无新的成长软锁")
	for cancel_now in [true,false]:
		var f: Dictionary=challenge_fixture(); var action: Dictionary=begin(f)
		if action.is_empty(): continue
		solve(f,action); advance(f,action)
		var outcome: Dictionary
		if cancel_now:
			outcome=Study.cancel(f.state,f.data,"student",action.id).details
			Step.release(f.state,action)
		else:
			f.state.roles["role.school"].enabled=false
			outcome=advance(f,action).outcome
		check(not outcome.success and outcome.xp_gained==24 and outcome.fee_paid==30 and outcome.unprovided_released==30,"取消或教师失效只结算已教一课24XP，不发未提供课时奖励")
		check(f.state.facts[action.id].etiquette.consumed and Study.validate_state(f.state,f.data).ok and f.state.wallets["wallet.student"].reserved.is_empty() and f.state.actors.student.skills.rites==104,"失败课程保留已教成长、封存实例并按原释放器释放余款")

## 作者源、私有报价、真值和半日取整均有具体负例；旧无题面课程仍走原基线且不要求随机流。
func source_and_rounding() -> void:
	var f: Dictionary=challenge_fixture(); var action: Dictionary=begin(f)
	if action.is_empty(): return
	var node: Dictionary=f.data.etiquette_challenges.values()[0].nodes[0]
	var params: Dictionary={"action_id":action.id,"node_id":node.id,"option_id":node.options[0].id}
	var offer: Dictionary=Study.preview_etiquette(f.state,f.data,"student",Etiquette.CHOOSE_KIND,params)
	f.data.texts[node.topic_key]+="变更"
	var before: Dictionary=f.state.duplicate(true)
	check(Study.apply_etiquette(f.state,f.data,"student",Etiquette.CHOOSE_KIND,params,offer).get("code")=="EtiquetteSourceChanged" and f.state==before,"题面文字外变不沿旧source和报价继续")
	f=challenge_fixture(); action=begin(f)
	var forged: Dictionary=f.state.duplicate(true); forged.facts[action.id].erase("etiquette")
	check(not Study.validate_state(forged,f.data).ok,"有题面课程不能删掉已开实例后重抽")
	forged=f.state.duplicate(true); forged.facts[action.id].terms.erase("etiquette"); forged.actions[action.id].params.study.erase("etiquette"); forged.facts[action.id].erase("etiquette")
	check(not Study.validate_state(forged,f.data).ok,"连条款一起删除也与实际作者课程不符")
	before=f.state.duplicate(true)
	check(not Study.begin(f.state,f.data,action,{}).ok and f.state==before,"原课程不能重复start刷新seed")
	f=challenge_fixture(); f.state.actors.student.fatigue=68; action=begin(f)
	if not action.is_empty():
		solve(f,action); advance(f,action)
		var outcome: Dictionary=advance(f,action)
		check(outcome.outcome.xp_gained==39 and 39*100<=33*120,"每课18/15先取整后各倍率为21/18，不把整课39.6重新凑整")
	var old: Dictionary=fixture(); var old_action: Dictionary=begin(old)
	if not old_action.is_empty():
		advance(old,old_action); var result: Dictionary=advance(old,old_action)
		check(result.outcome.xp_gained==40 and not old.state.facts[old_action.id].has("etiquette"),"无题面的原三字段课程兼容，不要求新RNG或伪造实例")

## 只执行新适配器的有限课程，旧纯叶50门及课程78/13/28不重复运行。
func run_checks() -> void:
	instance_lifecycle()
	baseline_and_interruption()
	source_and_rounding()
	print("STUDY_ETIQUETTE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
