## 只验个人战已结算片段的UI时钟与副本边界；不执行战斗命令，也不作为像素验收。
extends SceneTree

var checks: int=0
var failures: int=0

## 局部断言失败保留非零退出，便于与真实宿主节点证据分开判断。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 六阶段使用精确二进制小数，边界断言不依赖显示帧率或机器快慢。
func clip(id: String="clip.one") -> Dictionary:
	return {"ok":true,"playable":true,"id":id,"binding":{"kernel_id":17,"load_generation":0,"actor_id":"hero","world_id":"","execution_epoch":0},"battle_id":"battle.one","actors":{"hero":{"from_position":[10,10],"to_position":[20,20],"from_hp":100,"hp":90}},"phases":[{"kind":"prepare","duration":0.125,"events":[]},{"kind":"move","duration":0.25,"events":[{"action":"move","actor_id":"hero"}]},{"kind":"strike","duration":0.25,"events":[{"action":"attack","actor_id":"hero","target_id":"enemy","code":"Hit","damage":10}]},{"kind":"impact","duration":0.25,"events":[{"action":"attack","actor_id":"hero","target_id":"enemy","code":"Hit","damage":10},{"action":"counter","actor_id":"enemy","target_id":"hero","code":"Hit","damage":10}]},{"kind":"state","duration":0.125,"events":[]},{"kind":"settle","duration":0.125,"events":[]}]}

## 先验证实现存在，再走可恢复、完成、非法输入和零写的有限时序入口。
func _initialize() -> void:
	if "--signed-binding-only" in OS.get_cmdline_user_args():
		signed_binding_checks(); finish(); return
	check(ResourceLoader.exists("res://ui/personal_battle_playback.gd"),"个人回放时钟实现存在")
	if failures: finish(); return
	var Clock=load("res://ui/personal_battle_playback.gd")
	var player=Clock.new(); var source: Dictionary=clip(); var before: PackedByteArray=var_to_bytes(source)
	var initial: Dictionary=player.configure(source)
	check(initial.active and not initial.complete and initial.phase_kind=="prepare" and initial.phase_progress==0.0,"合法片段从准备开始")
	player.advance(0.125)
	check(player.frame().phase_kind=="move" and player.frame().phase_progress==0.0,"精确边界进入下一阶段")
	player.advance(0.125)
	check(player.frame().phase_kind=="move" and player.frame().phase_progress==0.5,"本地秒数确定移动进度")
	var resumed: Dictionary=player.snapshot(); var same: Dictionary=player.configure(source)
	check(same.elapsed==0.25 and same.phase_progress==0.5,"同对象同片段重配不从头播放")
	var restored=Clock.new(); restored.configure(source,resumed)
	check(restored.frame()==player.frame(),"重绘新对象由同源快照精确续放")
	restored.set_paused(true); var held: Dictionary=restored.frame(); restored.advance(50.0)
	check(restored.frame()==held and held.active,"暂停冻结UI时间而保留播放输入门")
	var pause_snapshot: Dictionary=restored.snapshot(); var paused_copy=Clock.new(); paused_copy.configure(source,pause_snapshot)
	check(paused_copy.frame()==held,"暂停状态随宿主UI元数据恢复")
	restored.set_speed(2.0); restored.set_paused(false); restored.advance(0.1875)
	check(restored.frame().phase_kind=="impact" and restored.frame().events.size()==2,"双倍速保同阶段同时伤害事件")
	var speed_snapshot: Dictionary=restored.snapshot(); var speed_copy=Clock.new(); speed_copy.configure(source,speed_snapshot)
	check(speed_copy.frame()==restored.frame(),"倍速和进度一起恢复")
	var returned: Dictionary=restored.frame(); returned.events[0].damage=999
	check(restored.frame().events[0].damage==10,"frame返回事件深副本")
	var saved: Dictionary=restored.snapshot(); saved.binding.actor_id="intruder"
	check(restored.snapshot().binding.actor_id=="hero","snapshot身份无引用别名")
	player.advance(20.0); var normal_final: Dictionary=player.frame()
	restored.skip(); var skip_final: Dictionary=restored.frame(); var fast=Clock.new(); fast.configure(source); fast.set_speed(2.0); fast.advance(20.0)
	check(terminal(normal_final) and terminal(skip_final) and terminal(fast.frame()),"正常双倍速与跳过均到真实终态")
	check(normal_final.elapsed==skip_final.elapsed and normal_final.elapsed==fast.frame().elapsed,"三条抵达路径总时长一致")
	check(player.configure(source).complete,"已完成同片段不会重播")
	check(player.configure(source,resumed).complete,"同对象已完成不能被较旧显式快照复活")
	var monotonic=Clock.new(); monotonic.configure(source); monotonic.advance(0.625); monotonic.configure(source,resumed)
	check(monotonic.frame().elapsed==0.625,"同对象重配不接受旧UI进度回退")
	var finished_copy=Clock.new(); finished_copy.configure(source,player.snapshot())
	check(finished_copy.frame().complete,"已完成片段跨重绘不重播")
	check(var_to_bytes(source)==before,"所有播放控制对公开输入逐字节零写")
	var caller_mutated: Dictionary=clip(); var isolated=Clock.new(); isolated.configure(caller_mutated); caller_mutated.phases[3].events[0].damage=999; caller_mutated.binding.actor_id="changed"; isolated.advance(0.625)
	check(isolated.frame().events[0].damage==10 and isolated.snapshot().binding.actor_id=="hero","调用者后续修改不能污染已接收片段")
	for field in ["kernel_id","load_generation","actor_id","world_id","execution_epoch"]:
		var changed: Dictionary=clip(); changed.binding[field]="other" if field in ["actor_id","world_id"] else 99
		var foreign=Clock.new(); foreign.configure(changed,resumed)
		check(terminal(foreign.frame()) and foreign.frame().reason=="resume_identity_mismatch","异源恢复直接終态 "+field)
	var old=Clock.new(); old.configure(clip("clip.two"),resumed)
	check(terminal(old.frame()),"上一轮快照不能应用给新片段")
	for delta in [-1.0,INF,NAN]:
		var broken=Clock.new(); broken.configure(source); broken.set_paused(true); broken.advance(delta)
		check(terminal(broken.frame()),"非法delta即使暂停仍交回输入 "+str(delta))
	var speed=Clock.new(); speed.configure(source); speed.set_speed(2.0)
	for value in [0.0,-1.0,1.5,INF,NAN]:
		speed.set_speed(value); check(speed.frame().speed==2.0,"非法倍速不引入停滞或倒放 "+str(value))
	speed.advance(20.0); check(terminal(speed.frame()),"拒绝非法倍速后仍能完成")
	for patch in [{"elapsed":NAN},{"elapsed":-1.0},{"elapsed":99.0},{"speed":0.0},{"paused":"yes"},{"complete":"yes"},{"version":999}]:
		var damaged: Dictionary=resumed.duplicate(true); damaged.merge(patch,true); var damaged_copy=Clock.new(); damaged_copy.configure(source,damaged)
		check(terminal(damaged_copy.frame()),"损坏恢复不会挂住输入 "+str(patch.keys()))
	var changed_timeline: Dictionary=clip(); changed_timeline.phases[1].duration=0.5
	var mismatched=Clock.new(); mismatched.configure(changed_timeline,resumed)
	check(terminal(mismatched.frame()),"同ID不同时间线拒绝恢复")
	var empty: Dictionary=clip(); empty.playable=false; empty.phases=[]
	var cold=Clock.new(); check(terminal(cold.configure(empty)),"冷恢复没有旧锚点直接真实终态")
	var zero: Dictionary=clip()
	for phase in zero.phases: phase.duration=0.0
	check(terminal(Clock.new().configure(zero)),"全零阶段不除零或循环")
	var skipped_phase: Dictionary=clip(); skipped_phase.phases[1].duration=0.0
	var sparse=Clock.new(); sparse.configure(skipped_phase); sparse.advance(0.125)
	check(sparse.frame().phase_kind=="strike","空移动阶段确定跳过")
	for bad_duration in [-1.0,INF,NAN,"long"]:
		var malformed: Dictionary=clip(); malformed.phases[1].duration=bad_duration
		check(terminal(Clock.new().configure(malformed)),"非法时长直接终态 "+str(bad_duration))
	var invalidated=Clock.new(); invalidated.configure(source); invalidated.invalidate()
	check(terminal(invalidated.frame()) and invalidated.configure(source).complete,"失效后同片段不自动重播")
	check(invalidated.configure(clip("clip.next")).active,"下一合法片段可重新取得独立UI时钟")
	var no_objects: Dictionary=clip(); no_objects.phases[0].events=[{"kernel":RefCounted.new()}]
	check(terminal(Clock.new().configure(no_objects)),"事件中的对象引用拒绝进入时钟")
	finish()

## 只复现真实RefCounted的有符号实例ID边界；可独立运行，不重跑已有时序矩阵。
func signed_binding_checks() -> void:
	var Clock=load("res://ui/personal_battle_playback.gd")
	var owner=RefCounted.new(); var instance_id: int=owner.get_instance_id()
	print("PERSONAL_PLAYBACK_SIGNED_BINDING instance_id=",instance_id)
	check(instance_id<0,"固定Godot4.7.2真实RefCounted返回负的有符号ID")
	var source: Dictionary=clip(); source.binding.kernel_id=instance_id
	var player=Clock.new(); var current: Dictionary=player.configure(source)
	check(current.active,"真实非零负实例ID可以播放")
	check(player.snapshot().binding.get("kernel_id")==instance_id,"实例ID保持原有符号整数不取绝对值或截断")
	var restored=Clock.new(); restored.configure(source,player.snapshot())
	check(restored.frame().active,"同一真实负ID允许快照恢复")
	for invalid_id in [0,1.0,str(instance_id)]:
		var invalid: Dictionary=clip(); invalid.binding.kernel_id=invalid_id
		check(terminal(Clock.new().configure(invalid)),"零值或非整数ID仍被拒绝 "+str(invalid_id))

## 稳定终态不保受击事件，也不留阻止玩家下一轮操作的active标记。
func terminal(value: Dictionary) -> bool:
	return not value.active and value.complete and value.phase_kind=="settle" and value.phase_progress==1.0 and value.events.is_empty()

## 汇报只包含纯时序协议的真实局部门，不声称世界或图形回归。
func finish() -> void:
	print("PERSONAL_PLAYBACK checks=",checks," failures=",failures)
	quit(1 if failures else 0)
