// The JSON reference and no-tree validator consume one real world component route.
#pragma once
#include "resident/checkpoint/projection_composition_index.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
enum class WorldExportPart { common, split, root_fields, section, fact_payloads, action_payloads, population_actions, validation_only };
class WorldExportComposition {
public:
    virtual ~WorldExportComposition()=default;
    template<class Emit>Status whole(WorldExportPart part,std::string_view domain,Emit&& emit){
        using F=std::remove_reference_t<Emit>;
        return append(part,domain,[](ProjectionWriter& writer,const void* call)->Status{return (*static_cast<const F*>(call))(writer);},&emit);
    }
    template<class Emit>Status fields(WorldExportPart part,std::string_view domain,Emit&& emit){
        return whole(part,domain,[&](ProjectionWriter& writer){return writer.object("",emit);});
    }
    virtual bool has_action_payload(const StableId&)const=0;
    virtual bool has_fact_payload(const StableId&)const=0;
    virtual void adopt_fact_domain(const StableId&,const StableId&)=0;
    virtual void adopt_fact_payload(const StableId&)=0;
    virtual Status adopt_action_payload(const StableId&,bool original_empty)=0;
    virtual Status finish_battles()=0;
    virtual Status finish()=0;
protected:
    using EmitCall=Status(*)(ProjectionWriter&,const void*);
    virtual Status append(WorldExportPart,std::string_view,EmitCall,const void*)=0;
};
class TreeWorldExportComposition final:public WorldExportComposition {
public:
    explicit TreeWorldExportComposition(const CommonState&);
    bool has_action_payload(const StableId&)const override;
    bool has_fact_payload(const StableId&)const override;
    void adopt_fact_domain(const StableId&,const StableId&)override;
    void adopt_fact_payload(const StableId&)override;
    Status adopt_action_payload(const StableId&,bool)override;
    Status finish_battles()override;
    Status finish()override;
    BoundaryValue take()&&;
private:
    const CommonState& common_;
    BoundaryValue output_,params_,facts_,battles_;
    Status append(WorldExportPart,std::string_view,EmitCall,const void*)override;
};
class ShapeWorldExportComposition final:public WorldExportComposition {
public:
    ShapeWorldExportComposition(const CommonState&,const CapacityProfile&,CheckpointJobBudget&,std::shared_ptr<CheckpointMemoryResource>);
    bool has_action_payload(const StableId&)const override;
    bool has_fact_payload(const StableId&)const override;
    void adopt_fact_domain(const StableId&,const StableId&)override;
    void adopt_fact_payload(const StableId&)override;
    Status adopt_action_payload(const StableId&,bool)override;
    Status finish_battles()override;
    Status finish()override;
private:
    struct ActionShape {
        const StableId* id;ProjectionKeyIndex fields;
        ActionShape(const StableId& key,std::shared_ptr<CheckpointMemoryResource> memory):id(&key),fields(std::move(memory)){}
        ActionShape(ActionShape&&)noexcept=default;
        ActionShape& operator=(ActionShape&&)noexcept=default;
    };
    class Observation;
    const CommonState& common_;const CapacityProfile& capacity_;CheckpointJobBudget& job_;
    std::shared_ptr<CheckpointMemoryResource> memory_;
    ProjectionKeyIndex roots_,params_,facts_,battles_;
    CheckpointVector<ActionShape> actions_;
    Status append(WorldExportPart,std::string_view,EmitCall,const void*)override;
};
}
