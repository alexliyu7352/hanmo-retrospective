## 显式快照初货进入真实bag；存读仅恢复已有lot，不重发基础定义或消耗RNG。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
var checks=0
var failures=0
## 正式目录由独立Python准备入口供给，本脚本不绕过内容校验。
func _init(): call_deferred("run")
## 每个断言保留实际计数与非零退出。
func check(ok,label):
 checks+=1
 if not ok: failures+=1; printerr("FAIL:",label)
## 真实新局、规范化逆序和存档恢复之间逐字段比较权威状态。
func run():
 var paths=[]
 for argument in OS.get_cmdline_user_args():
  if argument.begins_with("--goods-content="): paths.append(argument.trim_prefix("--goods-content="))
 check(paths.size()==2,"two formally prepared goods orders supplied")
 if paths.size()!=2: quit(1);return
 var states=[]
 for path in paths:
  var kernel=Kernel.new();var started=kernel.new_game("scenario.west",path)
  check(started.ok,"armed snapshot starts normally "+str(started))
  if not started.ok: continue
  var state=kernel.snapshot();var actor=state.actors[state.player_actor_id];var bag=state.inventories[actor.inventory_id]
  check(actor.combat.weapon_id=="item.sword" and bag.lots.size()==2,"explicit weapon and grain are actual personal lots")
  check(bag.lots[0].item_id=="item.grain" and bag.lots[1].item_id=="item.sword" and bag.lots[1].quantity==1,"stable item order without inherited duplication")
  check(state.inventories["inventory.west"].lots.size()==1,"separate organizational inventory retains own cargo")
  states.append(state)
  var used=state.duplicate(true);used.inventories[actor.inventory_id].lots.remove_at(0)
  check(kernel.restore(used).ok,"valid remaining personal cargo can be restored")
  var context=kernel.save_context();var encoded=Codec.encode(context.state,1,"goods-test",context.catalog,context.registry)
  check(not encoded.is_empty(),"snapshot encodes through normal codec")
  var decoded=Codec.decode(encoded,context.catalog)
  check(decoded.ok,"same exact content decodes saved remaining cargo")
  if decoded.ok:
   check(kernel.restore(decoded.state).ok,"restored candidate accepted")
   check(kernel.snapshot()==used,"restore never reinitializes lots IDs or RNG")
 if states.size()==2:
  # 两作者源数组顺序不同产生不同整体digest，仅比较其余完整权威世界。
  states[0].erase("content_digest");states[1].erase("content_digest")
  check(states[0]==states[1],"reversed explicit goods keep complete state IDs and RNG")
 print("SCENARIO_GOODS_CHECKS=",checks," FAILURES=",failures)
 quit(0 if failures==0 else 1)
