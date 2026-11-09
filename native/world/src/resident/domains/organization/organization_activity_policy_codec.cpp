#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "organization_activity_policy_codec_fields.hpp"
namespace hanmo::resident {
Status visit_organization_activity_policy(CodecVisitor& codec,OrganizationActivityPolicy& value){return organization_activity_policy_fields::fields(codec,value);}
Status emit_organization_activity_policy_projection(ProjectionWriter& out,std::string_view name,const OrganizationActivityPolicy& value){return emit_projection_record<ActorFoundationProjectionArchive>(out,name,value,[](auto& c,auto& v){return organization_activity_policy_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_organization_activity_policy(const OrganizationActivityPolicy& value){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_organization_activity_policy_projection(writer,"",value);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<OrganizationActivityPolicy> decode_organization_activity_policy(const BoundaryValue& input,const CatalogIdentity& identity,const OrganizationActivityPolicyOriginalRead& original,const OrganizationActivityPolicyLimitRead& technical){OrganizationActivityPolicy out;ActorFoundationJsonVisitor reader(input);auto valid=organization_activity_policy_fields::fields(reader,out);if(valid.ok)valid=reader.finish();if(valid.ok)valid=validate_organization_activity_policy(out,identity,original,technical);return {valid.ok,std::move(out),valid.error};}
}
