#pragma once
#include "performance.h"
namespace groove {
// UI/task context only. Engine owns immutable copies; resources are handed off
// before the UI mirror changes. This does not allocate inside audio rendering.
inline bool create_empty_session(Engine& engine,ProjectState& project,Performance& performance){auto* candidate=new ProjectState();make_empty_project(*candidate);WavView assets[kSampleAssets]{};bool ok=engine.stage_project(*candidate,assets);if(ok){project=*candidate;performance.project_loaded();performance.new_project_name();performance.notify(UiNotice::NewProject);}else performance.notify(UiNotice::Failed);delete candidate;return ok;}
}
