//! Weddell seal: male trills sweep down over seconds from several kHz to below 100 Hz while their
//! pulse rate also slows — researchers compared them to ARP synthesizers. Modelled as a laryngeal
//! source with an extreme pitch range, a pulse (amplitude) oscillator, and a short tract.

use super::build::{CMH2O, fill_areas};
use super::mammal::{MammalAirway, MammalSpec, mammal_airway, mammal_noise, step_glottis};
use super::CreatureSim;
use alloc::{boxed::Box, vec, vec::Vec};
use crate::airway::network::{AirwayNetwork, Tube};
use crate::math::{PI, sin};

const PITCH: usize = 0;
const PULSE_RATE: usize = 1;
const PULSE: usize = 2;
const DUTY: usize = 3;
const BRIGHT: usize = 4;
const ROUGH: usize = 5;
const PRESSURE: usize = 6;
const MOUTH: usize = 7;

pub struct Seal {
    a: MammalAirway,
    tubes: Vec<(&'static str, Tube)>,
    pulse_phase: f64,
    pulse_gate: f64,
}

pub fn create(fs: f64) -> Box<dyn CreatureSim> {
    let mut a = mammal_airway(fs, MammalSpec { trachea: (24.0, 4.0), pharynx: (11.0, 3.2), oral: (12.0, 3.6), nasal: Some((12.0, 1.5)), wall: None, wall_radiation: Some(0.6), coarse: None, nasal_radiates: None }, |_, _| {});
    let g = &mut a.glottis_src;
    g.params.jitter = 0.002;
    g.params.shimmer = 0.01;
    g.params.drift = 2.0;
    let tubes = vec![("trachea", a.trachea), ("pharynx", a.pharynx), ("oral", a.oral)];
    Box::new(Seal { a, tubes, pulse_phase: 0.0, pulse_gate: 1.0 })
}

impl CreatureSim for Seal {
    fn net(&self) -> Option<&AirwayNetwork> {
        Some(&self.a.net)
    }
    fn net_mut(&mut self) -> Option<&mut AirwayNetwork> {
        Some(&mut self.a.net)
    }
    fn tubes(&self) -> &[(&'static str, Tube)] {
        &self.tubes
    }
    fn gain(&self) -> f64 {
        6.4e-8
    }
    fn control(&mut self, p: &[f64], gate: f64, dt: f64) {
        let a = &mut self.a;
        self.pulse_phase += p[PULSE_RATE] * dt;
        self.pulse_phase -= libm::floor(self.pulse_phase);
        let ph = if self.pulse_phase < p[DUTY] { sin((PI * self.pulse_phase) / p[DUTY]) } else { 0.0 };
        self.pulse_gate = 1.0 - p[PULSE] + p[PULSE] * ph;
        a.net.set_fixed_pressure(a.lungs, p[PRESSURE] * CMH2O * gate * self.pulse_gate);
        let g = &mut a.glottis_src.params;
        g.f0 = p[PITCH];
        g.amplitude = 0.22;
        g.open_quotient = 0.55 - 0.25 * p[BRIGHT];
        g.closure_sharpness = 0.4 + 0.6 * p[BRIGHT];
        g.effort = 0.5 + 0.5 * p[BRIGHT];
        g.chink = 0.01;
        g.chaos = p[ROUGH];
        let mouth = p[MOUTH];
        let ph_areas = fill_areas(a.pharynx, |_| 3.0, &mut a.phar_area);
        a.net.set_tube_targets(a.pharynx, ph_areas, None, None);
        let or_areas = fill_areas(a.oral, |u| if u > 0.8 { 0.3 + 4.0 * mouth } else { 3.6 }, &mut a.oral_area);
        a.net.set_tube_targets(a.oral, or_areas, None, None);
        mammal_noise(a, 2e-6, None);
    }
    #[inline]
    fn sample(&mut self) -> f64 {
        step_glottis(&mut self.a);
        self.a.net.step()
    }
    fn flush_tiny(&mut self) {
        self.a.glottis_src.flush_tiny();
    }
}
