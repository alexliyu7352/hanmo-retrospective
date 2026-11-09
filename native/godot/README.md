# 隔离Linux只读Bridge

本片验证固定Godot 4.7.2的隔离Linux只读桥：一个 `HanmoWorldBridge : RefCounted` 通过判别union只持有一个v1库存或legacy人物承诺实例，两者复用相同Flecs存储实现。**真实X11图形首次导入→自然退出→新进程自动加载/查询已通过；headless冷首次导入仍BLOCKED。** 这是受验证的图形初始化流程，不是引擎headless生命周期修复；默认游戏接入、release export与其他平台未验收。

## 依赖、精度和已知阻碍

- 官方稳定 `godot-4.5-stable`／`e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77` 已先实际尝试；真实4.7.2 SDK的新整数极值常量与stdint宏冲突，编译失败。未改SDK或第三方源码。
- 当前仅此隔离片选官方 **10.0.0-rc2**／`5ed72a0dc2517a8082598a950895c6b24e8aa282`，MIT，明确是RC。官方生成器2325–2339行已处理该冲突。精确200份源码／许可证摘要在 `../dependencies.lock.json`；无子模块、无SCons安装。
- `GODOTCPP_PRECISION=single`；real_t 32位、指针64位，官方builtin配置因此叫 `float_64`。编译static_assert验证指针和Vector2大小，未启用REAL_T_IS_DOUBLE。
- 固定编辑器用 `GODOTCPP_TARGET=template_debug`；CMake `Release` 是编译优化，不能与Godot release模板混淆。hot reload关闭，异常启用。扩展仅声明 `linux.debug.x86_64`，不代表release export已通过。
- 精确SDK由固定引擎实际导出，API SHA `d0e4c08c03b165156dabe6bfb6a906baf0069189f62035341230a246c86d6986`；接口JSON／header和编辑器本体SHA均在锁中，不能拿另一个4.7 SDK替代。

历史headless首次editor自动发现的最小对照：全新无扩展工程0；只有官方绑定初始化、无类0；新空 `ProbeRef : RefCounted`（无Hanmo/Flecs/字段/方法）SIGABRT；runtime-only注册及正确template_debug均复现。实际构造追踪未见Probe实例被创建。已有缓存后的第二次导入0，不能覆盖第一次失败。见 `../../tests/fixtures/native_inventory_bridge/editor_discovery_probe.cpp`。尚未证明确切引擎内部根因，不归罪于某个相似上游issue。后续独立兼容调查须保留上述控制组，用固定SDK定位首发现的新类注册／编辑器生命周期；不以跳过整个editor扩展初始化冒充修复。2026-10-04真实桌面调查、两个独立阻碍及新链接修复见图形验证记录（未公开）。

## 调用合同

原v1三个同步主线程方法均返回独立Dictionary，不接Kernel或可写组件指针。

- `install_artifact(PackedByteArray)`：传精确原字节，一次成功安装；失败后可重试。成功返回 `ok/input_sha256/payload_sha256`。成功后再装保原 `AlreadyInstalled`。
- `query_batch(PackedByteArray)`：未安装保原 `NotInstalled`；整批成功返回原format、input/queries SHA和counts，count是规范十进制String。失败无部分counts。每个文档64MiB、深64与原严格JSON/typed wire完全沿用原库；桥无CLI双文件合计限额的独立文件操作。
- `audit()`：返回installed、install_count、两份来源SHA、按原序的inventory_lot_ids、实际组件SHA及编译身份，每层容器重建。空实例摘要为空，仅installed=false，不伪造来源。

非PackedByteArray明确 `ExpectedPackedByteArray /arguments/bytes`，防Godot隐式转码。native原错误code/path透传。边界捕获C++异常为 `NativeBridgeFailure /bridge`。

**Godot String输出是原CLI字符串域的严格子域。** 实测官方带长度 `String::utf8(data,size)` 仍截断U+0000；测试 `String.chr(0)` 又会先替换为U+FFFD，不能拿它制造伪通过。桥对所有动态返回字符串检查NUL并验证UTF8逐字节往返；不可无损表示时返回 `BridgeUnrepresentableString /bridge/string`，没有counts或半份audit。这包括query_id、原错误路径、库存／lot审计ID。原native输入／查询语义未变；该输出类型拒绝不重装、不损坏已装只读实例。已安装包含这类ID的合法native工件，其audit可以明确拒绝，不能因此假称未安装或允许再次安装。

返回值／输入的调用方修改不影响下一次查询。最后RefCounted释放由RAII销毁唯一实例；没有全局实例、刷新、world_id、模拟、写回、save/load或第二资产账本。

安装的发布点在成功返回值准备之后：局部candidate先执行原install_once、完整audit复制及Dictionary构造，全部成功才用不抛异常的unique_ptr移动成为桥持有实例。任何提交前C++异常都析构该candidate，桥仍未安装；不能返回NativeBridgeFailure却留下已装世界。成功后拒绝再次安装，判别union的monostate直接代表未发布，没有另存loaded标记。

## 同一CMake构建

沿 `../world/README.md` 先取得并核既有三库；本片的godot-cpp和SDK目录也必须与锁逐文件一致。构建只读本地依赖，不自动联网。以下变量用明确的本机绝对路径；CMake与Python沿项目便携安装：

```sh
"$CMAKE" -S native/world -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
  -DHANMO_FLECS_ROOT="$FLECS" -DHANMO_JSON_ROOT="$JSON" \
  -DHANMO_PICOSHA2_ROOT="$PICOSHA2" -DHANMO_BUILD_GODOT_BRIDGE=ON \
  -DHANMO_GODOT_CPP_ROOT="$GODOT_CPP" -DHANMO_GODOT_API_ROOT="$SDK" \
  -DPython3_EXECUTABLE="$PYTHON"
"$CMAKE" --build "$BUILD" --target hanmo_world_bridge --parallel 3
```

输出 `$BUILD/godot-bridge/libhanmo_world_bridge.so`，链接原hanmo_world及其Flecs目标。Linux/ELF以 `--exclude-libs,ALL` 隐藏静态依赖符号，并以 `exports.map` 将直接对象的C++ RTTI也设为local；实际动态定义导出必须仅为 `hanmo_world_library_init`；否则libstdc++ locale/唯一符号可与图形驱动依赖混用。固定SDK、原生核心、C ABI入口与静态C++链接方式均未更换。默认OFF不读取Godot依赖、不改变原CLI入口。源码commit仅是Git基线，审计另带精确源码／锁／SDK／profile摘要；未提交WIP不能只凭commit字段冒充已冻结身份。

## 有界实测流程

复用已冻结5组23条旧 `Inventory.count` oracle及其CLI audit，不再执行299条导出。准备器不生成预期。`$PREPARED_GAME` 是既有真实采购轻工程，`$CASES`／`$CLI_AUDITS` 指向已核冻结输出，`$FIXTURE` 必须全新：

```sh
"$PYTHON" tests/fixtures/native_inventory_bridge/prepare_fixture.py \
  --prepared-game "$PREPARED_GAME" --cases-root "$CASES" \
  --cli-audits-root "$CLI_AUDITS" --output "$FIXTURE"
"$GODOT" --headless --path "$FIXTURE/game" --editor --import
"$PYTHON" tests/fixtures/native_inventory_bridge/prepare_fixture.py \
  --attach-to "$FIXTURE" --library "$BUILD/godot-bridge/libhanmo_world_bridge.so"
"$GODOT" --headless --path "$FIXTURE/game" \
  --script res://tests/test_native_inventory_bridge.gd
```

每步检查实际退出码；第二步仅导入纯GD工程，第三步才加入扩展。第四步新进程真实 `GDExtensionManager.load_extension`，不是首editor发现通过。不要在已加载时覆盖库；新身份用新fixture。局部字符串门在最后命令附加 `-- --string-boundaries-only`。

原红门／首134／宏冲突／最小profile漏OS日志均保留。16条字符串边界已定位并修正；最终冻结的完整局部门、23 oracle、库SHA和独审结果以交接回执为准，不能拿早期184/0且进程异常的记录冒充终态。

## 安装发布异常的历史定点回归（旧未隔离库）

独审确定性注入暴露初版6e5在安装提交后audit发生bad_alloc时返回失败却已安装，原红门14检查／3失败保留。修复把candidate的审计／返回值准备放在桥发布之前；同14条门随后通过。该注入不是自然内存耗尽事件，也不是生产库开关：只在测试进程对这一个候选dlopen移除DEEPBIND，使用候选自身C++运行库抛一次bad_alloc；不加载第二份C++运行库。正常加载不使用hook，控制组另核无注入行为。

```sh
c++ -std=c++17 -shared -fPIC -nostdlib -fno-exceptions \
  -I native/world/include tests/fixtures/native_inventory_bridge/fail_audit_once.cpp \
  -o "$AUDIT_HOOK"
HANMO_REVIEW_BRIDGE_LIBRARY="$FIXTURE/game/native_inventory_bridge/bin/libhanmo_world_bridge.so" \
LD_PRELOAD="$AUDIT_HOOK" "$GODOT" --headless --path "$FIXTURE/game" \
  --script res://tests/test_native_inventory_bridge.gd -- --install-exception-only
```

脚本要求注入确实发生、候选未发布、失败后查询NotInstalled、同对象重试成功及最后引用释放。缺hook会使该门失败，不能误报正常流程通过。该定点门、原字符串16及独立reviewer43相邻检查覆盖此修改；旧195/23属于原6e5，保留其真实身份而不冒用为新源码全门。


## 当前图形冷初始化与导出门

必须在已有真实图形会话运行；不创建Xvfb、不修改系统安全设置。shell缺DISPLAY时先实际查看桌面并核对共享工作目录，不凭shell变量断言没有GUI。`$CASES_ROOT`含原5组冻结`cases.json`、`cases/`与`cli_audits/`；`$COLD`必须不存在。

```sh
"$PYTHON" tests/fixtures/native_inventory_bridge/check_exports.py \
  "$BUILD/godot-bridge/libhanmo_world_bridge.so"
"$PYTHON" tests/fixtures/native_inventory_bridge/run_cold_workflow.py \
  --godot "$GODOT" --library "$BUILD/godot-bridge/libhanmo_world_bridge.so" \
  --cases-root "$CASES_ROOT" --source-commit "$SOURCE_COMMIT" \
  --output "$COLD" --mode x11
```

流程保留engine/library/fixture SHA、无`.godot`前态、原始导入/查询日志和真实退出码。首次导入使用普通`--editor --import`，随后独立进程只消费引擎生成的自动扩展列表；脚本不调用`load_extension`。`--mode headless`是保留失败的对照，失败后立即停，不改帧数、sleep或热缓存重试。调用方另行观察普通编辑器打开与正常退出，不能用截图替代进程终态。

## 当前符号隔离库的安装异常回归

旧`fail_audit_once.cpp`依赖动态可见C++符号；新库故意不再暴露这些符号，不能继续用未触发的LD_PRELOAD测试报绿。使用专用`wrap_audit_once.cpp`与GNU链接期`--wrap=_ZNK5hanmo19InventoryProjection5auditEv`生成独立故障库。仅此测试库加入wrapper；生产库绝不加入hook，也不去掉DEEPBIND。wrapper同静态运行库抛一次`bad_alloc`，未标记时调用真实audit。沿同一源码构建输出的原对象/静态库重新链接，仍保留`--exclude-libs,ALL`：

```sh
c++ -std=c++17 -fPIC -fvisibility=hidden -fvisibility-inlines-hidden \
  -I native/world/include -c tests/fixtures/native_inventory_bridge/wrap_audit_once.cpp \
  -o "$WRAP_OBJECT"
c++ -shared -fPIC -static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL \
  "$BUILD/godot-bridge/CMakeFiles/hanmo_world_bridge.dir/hanmo_world_bridge.cpp.o" \
  "$BUILD/godot-bridge/CMakeFiles/hanmo_world_bridge.dir/register_types.cpp.o" \
  "$BUILD/libhanmo_world.a" "$BUILD/bin/libgodot-cpp.linux.template_debug.x86_64.a" \
  "$BUILD/libhanmo_flecs.a" "$WRAP_OBJECT" \
  -Wl,--wrap=_ZNK5hanmo19InventoryProjection5auditEv -ldl -lm -o "$FAULT_LIBRARY"
```

把故障库放入独立的既有边界fixture副本，再运行原`test_native_inventory_bridge.gd -- --install-exception-only`。必须观察一次`INJECTION: link-wrapped audit bad_alloc once`、14/0与自然exit0；正常库另做无hook边界/字符串/查询/释放控制。该故障库不是生产交付物。


## 显式 legacy profile 与 Godot 纯值结果

新 `install_legacy_artifact(PackedByteArray)` 只接受 `HM-NATIVE-LEGACY-ARTIFACT/1`；旧 `install_artifact` 仍只收库存v1。一个对象任一安装成功后，两安装口均AlreadyInstalled。新 `query_actor_commitments(PackedByteArray)` 只在legacy实例可用；跨profile查询明确UnsupportedProjectionProfile。类型、状态、profile、限额与原同库错误的顺序见冻结合同（未公开）。

新结果format为 `HM-GODOT-ACTOR-COMMITMENTS-RESULT/1`，results是独立Godot Array/Dictionary树，和CLI的TaggedValue文本区分。复用同库唯一人物承诺组装；NIL/BOOL/INT64/字符串/数组/字典保持类型，所有深层字符串和key均复用NUL／UTF8往返门。任意迟条或深层转换失败拒绝整批，没有部分results，也不改变已安装实例。新audit添加profile及分域组件摘要；旧空／v1 audit和count String不变。

受验证的正式库基线f1c907d，SHA `991c5e09d20b1deb07b15741d839c32259ee888a1af7411d4fe342f9a65546f2`。最初07f的3个bad_variant_access RTTI泄露形成4导出红例；显式ELF导出表修后仅C入口。主headless221/0、20冻结工件60旧oracle，独立80/0；真实X11冷import自然exit0后自动加载同门221/0并正常退出。旧v1相邻43/0、字符串16/0；两份仅测试用链接异常库分别14/0与12/0，真实各注入一次，未将hook库作为发布库。

GUI日志保留VSync/portal/Vulkan探测及editor StringName诊断；普通editor看Help后退出仍报672 ObjectDB遗留，旧5d同环境也见同量，但不据此断言与新桥无关。新API自动查询进程exit0且无此ObjectDB诊断。headless冷editor原134未重跑、未标修复；不代表默认game、在线刷新、保存或完整M1完成。准备入口为 `tests/fixtures/native_actor_commitments_bridge/prepare_fixture.py`，GUI以 `--require-auto-loaded` 模式禁止手动load，必须真实冷导入后再运行。


## 按已审profile准备开发工程

`tools/native_growth_profile.py`共用固定Git、完整作者、资源、nonce存储及库身份检查；公共入口仍为`tools/build.py stage --profile ...`与`tools/verify.py --profile ...`。速度收益以同内容、同调用工作量的独立测量为准。

- `native-growth`固定991c/f1库、`HM-NATIVE-GROWTH-STAGE/1`和`native-growth-stage.json`；保持原成长开关及代表门。
- `native-travel`固定65ada219/62654ab库、`HM-NATIVE-TRAVEL-STAGE/1`和`native-travel-stage.json`；只开启原半日调度点的旅行provider，成长读取关闭。工具从`verification/native-travel-schedule-20261004.json`读取已审来源，不以本地编译候选回执代替。

```sh
python3 tools/build.py stage --profile native-travel --output NEW_OUTPUT --native-library LOCAL_REVIEWED_65ADA_LIBRARY
python3 tools/verify.py --profile native-travel --staged-project NEW_OUTPUT/game --engine FIXED_4_7_2 --output NEW_EVIDENCE
```

每个profile仍要求本次Git中的native源码/锁与它选中的编译来源一致；交叉库和任意同名so均拒绝。当前源码已经推进到旅行版时，旧growth/f1工程继续在它自己的固定来源下核验，工具不会更改旧991c记录或默默替库。默认不带profile的stage/test/export沿原入口。

旅行配置仅为`development/native_travel_schedule_expected_build`支持六个String字段Dictionary：profile、source_commit、source_sha256、core_source_sha256、api_sha256、dependency_lock_sha256。Godot单行/多行和键排序等价；重复、额外、缺项、坏类型和身份值变化失败。其余项目配置继续按唯一节/键及原值文本核验。

新输出使用自己的nonce/profile/custom_user_dir。stage只准备，正常GUI导入完成后verify才执行该profile的唯一代表门；外部旧native全套仍须显式选择及独立fixture。两份既有冻结工程保持原来源。有关受控资源复用由整合流程记录，工具不增加不受支持的CoW或目录别名入口。
