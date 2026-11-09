// S2 private signed spatial leaf. It uses the sole canonical fraction codec;
// it is not another rational/time implementation or a MovementAnchor branch.
#pragma once
#include "military_spatial_chart.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
namespace hanmo::resident::military_spatial_point_fields {
template<class C>Status value(C& c,SignedSpatialRational& v){
 c.field("sign",v.sign);
 c.object("magnitude",[&](auto& x){return exact_fields::fraction(x,v.magnitude);});
 if(!c.status().ok)return c.status();
 if(v.sign < -1||v.sign > 1)return c.reject("InvalidMilitarySpatialSign");
 {
  const auto magnitude=exact_fields::take(encode_canonical_fraction(v.magnitude));
  if((magnitude.numerator=="0")!=(v.sign==0))return c.reject("NonCanonicalMilitarySpatialZero");
 }
 return c.status();
}
template<class C>Status value(C& c,MilitarySpatialPoint& v){
 c.object("x",[&](auto& x){return value(x,v.x);});
 c.object("y",[&](auto& x){return value(x,v.y);});return c.status();
}
}
