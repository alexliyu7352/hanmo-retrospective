#include "military_spatial_chart.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident { using namespace movement_detail;
namespace {
using Q=SignedSpatialRational;using P=MilitarySpatialPoint;
Q canonical(Q a,ExactArithmeticBudget b){if(compare(a.magnitude,{},b)==0)a.sign=0;return a;}
Q neg(Q a,ExactArithmeticBudget b){if(compare(a.magnitude,{},b)!=0)a.sign=-a.sign;return a;}
Q plus(const Q& a,const Q& c,ExactArithmeticBudget b){if(a.sign==c.sign)return canonical({a.sign,add(a.magnitude,c.magnitude,b)},b);int cmp=compare(a.magnitude,c.magnitude,b);return cmp>=0?canonical({a.sign,sub(a.magnitude,c.magnitude,b)},b):canonical({c.sign,sub(c.magnitude,a.magnitude,b)},b);}
Q minus(const Q& a,const Q& c,ExactArithmeticBudget b){return plus(a,neg(c,b),b);}
Q times(const Q& a,const Q& c,ExactArithmeticBudget b){return canonical({((a.sign<0)!=(c.sign<0))?-1:1,mul(a.magnitude,c.magnitude,b)},b);}
Q quotient(const Q& a,const Q& c,ExactArithmeticBudget b){return canonical({((a.sign<0)!=(c.sign<0))?-1:1,div(a.magnitude,c.magnitude,b)},b);}
int cmp(const Q& a,const Q& c,ExactArithmeticBudget b){if((a.sign<0)!=(c.sign<0))return a.sign<0?-1:1;int v=compare(a.magnitude,c.magnitude,b);return a.sign<0?-v:v;}
P minus(const P& a,const P& c,ExactArithmeticBudget b){return {minus(a.x,c.x,b),minus(a.y,c.y,b)};}
Q cross(const P& a,const P& c,ExactArithmeticBudget b){return minus(times(a.x,c.y,b),times(a.y,c.x,b),b);}
Q dot(const P& a,const P& c,ExactArithmeticBudget b){return plus(times(a.x,c.x,b),times(a.y,c.y,b),b);}
Q det(const MilitarySpatialMatrix& a,ExactArithmeticBudget b){return minus(times(a.a11,a.a22,b),times(a.a12,a.a21,b),b);}
P product(const MilitarySpatialMatrix& m,const P& p,ExactArithmeticBudget b){return {plus(times(m.a11,p.x,b),times(m.a12,p.y,b),b),plus(times(m.a21,p.x,b),times(m.a22,p.y,b),b)};}
void valid(const Q& q,ExactArithmeticBudget b){take(encode_canonical_fraction(q.magnitude,b));if(q.sign < -1||q.sign>1||(q.sign==0)!=(compare(q.magnitude,{},b)==0))fail("NonCanonicalSpatialSign");}
void valid(const P& p,ExactArithmeticBudget b){valid(p.x,b);valid(p.y,b);}
bool equal(const P& a,const P& c,ExactArithmeticBudget b){return cmp(a.x,c.x,b)==0&&cmp(a.y,c.y,b)==0;}
bool on_segment(const P& point,const MilitarySpatialSegment& edge,ExactArithmeticBudget b){auto v=minus(edge.to,edge.from,b),w=minus(point,edge.from,b);if(cmp(cross(v,w,b),{},b)!=0)return false;auto projection=dot(v,w,b),length=dot(v,v,b);return cmp(projection,{},b)>=0&&cmp(projection,length,b)<=0;}
// One exact clipping implementation for a + rate*t <= 0 on the current
// closed parameter window. SAT, receiver shapes and domain containment share
// it; their different strict-contact/permission rules remain at the caller.
bool clip_nonpositive_linear_constraint(MilitaryContactParameterWindow& window,
 const Q& a,const Q& rate,ExactArithmeticBudget b){
 if(rate.sign==0)return a.sign<=0;
 if(rate.sign>0){
  if(a.sign>0)return false;
  const auto upper=div(a.magnitude,rate.magnitude,b);
  if(compare(upper,window.last,b)<0)window.last=upper;
 }else if(a.sign>0){
  const auto lower=div(a.magnitude,rate.magnitude,b);
  if(compare(lower,window.first,b)>0)window.first=lower;
 }
 return compare(window.first,window.last,b)<=0;
}
bool inside(const MilitarySpatialChart2& chart,const P& p,ExactArithmeticBudget b){int orientation=0;for(std::size_t i=0;i<chart.domain_polygon.size();++i){const auto& a=chart.domain_polygon[i];const auto& c=chart.domain_polygon[(i+1)%chart.domain_polygon.size()];int v=cmp(cross(minus(c,a,b),minus(p,a,b),b),{},b);if(v==0)continue;if(orientation&&v!=orientation)return false;orientation=v;}return true;}
}
Outcome<SignedSpatialRational> spatial_integer(Integer value,ExactArithmeticBudget b){return guard<Q>([&]{bool negative=value<0;std::uint64_t absolute=negative?std::uint64_t(-(value+1))+1:std::uint64_t(value);return Q{value==0?0:negative?-1:1,take(normalize_fraction(std::to_string(absolute),"1",b))};});}
Outcome<SignedSpatialRational> spatial_binary64(double value,ExactArithmeticBudget b){return guard<Q>([&]{if(!std::isfinite(value))fail("NonFiniteSpatialCoordinate");return canonical({value<0?-1:1,take(fraction_from_binary64(std::abs(value),b))},b);});}
Outcome<bool> military_spatial_points_equal(const P& a,const P& c,ExactArithmeticBudget b){return guard<bool>([&]{valid(a,b);valid(c,b);return equal(a,c,b);});}
Status validate_military_spatial_chart(const MilitarySpatialChart2& c,ExactArithmeticBudget b){return status([&]{if(c.rule_version!=2||c.id.empty()||c.session_ref.empty()||c.geometry_definition_ref.empty()||c.world_geometry_ref.empty()||c.world_length_unit_ref.empty()||c.local_axis_unit_ref.empty()||!source_valid(c.creation_source_ref)||c.domain_polygon.size()<3||c.portal_rows.empty())fail("MilitarySpatialChartSourceRequired",c.id);for(const auto* q:{&c.matrix.a11,&c.matrix.a12,&c.matrix.a21,&c.matrix.a22})valid(*q,b);valid(c.translation.b1,b);valid(c.translation.b2,b);if(cmp(det(c.matrix,b),{},b)==0)fail("SingularMilitarySpatialChart",c.id);const auto polygon=validate_military_convex_footprint(c.domain_polygon,b);if(!polygon.ok){if(polygon.error.original.code=="NonConvexMilitaryBodyShape")fail("NonConvexMilitarySpatialDomain",c.id);if(polygon.error.original.code=="MilitaryConvexBodyShapeRequired"||polygon.error.original.code=="DuplicateMilitaryBodyVertex"||polygon.error.original.code=="DegenerateMilitaryBodyShape")fail("DegenerateMilitarySpatialDomain",c.id);throw Failure{polygon.error};}std::set<StableId> portals;for(const auto& p:c.portal_rows){if(p.portal_id.empty()||!portals.insert(p.portal_id).second||p.world_entry_exit_source_kind.empty()||p.world_boundary_ref.empty()||p.connector_path_ref.empty()||p.allowed_carrier_kinds.empty()||equal(p.local_boundary_segment.from,p.local_boundary_segment.to,b))fail("InvalidMilitarySpatialPortal",p.portal_id);valid(p.local_boundary_segment.from,b);valid(p.local_boundary_segment.to,b);bool boundary=false;for(std::size_t i=0;i<c.domain_polygon.size();++i){MilitarySpatialSegment edge{c.domain_polygon[i],c.domain_polygon[(i+1)%c.domain_polygon.size()]};if(on_segment(p.local_boundary_segment.from,edge,b)&&on_segment(p.local_boundary_segment.to,edge,b))boundary=true;}if(!boundary||cmp(det(p.orientation_mapping,b),{},b)==0)fail("MilitaryPortalNotOnBoundary",p.portal_id);std::set<StableId> kinds;for(const auto& k:p.allowed_carrier_kinds)if((k!="actor"&&k!="party"&&k!="army"&&k!="fleet"&&k!="transport_unit")||!kinds.insert(k).second)fail("MilitaryPortalCarrierKind",p.portal_id);}});}
Outcome<P> military_chart_to_world(const MilitarySpatialChart2& c,const P& local,ExactArithmeticBudget b){return guard<P>([&]{take(validate_military_spatial_chart(c,b));valid(local,b);auto result=product(c.matrix,local,b);return P{plus(result.x,c.translation.b1,b),plus(result.y,c.translation.b2,b)};});}
Outcome<P> military_chart_vector_to_world(const MilitarySpatialChart2& c,const P& offset,ExactArithmeticBudget b){return guard<P>([&]{take(validate_military_spatial_chart(c,b));valid(offset,b);return product(c.matrix,offset,b);});}
Outcome<P> military_chart_to_local(const MilitarySpatialChart2& c,const P& world,ExactArithmeticBudget b){return guard<P>([&]{take(validate_military_spatial_chart(c,b));valid(world,b);auto p=minus(world,P{c.translation.b1,c.translation.b2},b);auto d=det(c.matrix,b);MilitarySpatialMatrix inverse{quotient(c.matrix.a22,d,b),quotient(neg(c.matrix.a12,b),d,b),quotient(neg(c.matrix.a21,b),d,b),quotient(c.matrix.a11,d,b)};return product(inverse,p,b);});}
Outcome<P> military_world_point(const MovementAnchor& anchor,ExactArithmeticBudget b){return guard<P>([&]{if(const auto* t=std::get_if<TerrainContinuousAnchor>(&anchor))return P{take(spatial_binary64(take(decode_movement_binary64(t->x_bits)),b)),take(spatial_binary64(take(decode_movement_binary64(t->y_bits)),b))};if(const auto* original=std::get_if<OriginalMovementAnchor>(&anchor))if(const auto* w=std::get_if<WildernessLocation>(&original->location))return P{take(spatial_integer(w->point.x,b)),take(spatial_integer(w->point.y,b))};fail("MilitaryWorldGeometryProjectionRequired");});}
Outcome<bool> military_point_on_portal(const MilitarySpatialPortal& p,const P& point,ExactArithmeticBudget b){return guard<bool>([&]{valid(point,b);return on_segment(point,p.local_boundary_segment,b);});}
Outcome<MilitaryConnectorWork> military_connector_work(const MilitarySpatialChart2& chart,const MilitaryEntryConnectorDefinition& path,const P& entry,ExactArithmeticBudget b){return guard<MilitaryConnectorWork>([&]{take(validate_military_spatial_chart(chart,b));if(path.id.empty()||path.chart_ref!=chart.id||path.path.empty()||path.entered_cell_costs.size()+1!=path.path.size()||!source_valid(path.original_path_source)||!equal(path.path.front(),entry,b))fail("MilitaryConnectorSourceMismatch",path.id);const auto portal=std::find_if(chart.portal_rows.begin(),chart.portal_rows.end(),[&](const auto& p){return p.portal_id==path.portal_id;});if(portal==chart.portal_rows.end()||portal->connector_path_ref!=path.id||!on_segment(entry,portal->local_boundary_segment,b))fail("MilitaryConnectorPortalMismatch",path.id);MilitaryConnectorWork out;out.connector_ref=path.id;for(std::size_t i=1;i<path.path.size();++i){if(!inside(chart,path.path[i],b)||path.entered_cell_costs[i-1]<1)fail("MilitaryConnectorTerrainSourceRequired",path.id);auto delta=minus(path.path[i],path.path[i-1],b);auto summed=plus(delta.x,delta.y,b);auto length=delta.x.magnitude;if(compare(delta.y.magnitude,length,b)>0)length=delta.y.magnitude;if(compare(summed.magnitude,length,b)>0)length=summed.magnitude;auto work=mul(length,integer(path.entered_cell_costs[i-1],b),b);out.segment_work.push_back(work);out.total=add(out.total,work,b);}return out;});}
Outcome<MilitarySpatialPoint> military_spatial_interpolate(const P& a,const P& c,const CanonicalFraction& u,ExactArithmeticBudget b){return guard<P>([&]{valid(a,b);valid(c,b);if(compare(u,integer(1,b),b)>0)fail("SpatialInterpolationOutsideSegment");auto delta=minus(c,a,b);Q factor{compare(u,{},b)==0?0:1,u};return P{plus(a.x,times(delta.x,factor,b),b),plus(a.y,times(delta.y,factor,b),b)};});}
Outcome<MilitarySpatialPoint> military_spatial_difference(const P& a,const P& c,ExactArithmeticBudget b){return guard<P>([&]{valid(a,b);valid(c,b);return minus(a,c,b);});}
Outcome<CanonicalFraction> military_axial_length(const P& a,const P& c,ExactArithmeticBudget b){return guard<CanonicalFraction>([&]{valid(a,b);valid(c,b);auto delta=minus(c,a,b);auto third=plus(delta.x,delta.y,b);auto length=delta.x.magnitude;if(compare(delta.y.magnitude,length,b)>0)length=delta.y.magnitude;if(compare(third.magnitude,length,b)>0)length=third.magnitude;return length;});}
Status validate_military_spatial_geometry_definition(const MilitarySpatialGeometryDefinition2& definition,ExactArithmeticBudget b){return status([&]{if(definition.id.empty()||!source_valid(definition.actual_definition_source))fail("MilitarySpatialDefinitionSourceRequired");auto check=[&](const MilitarySpatialMatrix& matrix,const MilitarySpatialTranslation& translation){MilitarySpatialChart2 chart{"definition","definition",definition.id,definition.world_geometry_ref,definition.world_length_unit_ref,definition.local_axis_unit_ref,matrix,translation,definition.domain_polygon,definition.portal_rows,definition.actual_definition_source,2};take(validate_military_spatial_chart(chart,b));};if(const auto* locked=std::get_if<LockedArenaChartDefinition>(&definition.author_mode))check(locked->matrix,locked->translation);else{const auto& choices=std::get<EncounterChartDefinition>(definition.author_mode).orientation_choices;if(choices.empty())fail("EncounterOrientationSourceRequired");std::set<StableId> ids;for(const auto& choice:choices){if(choice.id.empty()||!ids.insert(choice.id).second||choice.tangent_region.empty())fail("EncounterOrientationSourceRequired");valid(choice.local_entry,b);for(const auto& plane:choice.tangent_region){valid(plane.normal,b);valid(plane.maximum,b);if(equal(plane.normal,{},b))fail("DegenerateOrientationSelector");}check(choice.matrix,{});bool on_portal=false;for(const auto& portal:definition.portal_rows)if(on_segment(choice.local_entry,portal.local_boundary_segment,b))on_portal=true;if(!on_portal)fail("EncounterLocalEntryNotOnPortal");}}});}
Outcome<MilitarySpatialChart2> make_military_spatial_chart(const MilitarySpatialGeometryDefinition2& definition,const StableId& id,const StableId& session,const SourceRef& source,const P& world,const P& tangent,const StableId& portal_id,ExactArithmeticBudget b){return guard<MilitarySpatialChart2>([&]{take(validate_military_spatial_geometry_definition(definition,b));if(id.empty()||session.empty()||!source_valid(source))fail("MilitarySpatialChartSourceRequired");valid(world,b);valid(tangent,b);MilitarySpatialChart2 chart;chart.id=id;chart.session_ref=session;chart.geometry_definition_ref=definition.id;chart.world_geometry_ref=definition.world_geometry_ref;chart.world_length_unit_ref=definition.world_length_unit_ref;chart.local_axis_unit_ref=definition.local_axis_unit_ref;chart.domain_polygon=definition.domain_polygon;chart.portal_rows=definition.portal_rows;chart.creation_source_ref=source;
if(const auto* locked=std::get_if<LockedArenaChartDefinition>(&definition.author_mode)){chart.matrix=locked->matrix;chart.translation=locked->translation;}else{if(equal(tangent,{},b))fail("EncounterActualOrientationRequired");const MilitaryEncounterOrientation* chosen=nullptr;for(const auto& choice:std::get<EncounterChartDefinition>(definition.author_mode).orientation_choices){bool eligible=true;for(const auto& plane:choice.tangent_region)if(cmp(dot(plane.normal,tangent,b),plane.maximum,b)>0){eligible=false;break;}if(eligible){chosen=&choice;break;}}if(!chosen)fail("EncounterOrientationNotAuthored");chart.matrix=chosen->matrix;const auto projected=product(chart.matrix,chosen->local_entry,b);chart.translation={minus(world.x,projected.x,b),minus(world.y,projected.y,b)};}
take(validate_military_spatial_chart(chart,b));const auto portal=std::find_if(chart.portal_rows.begin(),chart.portal_rows.end(),[&](const auto& p){return p.portal_id==portal_id;});if(portal==chart.portal_rows.end())fail("MilitarySpatialPortalRequired",portal_id);const auto local=take(military_chart_to_local(chart,world,b));if(!on_segment(local,portal->local_boundary_segment,b)||!equal(take(military_chart_to_world(chart,local,b)),world,b))fail("MilitaryActualEntryPortalMismatch",portal_id);return chart;});}

Outcome<bool> military_spatial_point_inside(const MilitarySpatialChart2& chart,const P& p,ExactArithmeticBudget b){return guard<bool>([&]{take(validate_military_spatial_chart(chart,b));valid(p,b);return inside(chart,p,b);});}
Status validate_military_convex_footprint(const std::vector<P>& points,
 ExactArithmeticBudget b){return status([&]{
  if(points.size()<3)fail("MilitaryConvexBodyShapeRequired");
  for(std::size_t i=0;i<points.size();++i){valid(points[i],b);for(std::size_t j=0;j<i;++j)
   if(equal(points[i],points[j],b))fail("DuplicateMilitaryBodyVertex");}
  int winding=0;
  // Every edge must support the complete polygon. Checking consecutive turns
  // alone could admit a self-intersecting star with a uniform turn direction.
  for(std::size_t i=0;i<points.size();++i){
   const auto edge=minus(points[(i+1)%points.size()],points[i],b);int side=0;
   for(const auto& p:points){const auto current=cmp(cross(edge,minus(p,points[i],b),b),{},b);
    if(!current)continue;if(side&&current!=side)fail("NonConvexMilitaryBodyShape");side=current;}
   if(!side)fail("DegenerateMilitaryBodyShape");
   if(winding&&side!=winding)fail("NonConvexMilitaryBodyShape");winding=side;
  }
 });}
Outcome<std::vector<P>> orient_military_body_footprint(const std::vector<P>& points,
 Integer heading,ExactArithmeticBudget b){return guard<std::vector<P>>([&]{
 take(validate_military_convex_footprint(points,b));
 if(heading<0||heading>5)fail("MilitaryBodyHeadingOutsideOriginalDirections");
 std::vector<P> out;out.reserve(points.size());
 for(const auto& original:points){
  auto value=original;
  for(Integer turn=0;turn<heading;++turn){
   auto next_q=neg(value.y,b),next_r=plus(value.x,value.y,b);
   value={std::move(next_q),std::move(next_r)};
  }
  out.push_back(std::move(value));
 }
 return out;
});}
Outcome<MilitaryConvexBodyContactRead> intersect_military_convex_body_motion(
 const std::vector<P>& first,const MilitarySpatialSegment& first_centers,
 const std::vector<P>& second,const MilitarySpatialSegment& second_centers,
 ExactArithmeticBudget b){return guard<MilitaryConvexBodyContactRead>([&]{
 take(validate_military_convex_footprint(first,b));take(validate_military_convex_footprint(second,b));
 valid(first_centers.from,b);valid(first_centers.to,b);
 valid(second_centers.from,b);valid(second_centers.to,b);
 const auto relative=minus(first_centers.from,second_centers.from,b);
 const auto velocity=minus(minus(first_centers.to,first_centers.from,b),
                           minus(second_centers.to,second_centers.from,b),b);
 MilitaryContactParameterWindow window{{},integer(1,b)};
 bool initially_strict=true,constant_boundary=false,empty=false;
 auto constraint=[&](const Q& a,const Q& rate){
  if(a.sign>=0)initially_strict=false;
  if(rate.sign==0&&a.sign==0)constant_boundary=true;
  if(!clip_nonpositive_linear_constraint(window,a,rate,b))empty=true;
 };
 auto axes=[&](const std::vector<P>& points){
  for(std::size_t i=0;i<points.size();++i){
   const auto edge=minus(points[(i+1)%points.size()],points[i],b);
   const P normal{neg(edge.y,b),edge.x};
   auto first_min=dot(normal,first.front(),b),first_max=first_min;
   auto second_min=dot(normal,second.front(),b),second_max=second_min;
   for(const auto& p:first){const auto value=dot(normal,p,b);if(cmp(value,first_min,b)<0)first_min=value;if(cmp(value,first_max,b)>0)first_max=value;}
   for(const auto& p:second){const auto value=dot(normal,p,b);if(cmp(value,second_min,b)<0)second_min=value;if(cmp(value,second_max,b)>0)second_max=value;}
   const auto start=dot(normal,relative,b),rate=dot(normal,velocity,b);
   // Both projected intervals overlap iff lower <= center difference <= upper.
   const auto lower=minus(second_min,first_max,b),upper=minus(second_max,first_min,b);
   constraint(minus(start,upper,b),rate);
   constraint(minus(lower,start,b),neg(rate,b));
  }
 };
 axes(first);axes(second);MilitaryConvexBodyContactRead out;
 if(empty)return out;
 out.closed_contact=window;out.initially_penetrating=initially_strict;
 if(!constant_boundary&&compare(window.first,window.last,b)<0)out.first_inward_contact=window.first;
 return out;
});}
Outcome<std::optional<MilitaryContactParameterWindow>> intersect_military_receiver_window(const P& from,const P& to,const std::vector<MilitaryReceiverHalfspace>& planes,ExactArithmeticBudget b){return guard<std::optional<MilitaryContactParameterWindow>>([&]()->std::optional<MilitaryContactParameterWindow>{valid(from,b);valid(to,b);if(planes.empty())fail("MilitaryReceiverShapeRequired");MilitaryContactParameterWindow window{{},integer(1,b)};const auto velocity=minus(to,from,b);for(const auto& plane:planes){valid(plane.normal,b);valid(plane.maximum,b);if(equal(plane.normal,{},b)||plane.maximum.sign<0)fail("InvalidMilitaryReceiverHalfspace");const auto a=minus(dot(plane.normal,from,b),plane.maximum,b),rate=dot(plane.normal,velocity,b);if(!clip_nonpositive_linear_constraint(window,a,rate,b))return {};}return window;});}

Outcome<MilitaryConvexDomainMotionRead> intersect_military_convex_domain_motion(
 const std::vector<P>& footprint,const MilitarySpatialSegment& centers,
 const std::vector<P>& domain,ExactArithmeticBudget b){return guard<MilitaryConvexDomainMotionRead>([&]{
 take(validate_military_convex_footprint(footprint,b));
 take(validate_military_convex_footprint(domain,b));valid(centers.from,b);valid(centers.to,b);
 const auto velocity=minus(centers.to,centers.from,b);
 MilitaryContactParameterWindow window{{},integer(1,b)};
 MilitaryConvexDomainMotionRead out;bool empty=false;
 for(std::size_t n=0;n<domain.size();++n){
  const auto& anchor=domain[n];const auto edge=minus(domain[(n+1)%domain.size()],anchor,b);
  int winding=0;
  for(const auto& point:domain){winding=cmp(cross(edge,minus(point,anchor,b),b),{},b);if(winding)break;}
  // The convex validator already proved one nonzero interior side per edge.
  const P outward=winding>0?P{edge.y,neg(edge.x,b)}:P{neg(edge.y,b),edge.x};
  const auto rate=dot(outward,velocity,b);
  for(const auto& offset:footprint){
   const P start{plus(centers.from.x,offset.x,b),plus(centers.from.y,offset.y,b)};
   const auto distance=dot(outward,minus(start,anchor,b),b);
   if(distance.sign>0)out.initially_outside=true;
   if(!clip_nonpositive_linear_constraint(window,distance,rate,b))empty=true;
  }
 }
 if(empty)return out;
 out.closed_inside_window=window;
 // Merely touching at the final endpoint has no proved outside suffix in this
 // source-valid piece. Starting on a boundary and moving inward/parallel also
 // produces no new outward gate. No epsilon or extended future rate is used.
 if(!out.initially_outside&&compare(window.last,integer(1,b),b)<0)
  out.first_outward_contact=window.last;
 return out;
});}
Outcome<Integer> military_spatial_direction(const P& from,const P& to,ExactArithmeticBudget b){return guard<Integer>([&]{
 valid(from,b);valid(to,b);const auto delta=minus(to,from,b);if(equal(delta,{},b))fail("CoincidentMilitaryBodiesHaveNoRay");
 static constexpr Integer directions[6][2]={{1,0},{0,1},{-1,1},{-1,0},{0,-1},{1,-1}};
 Integer selected=0;Q best;for(Integer i=0;i<6;++i){const auto q=minus(take(spatial_integer(directions[i][0],b)),delta.x,b),r=minus(take(spatial_integer(directions[i][1],b)),delta.y,b),sum=plus(q,r,b);const auto squared=plus(plus(times(q,q,b),times(r,r,b),b),times(sum,sum,b),b);if(i==0||cmp(squared,best,b)<0){best=squared;selected=i;}}
 return selected;
});}
Outcome<MilitaryHexSupercoverRead> military_hex_supercover(const P& from,const P& to,const std::vector<HexPoint>& cells,ExactArithmeticBudget b){return guard<MilitaryHexSupercoverRead>([&]{
 valid(from,b);valid(to,b);if(cells.empty()||cells.size()>1024)fail("InvalidHexCells");
 // Voronoi closed hexes in the original axial metric: 2q+r<=1,
 // q+2r<=1, -q+r<=1 and their opposites, relative to each cell center.
 static constexpr Integer normals[6][2]={{2,1},{1,2},{-1,1},{-2,-1},{-1,-2},{1,-1}};
 std::vector<MilitaryReceiverHalfspace> planes;planes.reserve(6);for(const auto& n:normals)planes.push_back({{take(spatial_integer(n[0],b)),take(spatial_integer(n[1],b))},take(spatial_integer(1,b))});
 MilitaryHexSupercoverRead result;std::set<std::pair<Integer,Integer>> seen;
 for(const auto& cell:cells){if(cell.q < -1024||cell.q>1024||cell.r < -1024||cell.r>1024||!seen.emplace(cell.q,cell.r).second)fail("InvalidHexCell");const P center{take(spatial_integer(cell.q,b)),take(spatial_integer(cell.r,b))};auto window=take(intersect_military_receiver_window(minus(from,center,b),minus(to,center,b),planes,b));if(window)result.intersections.push_back({cell,*window});}
 std::sort(result.intersections.begin(),result.intersections.end(),[&](const auto& a,const auto& z){const auto first=compare(a.parameter_window.first,z.parameter_window.first,b);if(first)return first<0;const auto last=compare(a.parameter_window.last,z.parameter_window.last,b);if(last)return last<0;return a.cell.q==z.cell.q?a.cell.r<z.cell.r:a.cell.q<z.cell.q;});
 CanonicalFraction covered;bool began=false;for(const auto& hit:result.intersections){if(!began){if(compare(hit.parameter_window.first,{},b)!=0)break;began=true;}else if(compare(hit.parameter_window.first,covered,b)>0)break;if(compare(hit.parameter_window.last,covered,b)>0)covered=hit.parameter_window.last;if(compare(covered,integer(1,b),b)==0){result.complete_board_coverage=true;break;}}
 return result;
});}

}
