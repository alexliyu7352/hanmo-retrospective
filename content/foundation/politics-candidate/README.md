青川政治作者候选

qingchuan-politics.delta.json 只针对 GitOID 11d59ec0ce5198f50571ee173e8afd8cc08d95ea 的 author-qualified 索引内部 catalog；它是待中央整合的新作者增量，不是可安装新包。24 项明确copy/add/replace操作新增明确政治 Grant、空缺辖务 Office、同一现有公库的辖区登记、政治/财政能力及中文职责。原采购 Grant、人物、家庭、地点、初始钱粮全部保持原引用。

新增的政治授权必须作为新作者规则审阅，不能说原仓务权限天然允许任官和税政。县仓书佐为目录已有青川县署 leader，新增封闭政治 Grant 只覆盖原青川街口与原公库；目标官职到任前空缺，不给开局玩家职权。初态没有税政记录，启用财政也不默认征收。玩家先沿真实具名入会，再接到任用，自行接受/拒绝，实际到治所才得限定授权；可以依法查收支并登记下一半日起税政。

NPC staffing profile 为显式候选，由中央 Director 接入同一调度器；这里没有写共享 Director 字段或添加另一时钟。原生玩家/NPC身份互斥仍保留。24半日答复/到任窗及最低行政属性1为本候选开发配置，单次资源额度直接沿原 grant.city 的1000；这些不是新宣称已定的历史平衡，发布前应沿产品试玩调整。

本域 schema 与两项语义检查均通过，证据在 docs/native/system-political-career/author-candidate-evidence.json。完整 prepared catalog、原生世界创建/存读、NPC实际采用、UI体验仍须在中央注册后跑。旧1.11和既有银行不改写。

另提供同一现有县仓书佐的可选扮演入口，保留47岁真人、原公职/责任及外貌。新情景默认仍程禾，不复制人物或增加私人钱粮；这供现任职责与S5生产管理共用同一正式世界来源，并不使原基层玩家自动获得公权。

为保留原2人v1情景，增量先复制为新society::scenario.qingchuan_political_career，再只在新情景启用selected_existing_actor_roster_v1；Native自动登记selected_existing_actor/2，不额外添加人物选择capability。原society::scenario.qingchuan_spring_reserve逐字段保持。检查器tools/content/politics_candidate.py调用S4实际名单/外貌检查器，避免只过本域schema就误报可建局。
