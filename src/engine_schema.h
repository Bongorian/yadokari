#pragma once
#include "engine_registry.h"
namespace groove {
// Read-only editor schema. Classification does not gate existing hardware UI.
// Engine declarations are followed by the unique shared controls in CORE.
struct EngineSchemaParameter {
 EngineParameterReference reference{};const ParameterDescriptor* value=nullptr;
 EngineParameterUi ui{};EngineDisplay display=EngineDisplay::Native;
 const char* const* choices=nullptr;unsigned choice_count=0;
};
inline unsigned engine_schema_count(Instrument engine){
 const auto* d=engine_descriptor(unsigned(engine));if(!d)return 0;unsigned n=d->parameters.count;
 for(unsigned i=0;i<8;++i)if(d->page.core[i].scope==EngineParameterScope::Track){bool seen=false;for(unsigned j=0;j<i;++j)if(d->page.core[j].scope==EngineParameterScope::Track && d->page.core[j].index==d->page.core[i].index)seen=true;if(!seen)++n;}
 return n;
}
inline EngineSchemaParameter engine_schema_parameter(Instrument engine,unsigned index){
 const auto* d=engine_descriptor(unsigned(engine));if(!d)return {};
 if(index<d->parameters.count){const auto& p=d->parameters.declarations[index];return {engine_control(index),&p.value,p.ui,p.display,p.choices,p.choice_count};}
 index-=d->parameters.count;
 for(unsigned i=0;i<8;++i)if(d->page.core[i].scope==EngineParameterScope::Track){bool seen=false;for(unsigned j=0;j<i;++j)if(d->page.core[j].scope==EngineParameterScope::Track && d->page.core[j].index==d->page.core[i].index)seen=true;if(seen)continue;if(index--==0){auto* p=parameter_descriptor(d->page.core[i].index);return {d->page.core[i],p,{EngineEditTier::Performance,declared_parameter_type(*p),"Track"}};}}
 return {};
}
}
