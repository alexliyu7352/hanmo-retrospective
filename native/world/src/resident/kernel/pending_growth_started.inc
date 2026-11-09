// 本叶只被完整命令捕获实现包含；原输入值由真实采用前读取并由同候选持有。
namespace {
// 保留字节全部走原分配器；仅旧只读API边界可临时转成普通字符串，绝不持有该临时值。
struct PendingText {
  using Storage=std::basic_string<char,std::char_traits<char>,CheckpointAllocator<char>>;
  Storage value;
  PendingText()=default;
  explicit PendingText(std::string_view text):value(text.data(),text.size()){}
  // 后续原请求身份复制仍进入同一追踪存储，不借调用者字符串寿命。
  PendingText& operator=(std::string_view text){value.assign(text.data(),text.size());return *this;}
  std::string_view view()const noexcept{return {value.data(),value.size()};}
  bool empty()const noexcept{return value.empty();}
  // 兼容只接受StableId的原接口；此副本只活到该次调用结束。
  operator StableId()const{return StableId(value.data(),value.size());}
  friend bool operator==(const PendingText& a,const PendingText& b)noexcept{return a.view()==b.view();}
  friend bool operator!=(const PendingText& a,const PendingText& b)noexcept{return !(a==b);}
  friend bool operator==(const PendingText& a,const StableId& b)noexcept{return a.view()==b;}
  friend bool operator==(const StableId& a,const PendingText& b)noexcept{return b==a;}
  friend bool operator!=(const PendingText& a,const StableId& b)noexcept{return !(a==b);}
  friend bool operator!=(const StableId& a,const PendingText& b)noexcept{return !(a==b);}
};
// 只存原来源三字段，不编码序列化文本或另造摘要身份。
struct PendingSourceCut {
  PendingText domain,kind,id;
  explicit PendingSourceCut(const SourceRef& source)
    :domain(source.domain),kind(source.kind),id(source.id){}
};
// 原空间schema与来源一同保留；域payload仍由原读取者验证。
struct PendingSpatialCut {
  PendingSourceCut source;
  Integer schema=0;
  explicit PendingSpatialCut(const ActualSpatialSourceRef& value):source(value.source),schema(value.schema){}
};
// 原Location有限六分支的无损副本；实际位置投影即使来源键未换也必须保持不变。
struct PendingLocationCut {
  static_assert(std::variant_size_v<Location> == 6,"原Location新增分支必须补全待绑定位置投影");
  std::size_t kind=0;
  PendingText first,second;
  Integer x=0,y=0,progress=0;
  CarriedLocation::CarrierKind carrier_kind=CarriedLocation::CarrierKind::transport_unit;
  explicit PendingLocationCut(const Location& value):kind(value.index()){
    std::visit([&](const auto& location){
      using T=std::decay_t<decltype(location)>;
      if constexpr(std::is_same_v<T,SettlementLocation>){first=location.settlement_id;second=location.place_id;}
      else if constexpr(std::is_same_v<T,WildernessLocation>){first=location.map_id;second=location.origin;x=location.point.x;y=location.point.y;}
      else if constexpr(std::is_same_v<T,FollowingLocation>)first=location.party_id;
      else if constexpr(std::is_same_v<T,InArmyLocation>)first=location.army_id;
      else if constexpr(std::is_same_v<T,OnRouteLocation>){first=location.edge_id;second=location.origin;progress=location.progress;}
      else {first=location.carrier_id;carrier_kind=location.carrier_kind;}
    },value);
  }
  // 各分支保留原字段与variant标签，不把路段进度或载体ID退化为单一地点字符串。
  bool operator==(const PendingLocationCut& other)const noexcept{
    return kind==other.kind&&first==other.first&&second==other.second&&x==other.x&&y==other.y&&
      progress==other.progress&&carrier_kind==other.carrier_kind;
  }
};
// 安静谓词与原完整来源序列均拥有追踪副本，不借短期安静读取者。
struct PendingQuietCut {
  bool quiet=false;
  CheckpointVector<PendingSourceCut> original_sources;
  explicit PendingQuietCut(const GrowthQuietView& value):quiet(value.quiet){
    original_sources.reserve(value.original_sources.size());
    for(const auto& source:value.original_sources)original_sources.emplace_back(source);
  }
};
// 原身体头仅有种子键与可选回执键；保留可选存在性，不以空字符串替代缺省。
struct PendingConditionHead {
  // 原种子仅保存稳定键，不附加未经原owner证明的身体字段。
  struct Seed {PendingText seed_key;explicit Seed(const InitialConditionSeedRef& ref):seed_key(ref.seed_key){}};
  // 原回执键仍与种子分开，可选存在性保留在外层头。
  struct Receipt {PendingText result_id;explicit Receipt(const ConditionReceiptRefV2& ref):result_id(ref.result_id){}};
  PendingText actor_id;
  Integer condition_revision=0;
  Seed initial_condition_seed_ref;
  std::optional<Receipt> latest_receipt_ref;
  explicit PendingConditionHead(const ActorConditionHead& head)
    :actor_id(head.actor_id),condition_revision(head.condition_revision),
     initial_condition_seed_ref(head.initial_condition_seed_ref){
    if(head.latest_receipt_ref)latest_receipt_ref.emplace(*head.latest_receipt_ref);
  }
};
// 永久能力变化是有限原variant的无损投影：两层tag、原ID与原课次/周期序数。
struct PendingCapabilityChange {
  static_assert(std::variant_size_v<GrowthCapabilityChangeRef> == 4&&
    std::variant_size_v<LifeGrowthContributionRef> == 7,"原能力来源新增分支必须补全待绑定来源投影");
  std::size_t kind=0,contribution_kind=0;
  PendingText id;
  Integer ordinal=0;
  explicit PendingCapabilityChange(const GrowthCapabilityChangeRef& value):kind(value.index()){
    std::visit([&](const auto& change){
      using T=std::decay_t<decltype(change)>;
      if constexpr(std::is_same_v<T,InitialConditionSeedRef>)id=change.seed_key;
      else if constexpr(std::is_same_v<T,ConditionReceiptRefV2>)id=change.result_id;
      else if constexpr(std::is_same_v<T,AttributeMilestoneRef>)id=change.milestone_result_id;
      else {
        contribution_kind=change.index();
        std::visit([&](const auto& contribution){
          using C=std::decay_t<decltype(contribution)>;
          if constexpr(std::is_same_v<C,GroupLessonResultRef>){id=contribution.course_id;ordinal=contribution.period_ordinal;}
          else if constexpr(std::is_same_v<C,PracticeAwardRef>)id=contribution.award_id;
          else if constexpr(std::is_same_v<C,AttributeMilestoneRef>)id=contribution.milestone_result_id;
          else {
            id=contribution.action_id;
            if constexpr(std::is_same_v<C,StudyLessonResultRef>||std::is_same_v<C,StudyPerformanceAdjustmentRef>)ordinal=contribution.lesson_ordinal;
          }
        },change);
      }
    },value);
  }
  // 不同原variant即使数字与ID相同也不相等。
  bool operator==(const PendingCapabilityChange& other)const noexcept{
    return kind==other.kind&&contribution_kind==other.contribution_kind&&id==other.id&&ordinal==other.ordinal;
  }
};
// 能力链原人物键、版号与最后变化同时持有；不从当前余额重建来源。
struct PendingCapabilityRevision {
  PendingText actor_id;
  Integer revision=0;
  PendingCapabilityChange last_change;
  explicit PendingCapabilityRevision(const GrowthCapabilityRevision& value)
    :actor_id(value.actor_id),revision(value.revision),last_change(value.last_change){}
};
// 原行动头的全部语义字段拥有追踪副本；参与者顺序和唯一性仍按原行动验证。
struct PendingActionCut {
  PendingText id,kind,actor_id;
  CheckpointVector<PendingText> participants;
  Integer start_tick=0,remaining_ticks=0;
  explicit PendingActionCut(const ActionCoreView& action)
    :id(action.id),kind(action.kind),actor_id(action.actor_id),
     start_tick(action.start_tick),remaining_ticks(action.remaining_ticks){
    participants.reserve(action.participants.size());
    for(const auto& actor:action.participants)participants.emplace_back(actor);
  }
  // 比较原行动头，不把资源监视失效改成重新接受当前行动。
  bool matches(const ActionCoreView& action)const noexcept{
    if(id!=action.id||kind!=action.kind||actor_id!=action.actor_id||start_tick!=action.start_tick||
       remaining_ticks!=action.remaining_ticks||participants.size()!=action.participants.size())return false;
    for(std::size_t n=0;n<participants.size();++n)if(participants[n]!=action.participants[n])return false;
    return true;
  }
};
// 比较原头的完整身份，不以今日数值或单一递增数字替代来源。
template<class A,class B>bool pending_head_equal(const A& a,const B& b) {
  return a.actor_id==b.actor_id && a.condition_revision==b.condition_revision &&
    a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key &&
    bool(a.latest_receipt_ref)==bool(b.latest_receipt_ref) &&
    (!a.latest_receipt_ref || a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);
}
// 所有比较均直接逐字段比原值，未编码或散列成新的许可键。
bool pending_source_equal(const PendingSourceCut& a,const PendingSourceCut& b) {
  return a.domain==b.domain && a.kind==b.kind && a.id==b.id;
}
bool pending_source_equal(const SourceRef& a,const PendingSourceCut& b) {
  return a.domain==b.domain && a.kind==b.kind && a.id==b.id;
}
// 固定标量槽覆盖原技能与属性，借扩展值避免额外复制其纯冷边界字段序。
std::array<Integer,21> pending_capability_values(const ActorRecord& a) {
  const auto& s=a.skills;const auto& v=a.attributes;
  const ActorExtendedSkills empty{};
  const auto& e=s.extended?*s.extended:empty;
  return {s.archery,s.cavalry,s.commerce,s.debate,s.infantry,s.martial,
    s.negotiation,s.rites,s.tactics,s.transport,e.naval,e.espionage,
    e.fortification,e.agriculture,e.medicine,e.forging,v.administration,
    v.charisma,v.intellect,v.leadership,v.martial};
}
}
// 此状态只活在原命令捕获内；每个阶段只能向前，脏见证不可替换重开。
struct GrowthPendingStartedProof::State : CheckpointAllocatedObject {
  // 每位实际参与者的独立只读切面；嵌套可变长值全部由追踪拥有者保管。
  struct ActorCut {
    PendingConditionHead condition;
    bool alive=false,available=false,extended=false;
    std::array<Integer,21> capability{};
    std::optional<PendingCapabilityRevision> permanent;
    CheckpointVector<PendingSpatialCut> spatial;
    std::optional<PendingLocationCut> location;
    std::optional<PendingQuietCut> quiet;
    // 在原头仍有效时复制最小完整头；其余所有拥有字段也使用追踪容器。
    explicit ActorCut(const ActorConditionHead& head):condition(head){}
  };
  enum class Stage {prospective,adopted,business,identified,consumed};
  WorldBundle& world;
  PendingActionCut action;
  WorldMoment at;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary adoption_boundary;
  CheckpointVector<ActorCut> actors;
  std::unique_ptr<CommonCaptureReadSetProof> resources,source;
  Stage stage=Stage::prospective;
  const WorldBundle* before=nullptr;
  Integer sequence=0;
  PendingText actor;
  // 保留原具名错误；reason仅分类真实失败候选，不生成成功结果。
  [[noreturn]] void reject(const char* reason) const {
    auto error=common_failure("GrowthInputOriginalStartRequired",action.id).error;
    error.reason=reason;error.actor_id=static_cast<StableId>(action.actor_id);
    throw Failure{std::move(error)};
  }
  // 每次都重借实际同M原owner，永不保留短期reader指针。
  template<class F> void with_body(F fn) const {
    const auto now=take(world.actual_phase_moment());
    if(!same(now,at,{}))reject("pending_start_moment_changed");
    std::unique_ptr<PhaseCurrentReadScope> phase;
    if(!world.has_active_read_phase())phase=take(world.enter_phase_read_context(
      "growth_pending_original_cut","growth",{now,now},now));
    auto owner=take(world.complete_exact_time_bindings());
    if(!owner)fail("CompleteGrowthOwningLeaseRequired");
    auto body=take(owner->reads().character_conditions.sources());
    if(&body.common!=&world.common || body.growth!=&world.growth ||
       !world.actor_foundation || &body.state!=&*world.actor_foundation ||
       !body.fixed_spatial || !same(body.actual_now,at,{}))
      fail("CompleteGrowthCandidateBindingMismatch");
    fn(body);
  }
  // 原身体链、技能/属性、永久能力链与实际空间/安静来源都复制为不可变读集。
  ActorCut read_actor(const CharacterConditionSources& body,const StableId& id) const {
    WorldCurrentMomentRead clock(world);
    const auto* head=take(read_verified_current_character_condition_head(
      body,world.common,*world.actor_foundation,id,clock));
    if(!head)fail("GrowthPendingOriginalConditionHeadRequired");
    take(read_character_condition_at_head(body,*head,at));
    const auto* row=world.common.state().actors.find(id);
    if(!row)fail("GrowthPendingOriginalActorRequired");
    ActorCut result(*head);result.alive=row->alive;
    result.available=row->available;result.extended=bool(row->skills.extended);
    result.capability=pending_capability_values(*row);
    if(body.growth_catalog && body.growth_catalog->complete_capability_application_chain) {
      const auto* current=body.permanent_growth?body.permanent_growth->capability_revisions.find(id):nullptr;
      if(!current)fail("GrowthCapabilityInitialRevisionRequired");
      result.permanent.emplace(*current);
    }
    take(validate_growth_fixed_spatial_source_moment(*body.fixed_spatial,at));
    const auto actual=take(read_actual_actor_spatial_location(world.common,
      body.fixed_spatial->movement,body.fixed_spatial->original,
      body.fixed_spatial->battle,id));
    result.location.emplace(actual.location_snapshot);
    if(actual.actual_location_sources.empty())fail("GrowthFixedActualLocationSourceRequired");
    result.spatial.reserve(actual.actual_location_sources.size());
    for(const auto& ref:actual.actual_location_sources){
      if(take(actor_spatial_history_source_schema(ref.source))!=ref.schema)
        fail("GrowthFixedActualLocationSourceRequired");
      result.spatial.emplace_back(ref);
    }
    if(world.catalog->actor_social_occupancy_selected) {
      if(!body.fixed_quiet || !body.fixed_quiet->bound_to(world.common))
        fail("GrowthQuietInputSameCaptureRequired");
      const auto quiet=take(body.fixed_quiet->read_actual(id,at));
      result.quiet.emplace(quiet);
    }
    return result;
  }
  // 仅在真实采用前建立原切面，重复身份或重复参与者都拒绝整候选。
  State(WorldBundle& w,const ActionCoreView& a):world(w),action(a),
      at(take(w.actual_phase_moment())),capture(w.common.actor_condition_capture_identity()),
      adoption_boundary(take(w.common.actor_condition_savepoint_boundary())) {
    if(!capture || action.id.empty() || action.participants.empty())
      fail("GrowthPendingActualCaptureRequired");
    if(w.common.action(a.id)||w.common.state().facts.find(a.id)||
       w.growth.actions.find(a.id)||w.growth.facts.find(a.id)||w.growth.life_activities.find(a.id))
      reject("pending_start_not_new");
    for(const auto& row:w.growth.fixed_service_boundaries.rows())
      if(row.second.action_ref==a.id)reject("pending_start_not_new");
    with_body([&](const CharacterConditionSources& body){
      for(const auto& id:action.participants) {
        for(const auto& prior:actors)if(prior.condition.actor_id==id)
          reject("pending_start_participant_mismatch");
        actors.push_back(read_actor(body,id));
      }
    });
  }
  // 身份监视先于内容比较；恢复相同值不能复活已撤销的人物/行动/业务来源。
  void validate(bool allow_started=false) const {
    if(capture!=world.common.actor_condition_capture_identity())
      reject("pending_start_capture_lifetime_changed");
    for(const auto* proof:{resources.get(),source.get()})if(proof) {
      auto valid=proof->validate(world.common);
      if(!valid.ok) {
        if(valid.error.reason)reject(valid.error.reason->c_str());
        throw Failure{std::move(valid.error)};
      }
    }
    if(stage!=Stage::prospective) {
      const auto* actual=world.common.action(action.id);
      if(!actual || !action.matches(*actual))
        reject("pending_start_action_lifetime_changed");
      for(const auto& id:action.participants) {
        const auto* row=world.common.actor(id);
        if(!row||row->action_id!=action.id)reject("pending_start_actor_written");
      }
    }
    if(!allow_started)for(const auto& row:world.growth.fixed_service_boundaries.rows())
      if(row.second.action_ref==action.id)reject("pending_start_unexpected_binding");
    with_body([&](const CharacterConditionSources& body){
      for(const auto& old:actors) {
        const auto current=read_actor(body,old.condition.actor_id);
        if(!pending_head_equal(old.condition,current.condition)||old.alive!=current.alive||old.available!=current.available)
          reject("pending_start_condition_changed");
        if(old.extended!=current.extended||old.capability!=current.capability ||
           bool(old.permanent)!=bool(current.permanent) ||
           (old.permanent && (old.permanent->actor_id!=current.permanent->actor_id ||
            old.permanent->revision!=current.permanent->revision ||
            !(old.permanent->last_change==current.permanent->last_change))))
          reject("pending_start_capability_changed");
        if(!old.location||!current.location||!(*old.location==*current.location)||
           old.spatial.size()!=current.spatial.size())reject("pending_start_spatial_changed");
        for(std::size_t i=0;i<old.spatial.size();++i)
          if(old.spatial[i].schema!=current.spatial[i].schema ||
             !pending_source_equal(old.spatial[i].source,current.spatial[i].source))
            reject("pending_start_spatial_changed");
        if(bool(old.quiet)!=bool(current.quiet))reject("pending_start_quiet_changed");
        if(old.quiet) {
          if(old.quiet->quiet!=current.quiet->quiet ||
             old.quiet->original_sources.size()!=current.quiet->original_sources.size())
            reject("pending_start_quiet_changed");
          for(std::size_t i=0;i<old.quiet->original_sources.size();++i)
            if(!pending_source_equal(old.quiet->original_sources[i],current.quiet->original_sources[i]))
              reject("pending_start_quiet_changed");
        }
      }
    });
  }
};
// 私有构造只接管同捕获状态，不允许消费者制造或替换待绑定证明。
GrowthPendingStartedProof::GrowthPendingStartedProof(std::unique_ptr<State> state):state_(std::move(state)){}
// 证明结束时释放全部追踪切面与通用观察者，不延长原世界或读取者寿命。
GrowthPendingStartedProof::~GrowthPendingStartedProof()=default;
// 读口没有新建能力；只接受拥有者已联接的同候选原行动。
Outcome<bool> GrowthPendingStartedProof::permits(const CommonStore& common,
    const GrowthState& growth,const CharacterConditionSources& body,const StableId& id) const {
  return guard<bool>([&]{
    if(!state_ || id!=state_->action.id || state_->stage==State::Stage::consumed)return false;
    if(&common!=&state_->world.common || &growth!=&state_->world.growth ||
       &body.common!=&common || body.growth!=&growth ||
       &body.state!=&*state_->world.actor_foundation)
      state_->reject("pending_start_candidate_mismatch");
    if(state_->stage!=State::Stage::identified)state_->reject("pending_start_not_identified");
    state_->validate();return true;
  });
}
