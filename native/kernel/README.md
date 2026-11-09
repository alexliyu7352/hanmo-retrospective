# 动态世界内核

设计见 docs/native/KERNEL_DESIGN.md（未公开）。本目录现在只有接口，还没有实现。

| 文件 | 内容 |
|---|---|
| `include/hanmo_kernel/types.hpp` | 编号、时刻、引用、拒绝原因、随机数 |
| `include/hanmo_kernel/world.hpp` | 世界状态的表：行动者、群体、东西、地方、关系、持续过程；真值视图 |
| `include/hanmo_kernel/knowledge.hpp` | 信息网：所知条目、记忆、舆论、只读所知的查询口 |
| `include/hanmo_kernel/action.hpp` | 基本动作：24 个内核动词、说话内容、提交与预览的结果 |
| `include/hanmo_kernel/fact.hpp` | 因果网：事实、保存级别、两种起因、因果查询 |
| `include/hanmo_kernel/mind.hpp` | 心智、路由、决策记录、反射 |
| `include/hanmo_kernel/package.hpp` | 玩法包：清单、登记、处理函数能做的事、观察、存档段、检查 |
| `include/hanmo_kernel/kernel.hpp` | 内核本体：装包、开局、提交、推进、存读、统计 |
| `include/hanmo_kernel/c_api.h` | C 接口：给沙盒（Python ctypes）和 Godot 桥接用 |

接口只由总设计改。语法检查：`g++ -std=c++17 -fsyntax-only -Wall -Wextra -Iinclude tests/syntax_check.cpp`。
