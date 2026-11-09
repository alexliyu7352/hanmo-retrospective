## 固定Godot本身产生独立精确向量；不含PCG替代实现，也不加载玩法脚本。
extends SceneTree

## 每个范围记录有符号状态字符串，并用同状态raw randi次数独立证明拒绝采样步数。
func _initialize() -> void:
	var cases: Array=[]
	var ranges: Array=[[0,9999],[1,100],[7,7],[-3,-3],[100,1],[0,1],[-10,10],[-2147483648,2147483647],[2147483647,-2147483648],[-1,2147483647],[-2147483648,0],[-2147483648,2147483646],[-2147483648,-2147483647],[2147483646,2147483647]]
	var seeds: Array=["597148785467037304","3088738697395591190","4242164904868466701","7377823470645531738","5291067661780965079","0","1","-1","-9223372036854775808","9223372036854775807","123456789"]
	var expected_initial: Array=["-4209960268200979286","-1250381258646700176","-6757424515913883941","-1017756221890669724","6235278727585056349"]
	var seed_rows: Array=[]
	for i in range(seeds.size()):
		var random=RandomNumberGenerator.new()
		random.seed=seeds[i].to_int()
		var initial: String=str(random.state)
		if i<expected_initial.size() and initial!=expected_initial[i]:
			push_error("FrozenWorldFactoryStateMismatch"); quit(2); return
		seed_rows.append({"seed":str(random.seed),"state":initial})
		cases.append({"name":"seeded_"+str(i),"seed":seeds[i],"state":initial})
	for state in ["0","1","-1","-9223372036854775808","9223372036854775807","-4209960268200979286"]:
		cases.append({"name":"restored_"+state,"seed":seeds[0],"state":state})
	var vectors: Array=[]
	for origin in cases:
		for limits in ranges:
			var random=RandomNumberGenerator.new()
			random.seed=origin.seed.to_int()
			random.state=origin.state.to_int()
			for index in range(4):
				var before: String=str(random.state)
				var value: int=random.randi_range(limits[0],limits[1])
				var after: String=str(random.state)
				var probe=RandomNumberGenerator.new()
				probe.seed=origin.seed.to_int()
				probe.state=before.to_int()
				var raw: Array=[]
				while str(probe.state)!=after:
					raw.append(probe.randi())
					if raw.size()>128:
						push_error("UnmatchedRngStateAdvance");quit(3);return
				vectors.append({"case":origin.name,"seed":str(random.seed),"before":before,"from":limits[0],"to":limits[1],"value":value,"after":after,"raw":raw})
	var args=OS.get_cmdline_user_args()
	if args.size()!=1:
		push_error("NeedOutputPath");quit(4);return
	var file=FileAccess.open(args[0],FileAccess.WRITE)
	if file==null:
		push_error("CannotWriteVectors");quit(5);return
	file.store_string(JSON.stringify({"format":"HM-GODOT-PCG32-VECTORS/1","engine":Engine.get_version_info(),"source_commit":"ed1daf0bf001b61586d9930840f2f1394092c079","seed_rows":seed_rows,"vectors":vectors},"",false,true)+"\n")
	file.close()
	print("RNG_VECTORS_OK ",vectors.size())
	quit(0)
