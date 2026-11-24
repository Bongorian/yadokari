#pragma once
// Included after the registry and the portable parameter target codec.
namespace groove {
inline bool valid_sound_route(const SoundModRoute& row,Instrument owner){
 return engine_target(row.parameter) && !unavailable_target(row.parameter) && target_engine(row.parameter)==owner &&
        macro_target_allowed(row.parameter) && row.source!=ModSource::Macro && unsigned(row.source)<=unsigned(ModSource::Velocity) &&
        unsigned(row.curve)<=unsigned(MacroCurve::Smooth) && valid_target_layer(row.parameter,row.mode,row.low) &&
        valid_target_layer(row.parameter,row.mode,row.high);
}
inline bool valid_sound_routes(const SoundModRoutes& routes,Instrument owner){
 if(routes.count>kSoundModRouteLimit)return false;
 for(unsigned i=0;i<routes.count;++i)if(!valid_sound_route(routes.rows[i],owner))return false;
 return true;
}
inline bool encode_sound_routes(ChunkBuffer& out,const EngineStateSource& s){
 if(!s.mod_routes || !valid_sound_routes(*s.mod_routes,s.owner))return false;
 ChunkWriter<ChunkBuffer> w(out);
 if(!w.u8(s.mod_routes->count))return false;
 for(unsigned i=0;i<s.mod_routes->count;++i){
  const auto& row=s.mod_routes->rows[i];
  if(!write_parameter_target(w,row.parameter) || !w.f32(row.low) || !w.f32(row.high) ||
     !w.u8(unsigned(row.mode)|(unsigned(row.curve)<<2)|(unsigned(row.source)<<4)))return false;
 }
 return true;
}
inline bool decode_sound_routes(ChunkSource& src,const EngineStateView& s){
 ChunkReader<ChunkSource> r(src);SoundModRoutes routes;
 if(!s.mod_routes || !r.u8(routes.count) || !routes.count || routes.count>kSoundModRouteLimit)return false;
 for(unsigned i=0;i<routes.count;++i){
  auto& row=routes.rows[i];uint8_t flags;
  if(!read_parameter_target(r,row.parameter) || !r.f32(row.low) || !r.f32(row.high) || !r.u8(flags))return false;
  row.mode=LayerMode(flags&3);row.curve=MacroCurve((flags>>2)&3);row.source=ModSource(flags>>4);
  if(!valid_sound_route(row,s.owner))return false;
 }
 if(!src.at_end())return false;
 *s.mod_routes=routes;return true;
}
}
