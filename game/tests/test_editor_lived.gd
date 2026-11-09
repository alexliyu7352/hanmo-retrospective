## Exercise actual local author-process communication, not an in-process replacement of its document model.
extends SceneTree
var checks: int=0
var failures: int=0
## Run once the tree can host the process client.
func _init() -> void:
	call_deferred("_run")
## Record explicit assertion failures without hiding program exit status.
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failures+=1; printerr("FAIL: ",label)
## Issue an actual process request and wait for its asynchronously polled response.
func ask(client, op: String, args: Dictionary={}) -> Dictionary:
	check(client.request(op,args),"request accepted: "+op)
	for _frame in range(20000):
		if not client.busy: return client.last_reply
		await process_frame
	check(false,"worker response timeout: "+op)
	return {"ok":false}

## Verify author mutations, disk readback and loading the resulting content with the real Godot catalog.
func _run() -> void:
	var path: String="res://addons/hanmo_author/"+"client.gd"
	check(FileAccess.file_exists(path),"Godot can communicate with the existing author worker")
	if not FileAccess.file_exists(path): quit(1); return
	var client=load(path).new(); root.add_child(client)
	var base: String=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir()
	check(client.start_worker("python3",base.path_join("tools/authoring_worker.py")).ok,"real author process starts")
	var reply: Dictionary=await ask(client,"open",{"path":base.path_join("content/foundation/lived_city.json")})
	check(reply.ok,"source opens")
	reply=await ask(client,"set_text",{"key":"rebuild.test.text","value":"测试编辑器不会改动运行中的周目"})
	check(reply.ok and client.document.dirty,"edit marks dirty")
	reply=await ask(client,"undo"); check(reply.ok and not client.document.dirty,"undo to original bytes")
	reply=await ask(client,"redo"); check(reply.ok and client.document.dirty,"redo restores edit")
	reply=await ask(client,"clone_record",{"table":"actors","record_id":"actor.he","new_id":"actor.editor_copy"})
	check(reply.ok and reply.result.id=="actor.editor_copy","clone creates distinct identity")
	var folder: String=OS.get_user_data_dir().path_join("editor-check-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var target: String=folder.path_join("edited.json")
	reply=await ask(client,"save",{"path":target})
	check(reply.ok and FileAccess.file_exists(target),"Save As writes real new source")
	reply=await ask(client,"open",{"path":target}); check(reply.ok and not client.document.dirty,"saved file reopens")
	var output: String=folder.path_join("edited.hmcatalog.json")
	reply=await ask(client,"prepare",{"output":output})
	check(reply.ok and FileAccess.file_exists(output),"author pack prepared through original validator")
	var catalog: Dictionary=load("res://core/content_catalog.gd").read_prepared(output)
	check(catalog.ok and catalog.data.actors.has("actor.editor_copy"),"same game catalog reads edited character")
	var kernel=load("res://core/world_kernel.gd").new()
	check(kernel.new_game("scenario.lived_city",output).ok,"edited city starts in production kernel")
	check(kernel.content().data.roles["role.inn"].activities.rest.cost==20,"editor retained typed service terms")
	client.stop_worker(); client.queue_free(); await process_frame
	print("EDITOR_LIVED_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
