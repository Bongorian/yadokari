//! African elephant: infrasonic rumbles from self-oscillating vocal folds, routed through the trunk
//! or the mouth; trumpets from a valve at the trunk entrance that locks onto the trunk's resonances
//! like a brass player's lips on a bugle; squeaks from tensed lips.

use super::build::{CMH2O, TubeArea, TubeOpts, fill_areas, tube};
use super::mammal::{MammalAirway, MammalSpec, mammal_airway, mammal_noise, step_glottis};
use super::CreatureSim;
use alloc::{boxed::Box, vec, vec::Vec};
use crate::airway::network::{AirwayNetwork, PortSpec, Tube};
use crate::math::js_max;
use crate::physics::WALL_NASAL;
use crate::sources::valves::LipReed;

const MODE: usize = 0;
const PITCH: usize = 1;
const LIP_PITCH: usize = 2;
const PRESSURE: usize = 3;
const TRUNK: usize = 4;
const CHAOS: usize = 5;

pub struct Elephant {
    a: MammalAirway,
    tubes: Vec<(&'static str, Tube)>,
    nasal: Tube,
    trunk: Tube,
    valve: usize,
    velum: usize,
    lips: LipReed,
    mode: f64,
}

pub fn create(fs: f64) -> Box<dyn CreatureSim> {
    let mut trunk: Option<Tube> = None;
    let mut valve = 0usize;
    let mut a = mammal_airway(
        fs,
        MammalSpec { trachea: (45.0, 14.0), pharynx: (32.0, 14.0), oral: (28.0, 10.0), nasal: Some((14.0, 6.0)), wall: None, wall_radiation: Some(0.6), coarse: Some(1.2), nasal_radiates: Some(false) },
        |net, parts| {
            // the "nasal" tube is the trunk base; a lip-like valve leads into the long trunk
            let t = tube(net, "trunk", fs, 185.0, TubeArea::Profile(&|u| 4.2 - 3.0 * u), TubeOpts { wall: Some(WALL_NASAL), coarse: Some(2.3), max: Some(110.0), perimeter_factor: Some(1.4), ..Default::default() });
            valve = net.add_port(parts.nasal.expect("elephant has a trunk base").last_node(), t.first_node(), PortSpec::new(0.0, 0.6).kinetic(0.3, 1.0));
            net.add_radiation(t.last_node());
            trunk = Some(t);
        },
    );
    let trunk = trunk.expect("trunk built");
    a.glottis_src.threshold = 1.5 * CMH2O;
    let nasal = a.nasal.expect("elephant has a trunk base");
    let velum = a.velum.expect("elephant has a velum");
    let tubes = vec![("trachea", a.trachea), ("pharynx", a.pharynx), ("oral", a.oral), ("nasal", nasal), ("trunk", trunk)];
    Box::new(Elephant { a, tubes, nasal, trunk, valve, velum, lips: LipReed::new(fs), mode: 0.0 })
}

impl CreatureSim for Elephant {
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
        1.8e-6
    }
    fn control(&mut self, p: &[f64], gate: f64, _dt: f64) {
        self.mode = p[MODE];
        let mode = self.mode;
        let a = &mut self.a;
        a.net.set_fixed_pressure(a.lungs, p[PRESSURE] * CMH2O * gate);
        // rumble: folds vibrate; trumpet: folds open, valve buzzes
        let g = &mut a.glottis_src.params;
        g.f0 = p[PITCH];
        g.amplitude = 1.6 * (1.0 - mode);
        g.abduction = 0.05 + 3.0 * mode; // trumpet: larynx wide open, silent
        g.open_quotient = 0.6;
        g.closure_sharpness = 0.6;
        g.effort = 0.9;
        g.chaos = p[CHAOS] * 0.6;
        let lips = &mut self.lips;
        lips.f = p[LIP_PITCH];
        lips.zeta = 0.08;
        lips.h0 = 0.003; // a slight rest gap: buzzing lips start from just apart
        lips.width = 2.4;
        lips.surface_per_mass = 40.0;
        let to_trunk = p[TRUNK];
        a.net.set_port_target(self.velum, 0.1 + 6.0 * to_trunk);
        let ph_areas = fill_areas(a.pharynx, |_| 14.0, &mut a.phar_area);
        a.net.set_tube_targets(a.pharynx, ph_areas, None, None);
        let or_areas = fill_areas(a.oral, |u| if u > 0.85 { 0.05 + 10.0 * (1.0 - to_trunk) } else { 10.0 }, &mut a.oral_area);
        a.net.set_tube_targets(a.oral, or_areas, None, None);
        mammal_noise(a, 4e-7 * (1.0 - mode), None);
    }
    #[inline]
    fn sample(&mut self) -> f64 {
        step_glottis(&mut self.a);
        let mode = self.mode;
        if mode > 0.02 {
            let p = &self.a.net.p;
            let dp = p[self.nasal.last_node()] - p[self.trunk.first_node()];
            let open = self.lips.next(dp * mode);
            self.a.net.set_port_area(self.valve, js_max(open, 4.5 * (1.0 - mode)));
        } else {
            self.a.net.set_port_area(self.valve, 4.5);
        }
        self.a.net.step()
    }
    fn flush_tiny(&mut self) {
        self.a.glottis_src.flush_tiny();
    }
}
