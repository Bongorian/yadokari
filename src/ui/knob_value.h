// UiRenderer member: one value formatter for full and cell presentations.
 struct KnobValue {char text[128]{},digits[24]{},unit[32]{};
  void append(const char* value){unsigned n=unsigned(__builtin_strlen(text));if(digits[0] && n && !unit[0])copy_text(unit,sizeof unit,value);if(n && n<126)text[n++]=' ';while(*value && n<127)text[n++]=*value++;text[n]=0;}
  void number(float value,unsigned precision){char number[24];SoftwareCanvas::format_value(number,value,precision);bool first=!text[0];append(number);if(first)copy_text(digits,sizeof digits,number);}
 };

 KnobValue formatted_knob(const ProjectState& p,const Performance& perf,unsigned i);
