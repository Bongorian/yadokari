//! CHIMERA Bestiary DSP from eeb0c1940d2d52f5f5effdecf3507fe79157f389.

mod alligator;
mod build;
mod elephant;
mod mammal;
mod seal;
mod songbird;

use alloc::boxed::Box;
use crate::airway::{decimator::Decimator, network::{AirwayNetwork, Tube, tiny}};
use crate::math::{PI, exp, sqrt, tanh};
use crate::{Geometry, ANIMAL_TUBES, ANIMAL_SECTIONS};

trait CreatureSim: Send {
    fn net(&self) -> Option<&AirwayNetwork>;
    fn net_mut(&mut self) -> Option<&mut AirwayNetwork>;
    fn tubes(&self) -> &[(&'static str, Tube)];
    fn control(&mut self, p: &[f64], gate: f64, dt: f64);
    fn sample(&mut self) -> f64;
    fn gain(&self) -> f64;
    fn flush_tiny(&mut self) {}
}

const OUTPUT_RATE: f64 = 32000.0;
const CONTROL_PERIOD: usize = 64;

pub struct CreatureVoice {
    kind: u32,
    sim: Box<dyn CreatureSim>,
    target: [f64; 9],
    value: [f64; 9],
    gate: f64,
    goal: f64,
    dec: Decimator,
    ctrl: usize,
    dc_x: f64,
    dc_y: f64,
    dc_coef: f64,
}

impl CreatureVoice {
    pub fn new(kind: u32) -> Self {
        let mut params = [0.0; 9];
        let (factor, create): (usize, fn(f64) -> Box<dyn CreatureSim>) = match kind {
            3 => {
                params = [2600.0, 7.0, 0.5, 0.35, 0.4, 0.9, 0.8, 0.4, 0.55];
                (4, songbird::create)
            }
            4 => {
                params[..8].copy_from_slice(&[900.0, 18.0, 0.6, 0.55, 0.6, 0.05, 10.0, 0.35]);
                (2, seal::create)
            }
            5 => {
                params[..6].copy_from_slice(&[28.0, 0.75, 18.0, 0.02, 0.8, 0.0]);
                (2, alligator::create)
            }
            _ => {
                params[..6].copy_from_slice(&[0.0, 14.0, 470.0, 16.0, 1.0, 0.2]);
                (1, elephant::create)
            }
        };
        let mut sim = create(OUTPUT_RATE * factor as f64);
        sim.control(&params, 0.0, CONTROL_PERIOD as f64 / OUTPUT_RATE);
        if let Some(net) = sim.net_mut() { net.snap_to_targets(); }
        Self { kind, sim, target: params, value: params, gate: 0.0, goal: 0.0,
               dec: Decimator::new(factor), ctrl: 0, dc_x: 0.0, dc_y: 0.0,
               dc_coef: exp(-2.0 * PI * 12.0 / OUTPUT_RATE) }
    }

    pub fn controls(&mut self, hz: f32, gate: f32, macro_values: &[f32; 5]) {
        let pitch = f64::from(hz) / 261.625565;
        let [shape, color, aperture, branch, pressure] = macro_values.map(f64::from);
        self.goal = f64::from(gate).clamp(0.0, 1.0);
        match self.kind {
            3 => {
                self.target[0] = (2600.0 * pitch).clamp(400.0, 9000.0);
                self.target[2] = branch;
                self.target[3] = shape * 0.7;
                self.target[4] = color * 0.8;
                self.target[5] = 0.2 + 1.4 * pressure;
                self.target[8] = 0.1 + 0.9 * aperture;
            }
            4 => {
                self.target[0] = (900.0 * pitch).clamp(40.0, 6000.0);
                self.target[1] = 2.0 + 32.0 * branch;
                self.target[2] = shape;
                self.target[4] = color;
                self.target[6] = 2.0 + 16.0 * pressure;
                self.target[7] = aperture * 0.7;
            }
            5 => {
                self.target[0] = (28.0 * pitch).clamp(12.0, 120.0);
                self.target[1] = 0.3 + 0.7 * shape;
                self.target[2] = 4.0 + 28.0 * pressure;
                self.target[3] = aperture * aperture;
                self.target[4] = branch;
                self.target[5] = (color - 0.5).max(0.0) * 2.0;
            }
            _ => {
                self.target[0] = ((shape - 0.5) * 2.0).max(0.0);
                self.target[1] = (14.0 * pitch).clamp(8.0, 45.0);
                self.target[2] = (470.0 * pitch).clamp(150.0, 900.0);
                self.target[3] = 4.0 + 24.0 * pressure;
                self.target[4] = 0.25 * aperture + 0.75 * branch;
                self.target[5] = color * 0.4;
            }
        }
    }

    pub fn reset(&mut self) {
        self.gate = 0.0;
        self.goal = 0.0;
        self.dc_x = 0.0;
        self.dc_y = 0.0;
        self.ctrl = 0;
        self.dec.reset();
        if let Some(net) = self.sim.net_mut() { net.reset(); }
    }

    pub fn sample(&mut self) -> f32 {
        let factor = self.dec.factor;
        if self.ctrl == 0 {
            let dt = CONTROL_PERIOD as f64 / OUTPUT_RATE;
            let tau = if self.goal > self.gate { 0.012 } else { 0.025 };
            self.gate += (self.goal - self.gate) * (1.0 - exp(-dt / tau));
            tiny(&mut self.gate);
            for (value, target) in self.value.iter_mut().zip(self.target) {
                *value += (target - *value) * 0.12;
            }
            self.sim.flush_tiny();
            self.sim.control(&self.value, self.gate, dt);
            if let Some(net) = self.sim.net_mut() { net.begin_block(CONTROL_PERIOD * factor); }
            self.ctrl = CONTROL_PERIOD;
        }
        self.ctrl -= 1;
        for _ in 0..factor { self.dec.push(self.sim.sample()); }
        let input = self.dec.value * self.sim.gain();
        let output = input - self.dc_x + self.dc_coef * self.dc_y;
        self.dc_x = input;
        self.dc_y = output;
        tiny(&mut self.dc_x);
        tiny(&mut self.dc_y);
        if output.abs() > 0.7 {
            (output.signum() * (0.7 + 0.3 * tanh((output.abs() - 0.7) / 0.3))) as f32
        } else {
            output as f32
        }
    }

    pub fn geometry(&self, out: &mut Geometry) {
        out.model = self.kind;
        out.gate = self.gate as f32;
        let Some(net) = self.sim.net() else { return };
        for (section, (name, tube)) in out.animal.iter_mut().zip(self.sim.tubes().iter().take(ANIMAL_TUBES)) {
            section.name.fill(0);
            section.name[..name.len()].copy_from_slice(name.as_bytes());
            section.length = net.tube_length[tube.index] as f32;
            for i in 0..ANIMAL_SECTIONS {
                let node = tube.node(i * (tube.n - 1) / (ANIMAL_SECTIONS - 1));
                section.radius[i] = sqrt(net.area[node].max(0.0) / PI) as f32;
                let value = net.p[node];
                section.pressure[i] = if value.is_finite() { (value as f32 / 4000.0).clamp(-1.0, 1.0) } else { 0.0 };
            }
        }
    }
}
