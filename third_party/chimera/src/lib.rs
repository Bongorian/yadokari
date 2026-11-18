#![no_std]
#![allow(dead_code)] // The imported voice modules also expose upstream analysis APIs.
// Human voice DSP from Jameson Hodge's CHIMERA, commit 3bd3047ca7490a6f21facbc29e4ace923bad1a2c.
extern crate alloc;

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
use anatomy::human::{ArticKey, ArticPatch, BODY_MALE, NEUTRAL};
use core::ptr;
use core::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use voice::human_voice::{HumanVoice, SourcePatch};

const ARENA_BYTES: usize = 512 * 1024;
const GEOMETRY_POINTS: usize = 32;
const REGIONS: usize = 16;

// Construction is serialized. Each host owns its arena and releases it in one
// operation. The live voice, including geometry updates, allocates nothing.
struct ArenaAllocator;
static BUILDING: AtomicBool = AtomicBool::new(false);
static BASE: AtomicUsize = AtomicUsize::new(0);
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
        if used > ARENA_BYTES {
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
pub struct Geometry {
    outer: [Point; GEOMETRY_POINTS],
    inner: [Point; GEOMETRY_POINTS],
    tongue: [Point; GEOMETRY_POINTS],
    nasal: [Point; GEOMETRY_POINTS],
    glottis: Point,
    section_inner: [Point; REGIONS],
    section_outer: [Point; REGIONS],
    pressure: [f32; REGIONS],
    gate: f32,
}
const _: () = assert!(core::mem::size_of::<Geometry>() == 1356);

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_new(arena: *mut u8, bytes: usize) -> *mut HumanVoice {
    if arena.is_null() || bytes < ARENA_BYTES {
        return ptr::null_mut();
    }
    while BUILDING.compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed).is_err() {
        core::hint::spin_loop();
    }
    USED.store(0, Ordering::Relaxed);
    BASE.store(arena as usize, Ordering::Relaxed);
    let voice = Box::into_raw(Box::new(HumanVoice::new(32000.0, BODY_MALE, 2)));
    BASE.store(0, Ordering::Relaxed);
    BUILDING.store(false, Ordering::Release);
    voice
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn yadokari_chimera_controls(voice: &mut HumanVoice, hz: f32, gate: f32, params: &[f32; 5]) {
    let [tongue_x, tongue_y, jaw, lips, breath] = params.map(f64::from);
    let mut articulation = ArticPatch::default();
    articulation
        .set(ArticKey::TbX, NEUTRAL.tb_x + (tongue_x - 0.5) * 4.0)
        .set(ArticKey::TbY, NEUTRAL.tb_y + (tongue_y - 0.5) * 3.0)
        .set(ArticKey::Jaw, 0.25 + jaw * 1.7)
        .set(ArticKey::LipAp, 0.22 + (1.0 - lips) * 0.65)
        .set(ArticKey::LipProt, lips * 0.9)
        .set(ArticKey::LipWidth, 1.0 - lips);
    voice.set_articulation(&articulation, None);
    let source = SourcePatch {
        f0: Some(f64::from(hz).clamp(16.0, 4000.0)),
        glide: Some(0.012),
        pressure: Some(4.5 + breath * 7.0),
        breathiness: Some(0.05 + breath * 0.28),
        ..SourcePatch::default()
    };
    voice.set_source(&source);
    voice.set_gate(f64::from(gate).clamp(0.0, 1.0));
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_sample(voice: &mut HumanVoice) -> f32 {
    let mut out = [0.0];
    voice.render(&mut out);
    out[0]
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_reset(voice: &mut HumanVoice) {
    voice.reset_airflow();
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
pub extern "C" fn yadokari_chimera_geometry(voice: &HumanVoice, out: &mut Geometry) {
    let Some(frame) = &voice.airway.frame else { return };
    copy_points(&mut out.outer, &frame.outer);
    copy_points(&mut out.inner, &frame.inner);
    copy_points(&mut out.tongue, &frame.tongue);
    copy_points(&mut out.nasal, &frame.nasal_cavity);
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
