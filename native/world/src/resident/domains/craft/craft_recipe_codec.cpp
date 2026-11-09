#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "craft_codec_archive.hpp"
namespace hanmo::resident {
Outcome<CraftRecipeCatalog> decode_craft_recipe_catalog(const BoundaryValue&input,const CommonCatalog&common,const ItemServiceCatalog&items){try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<CraftRecipeCatalog>();CraftRecipeCatalog out;craft_codec::decode(input,out,"/craft_recipe_catalog");auto checked=validate_craft_recipe_catalog(out,common,items);if(!checked.ok)return {false,{},checked.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure&e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_craft_recipe_catalog(const CraftRecipeCatalog&value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto status=emit_craft_recipe_catalog(writer,"",value);if(status.ok)status=writer.finish();if(!status.ok)return {false,{},status.error};return {true,std::move(out),{}};}
Status emit_craft_recipe_catalog(ProjectionWriter&writer,const StableId&name,const CraftRecipeCatalog&value){return craft_codec::emit(writer,name,value);}
Status visit_craft_recipe_catalog(CodecVisitor&visitor,CraftRecipeCatalog&catalog){try{return craft_codec::checkpoint(visitor,"craft_recipe_catalog",catalog);}catch(const exact_fields::CodecFailure&e){return {false,{},e.error};}}

}
