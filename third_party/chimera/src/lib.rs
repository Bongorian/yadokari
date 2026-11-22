#![no_std]
#![allow(dead_code)] // The imported voice modules also expose upstream analysis APIs.
// Human voice DSP from Jameson Hodge's CHIMERA, commit 3bd3047ca7490a6f21facbc29e4ace923bad1a2c.
extern crate alloc;

mod creatures;
mod ipa;
mod math;
mod physics;
mod airway {
    pub mod decimator;
    pub mod network;
}
mod anatomy {
    pub mod geom;
    pub mod human;
}
mod sources {
    pub mod glottis;
    pub mod syrinx;
    pub mod syrinx_table;
    pub mod valves;
}
mod control {
    pub mod articulators;
    pub mod score;
}
mod voice {
    pub mod human_airway;
    pub mod human_voice;
}

use alloc::alloc::{GlobalAlloc, Layout};
use alloc::boxed::Box;
use anatomy::geom::Vec2;
use anatomy::human::{ArticKey, ArticPatch, BODY_CHILD, BODY_FEMALE, BODY_MALE, NEUTRAL, rest_tip};
use control::score::{ControlKey, Gesture, Targets};
use core::ptr;
use creatures::CreatureVoice;
use core::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use voice::human_voice::{HumanVoice, SourcePatch};

const ARENA_BYTES: usize = 512 * 1024;
const DETAIL_COUNT: usize = 23;
const GEOMETRY_POINTS: usize = 32;
const REGIONS: usize = 16;
const ANIMAL_TUBES: usize = 5;
const ANIMAL_SECTIONS: usize = 16;

// Construction is serialized. Each host owns its arena and releases it in one
// operation. The live voice, including geometry updates, allocates nothing.
struct ArenaAllocator;
static BUILDING: AtomicBool = AtomicBool::new(false);
static BASE: AtomicUsize = AtomicUsize::new(0);
static LIMIT: AtomicUsize = AtomicUsize::new(0);
static USED: AtomicUsize = AtomicUsize::new(0);

unsafe impl GlobalAlloc for ArenaAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let base = BASE.load(Ordering::Relaxed);
        if base == 0 {
            return ptr::null_mut();
        }
        let tail = base + USED.load(Ordering::Relaxed);
        let aligned = (tail + layout.align() - 1) & !(layout.align() - 1);
        let used = aligned - base + layout.size();
        if used > LIMIT.load(Ordering::Relaxed) {
            return ptr::null_mut();
        }
        USED.store(used, Ordering::Relaxed);
        aligned as *mut u8
    }

    unsafe fn dealloc(&self, _: *mut u8, _: Layout) {}
}

#[global_allocator]
static ALLOCATOR: ArenaAllocator = ArenaAllocator;

#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
    terminate()
}

fn terminate() -> ! {
    #[cfg(target_os = "none")]
    unsafe { core::arch::asm!("brk #0", options(noreturn)) }
    #[cfg(not(target_os = "none"))]
    {
        unsafe extern "C" { fn abort() -> !; }
        unsafe { abort() }
    }
}

// The prebuilt hosted compiler builtins carry this reference even when the
// voice is compiled with panic=abort. No unwind can cross the C ABI.
#[cfg(not(target_os = "none"))]
#[unsafe(no_mangle)]
pub extern "C" fn rust_eh_personality() -> ! {
    terminate()
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Point {
    x: f32,
    y: f32,
}
impl From<Vec2> for Point {
    fn from(point: Vec2) -> Self {
        Self { x: point.x as f32, y: point.y as f32 }
    }
}

#[repr(C)]
pub struct AnimalTube {
    name: [u8; 12],
    length: f32,
    radius: [f32; ANIMAL_SECTIONS],
    pressure: [f32; ANIMAL_SECTIONS],
}

pub enum Dsp {
    Human(Box<HumanVoice>),
    Creature(Box<CreatureVoice>),
}

#[repr(C)]
pub struct Geometry {
    outer: [Point; GEOMETRY_POINTS],
    inner: [Point; GEOMETRY_POINTS],
    tongue: [Point; GEOMETRY_POINTS],
    nasal: [Point; GEOMETRY_POINTS],
    velum: [Point; GEOMETRY_POINTS],
    glottis: Point,
    section_inner: [Point; REGIONS],
    section_outer: [Point; REGIONS],
    pressure: [f32; REGIONS],
    gate: f32,
    model: u32,
    animal: [AnimalTube; ANIMAL_TUBES],
}
const _: () = assert!(core::mem::size_of::<Geometry>() == 2336);

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_new(arena: *mut u8, bytes: usize, body: u32, wait: bool) -> *mut Dsp {
    if arena.is_null() || bytes < ARENA_BYTES || body > 6 {
        return ptr::null_mut();
    }
    while BUILDING.compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed).is_err() {
        if !wait { return ptr::null_mut(); }
        core::hint::spin_loop();
    }
    USED.store(0, Ordering::Relaxed);
    LIMIT.store(ARENA_BYTES, Ordering::Relaxed);
    BASE.store(arena as usize, Ordering::Relaxed);
    let voice = if let Some(model) = [BODY_MALE, BODY_FEMALE, BODY_CHILD].get(body as usize) {
        Dsp::Human(Box::new(HumanVoice::new(32000.0, *model, 2)))
    } else {
        Dsp::Creature(Box::new(CreatureVoice::new(body)))
    };
    let voice = Box::into_raw(Box::new(voice));
    BASE.store(0, Ordering::Relaxed);
    BUILDING.store(false, Ordering::Release);
    voice
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_controls(
    model: &mut Dsp, hz: f32, gate: f32, params: &[f32; 5], detail: &[f32; DETAIL_COUNT], score: bool,
) {
    let voice = match model {
        Dsp::Human(voice) => voice,
        Dsp::Creature(voice) => { voice.controls(hz, gate, params); return; }
    };
    let [tongue_x, tongue_y, jaw, lips, breath] = params.map(f64::from);
    let [_, depth, rate, glide, pressure, breathiness, tip_x, tip_y, tip_curl, root,
        lip_ap, lip_prot, lip_width, labiodental, velum, larynx, epilarynx,
        ventricular, lateral, dorsum, _, _, _] = detail.map(f64::from);
    let jaw = 0.25 + jaw * 1.7;
    let tongue_x = NEUTRAL.tb_x + (tongue_x - 0.5) * 4.0;
    let tongue_y = NEUTRAL.tb_y + (tongue_y - 0.5) * 3.0;
    let tip = rest_tip(jaw, tongue_x, tongue_y);
    let manual_tip = tip_x != 0.0 || tip_y != 0.0;
    let mut articulation = ArticPatch::default();
    articulation
        .set(ArticKey::TbX, tongue_x)
        .set(ArticKey::TbY, tongue_y)
        .set(ArticKey::Jaw, jaw)
        .set(ArticKey::LipAp, 0.22 + (1.0 - lips) * 0.65 + lip_ap)
        .set(ArticKey::LipProt, lips * 0.9 + lip_prot)
        .set(ArticKey::LipWidth, 1.0 - lips + lip_width)
        .set(ArticKey::TtCurl, tip_curl)
        .set(ArticKey::Root, root)
        .set(ArticKey::Labiodental, labiodental)
        .set(ArticKey::Velum, if velum == f64::from(-0.15f32) { NEUTRAL.velum } else { velum })
        .set(ArticKey::Larynx, larynx)
        .set(ArticKey::Epilarynx, epilarynx)
        .set(ArticKey::Ventricular, ventricular)
        .set(ArticKey::Lateral, lateral)
        .set(ArticKey::Dorsum, dorsum);
    if manual_tip || score {
        articulation.set(ArticKey::TtX, tip.x + tip_x).set(ArticKey::TtY, tip.y + tip_y);
    }
    voice.set_articulation(&articulation, Some(!manual_tip && !score));
    let source = SourcePatch {
        f0: Some(f64::from(hz).clamp(16.0, 4000.0)),
        glide: Some(glide * 0.001),
        pressure: Some((4.5 + breath * 7.0 + pressure).clamp(0.5, 20.0)),
        breathiness: Some((0.05 + breath * 0.28 + breathiness).clamp(0.0, 1.0)),
        vibrato_depth: Some(depth),
        vibrato_rate: Some(rate),
        ..SourcePatch::default()
    };
    voice.set_source(&source);
    voice.set_gate(f64::from(gate).clamp(0.0, 1.0));
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_hit(model: &mut Dsp, note: u32, velocity: f32) -> u32 {
    let Dsp::Human(voice) = model else { return 0 };
    voice.cancel_score();
    voice.reset_airflow();
    let level = f64::from(velocity).clamp(0.0, 1.0);
    let t = voice.clock + 0.001;
    let (name, close, end, mouth, burst, source) = match note % 4 {
        0 => ("Kick", 0.026, 0.105,
            Targets::default().with(ArticKey::LipAp, -0.2).with(ArticKey::Jaw, 0.4),
            Targets::default().with(ArticKey::LipAp, 0.6).with(ArticKey::LipProt, 0.9),
            Targets::default().with(ControlKey::F0, 65.0).with(ControlKey::BREATH, 1.3 * level)),
        1 => ("Snare", 0.022, 0.112,
            Targets::default().with(ArticKey::LipAp, -0.18),
            Targets::default().with(ArticKey::Labiodental, 0.85).with(ArticKey::LipAp, 0.28),
            Targets::default().with(ControlKey::ABDUCTION, 0.48).with(ControlKey::BREATH, 1.5 * level)),
        hit => ("Hat", 0.014, if hit == 2 { 0.049 } else { 0.195 },
            Targets::default().with(ArticKey::TtCurl, 0.85).with(ArticKey::Jaw, 0.4),
            Targets::default().with(ArticKey::TtCurl, 0.27).with(ArticKey::LipAp, 0.32),
            Targets::default().with(ControlKey::ABDUCTION, 0.4).with(ControlKey::BREATH, 1.15 * level)),
    };
    let seal = Targets::default().with(ControlKey::CLOSURE, if note % 4 == 0 { 0.8 } else { 0.25 });
    voice.schedule(&[
        Gesture::new(t, t + close, 0.003, 0.005, 32.0, mouth).speed(4.0).label(name),
        Gesture::new(t, t + close, 0.006, 0.008, 8.0, seal),
        Gesture::new(t + close, t + end, 0.004, 0.019, 12.0, burst).speed(2.5).label(name),
        Gesture::new(t + 0.003, t + end, 0.006, 0.021, 4.0, source),
    ]);
    ((end + 0.05) * 48000.0) as u32
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_sample(model: &mut Dsp, score_time: f32) -> f32 {
    let voice = match model {
        Dsp::Human(voice) => voice,
        Dsp::Creature(voice) => return voice.sample(),
    };
    if score_time >= 0.0 { voice.score_time = Some(f64::from(score_time)); }
    let mut out = [0.0];
    voice.render(&mut out);
    out[0]
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_reset(model: &mut Dsp) {
    match model {
        Dsp::Human(voice) => { voice.cancel_score(); voice.reset_airflow(); }
        Dsp::Creature(voice) => voice.reset(),
    }
}

fn copy_points(to: &mut [Point; GEOMETRY_POINTS], from: &[Vec2]) {
    if from.is_empty() {
        return;
    }
    for (index, point) in to.iter_mut().enumerate() {
        *point = from[index * (from.len() - 1) / (GEOMETRY_POINTS - 1)].into();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_tongue(model: &Dsp, xy: &mut [f32; 2]) -> bool {
    let Dsp::Human(voice) = model else { return false };
    let pose = &voice.artic.x;
    xy[0] = ((pose.tb_x - NEUTRAL.tb_x) * 0.25 + 0.5).clamp(0.0, 1.0) as f32;
    xy[1] = ((pose.tb_y - NEUTRAL.tb_y) / 3.0 + 0.5).clamp(0.0, 1.0) as f32;
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_geometry(model: &Dsp, out: &mut Geometry) {
    let voice = match model {
        Dsp::Human(voice) => voice,
        Dsp::Creature(voice) => {
            for tube in &mut out.animal { tube.name[0] = 0; }
            voice.geometry(out);
            return;
        }
    };
    out.model = 0;
    let Some(frame) = &voice.airway.frame else { return };
    copy_points(&mut out.outer, &frame.outer);
    copy_points(&mut out.inner, &frame.inner);
    copy_points(&mut out.tongue, &frame.tongue);
    copy_points(&mut out.nasal, &frame.nasal_cavity);
    copy_points(&mut out.velum, &frame.velum);
    out.glottis = frame.glottis.into();
    for (i, pressure) in out.pressure.iter_mut().enumerate() {
        if !frame.grid.is_empty() {
            let section = &frame.grid[i * (frame.grid.len() - 1) / (REGIONS - 1)];
            out.section_inner[i] = section.inner.into();
            out.section_outer[i] = section.outer.into();
        }
        let airway = &voice.airway;
        let point = i * (airway.n_ph + airway.n_or - 1) / (REGIONS - 1);
        let node = if point < airway.n_ph {
            airway.pharynx.first_node() + point
        } else {
            airway.oral.first_node() + point - airway.n_ph
        };
        let value = airway.net.p[node];
        *pressure = if value.is_finite() { (value as f32 / 800.0).clamp(-1.0, 1.0) } else { 0.0 };
    }
    out.gate = voice.gate as f32;
}
