# 隔离Bridge局部门

完整构建／调用命令及headless冷首自动发现的剩余阻碍见 [native/godot/README.md](../../../native/godot/README.md)。本fixture只消费现成真实采购轻工程、冻结5组23条旧oracle、旧CLI组件摘要及当前共享库。

准备器先逐字节核冻结artifact/query/oracle，复制原core并比较全部152份GDScript；不补钱、不换目录、不重新生成oracle。纯GD导入后才显式attach已核共享库，分别写 preparation.json 与 attachment.json；后者明确记录editor首发现仍BLOCKED。测试中的原Kernel始终存活并在结束时检查state/RNG/revision和完整三元组未变。

`editor_discovery_probe.cpp` 为无领域依赖的保留反例：使用当前CMake生成include与已核godot-cpp静态库编译为单独.so，`-DNO_PROBE_CLASS` 为无类控制。对每个variant建立全新空project.godot和同入口的.gdextension，执行固定编辑器 `--headless --editor --import` 并保进程码。该诊断不接主游戏，不能把已有.godot缓存后的0冒充首次成功。原CPP、每个二进制、空工程和原始日志均在本片本地evidence保全。

主门验证真实ClassDB／RefCounted、23条原oracle、精确SHA、完整owner/lot组件与次序、坏安装后重试、二次安装、错误query后恢复、无输入/返回值别名、独立对象及最后引用释放。PackedByteArray类型门、原始非法UTF8/NUL、深64/65、64MiB/+1均在真实Godot调用边界运行。合法native的不可表示字符串使用原ASCII JSON转义构造，覆盖query_id、错误路径和审计库存/lot ID，明确结构性拒绝而非截断成功。

`fail_audit_once.cpp` 是Linux/GNU固定ABI的专用故障工具，不编入生产库。`--install-exception-only` 使用它验证安装返回准备异常的原子性；详细编译／调用命令在Bridge README。原6e5的确定性14/3红与修复14/0绿分别保留，正常43条相邻控制在相同hook下不设置注入标记。不能把加载器测试拦截用于正式导入，也不能拿它替代正常未hook的字符串/加载观察。


2026-10-04新增真实图形冷流程：`run_cold_workflow.py`只在全新目录导入，再以自动扩展发现执行`query_lifecycle.gd`的5组23旧oracle、组件摘要、坏查询恢复与最后引用释放。`check_exports.py`约束Linux桥唯一C入口，防静态C++运行库/Flecs符号污染宿主。`wrap_audit_once.cpp`是隔离符号后的链接期专用故障库方案，替代不再可用的动态符号hook；从不编入生产目标。图形首导入可用不代表headless首导入已修复，原失败保留。
