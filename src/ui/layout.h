#pragma once
namespace groove { namespace ui {
// One immutable viewport shared by Desktop/Circle. Temporary layers cover it.
struct Layout {
 static constexpr int width=480,height=272,body_y=64,legend_h=30;
 static constexpr int legend_y=height-legend_h,body_h=legend_y-body_y;
 static constexpr int overlay_y=224,overlay_h=46;
 static constexpr int tracker_y=88,tracker_pitch=16;
 static constexpr int tracker_rows=(legend_y-tracker_y)/tracker_pitch;
};
}}
