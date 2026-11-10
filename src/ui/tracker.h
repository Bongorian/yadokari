// UiRenderer member: the same phrase, eight track columns across both displays.
 void tracker(const ProjectState& p,const Performance& perf){
  const int rows=ui::Layout::tracker_rows,mid=rows/2;
  unsigned first=perf.selected_track()/8*8;
  for(unsigned column=0;column<8;++column){unsigned side=column/4,x=10+(column%4)*116,ti=first+column;const auto t=track_at(p,p.active_pattern,ti);bool selected=ti==perf.selected_track(),audio=p.audio[ti].kind!=TrackKind::Instrument;bool playing=perf.playing() && p.clips[ti].playing && p.clips[ti].pattern==p.active_pattern && !audio;
   label(side,int(x),67,"T",theme.secondary,Font::Small,12);value(side,int(x)+10,67,20,ti+1,selected?theme.teal:theme.text,Font::Small,0);label(side,int(x)+43,67,audio?"AUDIO":playing?"POS":"LEN",theme.secondary,Font::Small,38);if(!audio)value(side,int(x)+77,67,30,playing?perf.position(ti)+1:t.length,playing?theme.teal:theme.secondary,Font::Small,0);
   if(selected)fill(side,int(x),86,112,ui::Layout::legend_y-86,theme.surface);
   bool running=perf.playing() && perf.follow_display() && p.clips[ti].playing && p.clips[ti].pattern==p.active_pattern && !audio;unsigned center=running?perf.position(ti):perf.selected_step();
   for(int row=0;row<rows;++row){unsigned step=tracker_row(center,row-mid,t.length);int y=88+row*16;bool focus=selected && step==perf.selected_step();if(step%4==0)fill(side,int(x),y-2,112,15,theme.surface);if(row==mid)rounded(side,int(x),y-2,112,15,2,focus?theme.raised:theme.surface);if(focus)selected_mark(side,int(x),y-2,112,15);
    char text[20]{};text[0]=char('0'+(step+1)/100);text[1]=char('0'+(step+1)/10%10);text[2]=char('0'+(step+1)%10);text[3]=' ';unsigned vel=0;bool enabled=false,hidden=false;
    if(audio){__builtin_memcpy(text+4,"AUDIO",5);}
    else if(t.instrument==Instrument::DrumKit){hidden=tracker_hidden(t.steps[step]);unsigned hits=0;for(unsigned pad=0;pad<16;++pad){const auto& s=t.kit.steps[pad][step];if(s.enabled){++hits;if(s.velocity>vel)vel=s.velocity;}hidden|=tracker_hidden(s);}enabled=hits>0;text[4]=char('0'+hits/10);text[5]=char('0'+hits%10);text[6]='H';text[7]=' ';}
    else{const auto& s=t.steps[step];enabled=s.enabled;vel=s.velocity;hidden=tracker_hidden(s);if(enabled){static const char* names[]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};unsigned note=s.note;text[4]=names[note%12][0];text[5]=names[note%12][1];int octave=int(note/12)-1;text[6]=octave<0?'-':char('0'+octave);text[7]=octave<0?'1':' ';}else __builtin_memcpy(text+4,"--- ",4);}
    if(!audio && t.instrument!=Instrument::DrumKit){const auto& s=t.steps[step];step_attributes(side,int(x)+64,y+2,s.slide(),s.accent(),enabled);}
    if(!audio){text[8]=' ';text[9]=char('0'+vel/100);text[10]=char('0'+vel/10%10);text[11]=char('0'+vel%10);text[12]=hidden?'*':' ';}
    char number[4]={text[0],text[1],text[2],0},pitch[5]={text[4],text[5],text[6],text[7],0},velocity[5]={text[9],text[10],text[11],text[12],0};
    label(side,int(x)+3,y,number,theme.secondary,Font::Small,25,true);
    label(side,int(x)+31,y,audio?"AUDIO":pitch,enabled?theme.text:theme.secondary,Font::Small,39,true);
    if(!audio){label(side,int(x)+75,y,enabled?velocity:"---",enabled?theme.text:theme.disabled,Font::Small,36,true);if(enabled)progress(side,int(x)+75,y+13,34,2,float(vel)/127,focus?theme.value_edit:theme.secondary);}
    if(perf.playing() && p.clips[ti].playing && p.clips[ti].pattern==p.active_pattern && step==perf.position(ti))fill(side,int(x)+2,y-2,108,3,theme.motion);
   }
  }
 }
