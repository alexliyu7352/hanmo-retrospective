## 真实compose→stage→Catalog→v2存读→单次Snapshot→冷Kernel恢复，未替换为合成锁。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Resolver=preload("res://core/content_set_resolver.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Native=preload("res://storage/native_storage.gd")
const Strict=preload("res://core/strict_json.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
var checks=0
var failures=0
## 引擎就绪后读取受信索引，不以源码存在性代替行为。
func _init():call_deferred("run")
## 失败逐项保留并造成非零退出。
func check(ok,label):
 checks+=1
 if not ok: failures+=1;printerr("FAIL:",label)
## 同一个真实文本、目录与registry贯通全部步骤，恢复前不建立默认世界。
func run():
 var installed=Catalog.installed_catalogs();check(installed.ok,"installed original catalogs load")
 if not installed.ok: print(installed);quit(1);return
 var catalog=installed.catalogs[0];var scenario=catalog.data.scenarios.keys()[0]
 var source=Kernel.new();check(source.new_game(scenario,catalog.catalog_path).ok,"actual composed new game")
 var context=source.save_context()
 var text=Codec.encode(context.state,1,"manual-1",context.catalog,context.registry)
 check(not text.is_empty(),"real installed set writes v2")
 var metadata=Codec.inspect_metadata(text);check(metadata.ok and metadata.metadata.format_version==2,"v2 physical metadata")
 if not metadata.ok: quit(1);return
 var registry=Registry.build(Builtin.descriptors())
 var matched=Resolver.resolve(metadata.metadata,installed.catalogs,registry);check(matched.ok,"same real lock resolves original catalog")
 var selection=Codec.inspect_restore_selection(text);check(selection.ok,"bounded candidate selection")
 if not matched.ok or not selection.ok:quit(1);return
 var effective=Snapshot.resolve(matched.catalog,selection.scenario_id,selection.founder_actor_id)
 check(effective.ok,"one snapshot from original installed catalog")
 var decoded=Codec.decode(text,effective.catalog,registry);check(decoded.ok,"full v2 decode against selected view")
 if not decoded.ok: print(decoded);quit(1);return
 var target=Kernel.new();check(target.snapshot().is_empty(),"restore target has never started a default world")
 var restored=target.restore_with_catalog(decoded.state,effective.catalog,registry)
 check(restored.ok,"atomic cold restore")
 check(target.snapshot()==context.state,"complete world IDs cargo facts and RNG identical")
 var before=target.save_context();var bad=decoded.state.duplicate(true);bad.player_actor_id="missing"
 check(not target.restore_with_catalog(bad,effective.catalog,registry).ok and target.snapshot()==before.state and target.content()==before.catalog,"bad candidate preserves complete existing target")
 var folder="user://a2-installed-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
 var native=Native.new(folder,context.catalog,context.registry)
 var stored=native.put("manual-1",text,0)
 check(stored.ok,"native writes real v2 bank")
 if not stored.ok:print(stored);quit(1);return
 var physical=native.inspect_slot("manual-1")
 check(physical.ok and physical.text==text,"native metadata keeps exact highest bank bytes")
 var reread=native.get_slot("manual-1")
 check(reread.ok and Codec.decode(reread.get("text",""),context.catalog,context.registry).get("state")==context.state,"native v2 decode receives same registry")
 var no_registry=Native.new(folder,context.catalog)
 var blocked=no_registry.get_slot("manual-1")
 check(not blocked.ok and blocked.get("code")=="IncompatibleRuleModules","missing registry cannot silently rebuild v2 context")
 check(native.inspect_slot("manual-1").get("text")==text,"semantic failure leaves bank unchanged")
 var index_path="res://data/generated/installed-content.json"
 var index_bytes=FileAccess.get_file_as_bytes(index_path)
 var index=Strict.parse_text(index_bytes.get_string_from_utf8()).value
 var bad_index=index.duplicate(true);bad_index.sets[0].directory="../outside"
 write_bytes(index_path,JSON.stringify(bad_index,"",true,true).to_utf8_buffer())
 check(not Catalog.installed_catalogs().ok,"index path escape rejected")
 write_bytes(index_path,index_bytes)
 var asset=""
 for row in index.sets[0].files:
  if row.path.ends_with(".png"):asset="res://data/generated/"+index.sets[0].directory+"/"+row.path;break
 check(not asset.is_empty(),"real installed PNG fixture exists")
 if not asset.is_empty():
  var original=FileAccess.get_file_as_bytes(asset)
  var changed=original.duplicate();changed[changed.size()-1]=changed[changed.size()-1]^1
  write_bytes(asset,changed)
  check(Catalog.installed_catalogs().get("code")=="InstalledContentDigestMismatch","actual resource byte tamper rejected")
  write_bytes(asset,original)
 check(Catalog.installed_catalogs().ok,"original exact artifact bytes restored")
 print("INSTALLED_RESTORE_CHECKS=",checks," FAILURES=",failures)
 quit(0 if failures==0 else 1)

## 只在这个独立测试工程中改写再还原同一原字节，不接收任意外部目标。
func write_bytes(path: String, bytes: PackedByteArray) -> void:
 var file=FileAccess.open(path,FileAccess.WRITE)
 file.store_buffer(bytes);file.close()
