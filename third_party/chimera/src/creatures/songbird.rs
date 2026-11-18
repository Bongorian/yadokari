//! Two-voice songbird (thrush-sized). Each side of the syrinx has its own labial oscillator and
//! pitch; both bronchi merge into the trachea → larynx → oropharyngeal–oesophageal cavity (OEC,
//! a Helmholtz resonator whose volume can track f0 for pure tones) → beak (gape controls radiation).

use super::build::{TubeArea, TubeOpts, fill_areas, tube};
use super::CreatureSim;
use alloc::{boxed::Box, vec, vec::Vec};
use crate::airway::network::{AirwayNetwork, PortSpec, Tube};
use crate::math::{cbrt, js_max, js_min, pow};
use crate::physics::WALL_TRACHEA;
use crate::sources::syrinx::SyrinxLabia;

const OEC_NECK: f64 = 0.03; // cm² (larynx)
const OEC_LEN: f64 = 0.35; // cm, with end corrections
const AMAX: f64 = 0.03;

// parameter indices (order of `def().params`)
const PITCH: usize = 0;
const INTERVAL: usize = 1;
const VOICES: usize = 2;
const DRIVE: usize = 3;
const TENSION: usize = 4;
const PRESSURE: usize = 5;
const PURITY: usize = 6;
const OEC: usize = 7;
const GAPE: usize = 8;

pub struct Songbird {
    net: AirwayNetwork,
    tubes: Vec<(&'static str, Tube)>,
    beak: Tube,
    sac: usize,
    v_l: usize,
    v_r: usize,
    oec: usize,
    oec_len: f64,
    lab_l: SyrinxLabia,
    lab_r: SyrinxLabia,
    beak_areas: Vec<f64>,
}

pub fn create(fs: f64) -> Box<dyn CreatureSim> {
    let mut net = AirwayNetwork::new(fs);
    let bron_l = tube(&mut net, "bronchusL", fs, 0.8, TubeArea::Uniform(0.02), TubeOpts::default());
    let bron_r = tube(&mut net, "bronchusR", fs, 0.8, TubeArea::Uniform(0.02), TubeOpts::default());
    let trachea = tube(&mut net, "trachea", fs, 3.6, TubeArea::Uniform(0.045), TubeOpts { wall: Some(WALL_TRACHEA), ..Default::default() });
    let sac = net.add_pressure_node("air sac");
    net.add_port(sac, bron_l.first_node(), PortSpec::new(0.05, 0.4));
    net.add_port(sac, bron_r.first_node(), PortSpec::new(0.05, 0.4));
    let v_l = net.add_port(bron_l.last_node(), trachea.first_node(), PortSpec::new(0.0, 0.08).kinetic(0.5, 1.0));
    let v_r = net.add_port(bron_r.last_node(), trachea.first_node(), PortSpec::new(0.0, 0.08).kinetic(0.5, 1.0));
    let oec = net.add_cavity("oec", 0.4, None);
    net.add_port(trachea.last_node(), oec, PortSpec::new(OEC_NECK, 0.25).kinetic(0.3, 0.8));
    let beak = tube(&mut net, "beak", fs, 1.3, TubeArea::Uniform(0.08), TubeOpts::default());
    net.connect(oec, beak.first_node());
    net.add_radiation(beak.last_node());
    net.finalize();
    Box::new(Songbird {
        net,
        tubes: vec![("trachea", trachea), ("beak", beak), ("bronchusL", bron_l), ("bronchusR", bron_r)],
        beak,
        sac,
        v_l,
        v_r,
        oec,
        oec_len: cbrt(0.4),
        lab_l: SyrinxLabia::new(fs, 0.0),
        lab_r: SyrinxLabia::new(fs, 1.0),
        beak_areas: vec![0.0; beak.n],
    })
}

impl CreatureSim for Songbird {
    fn net(&self) -> Option<&AirwayNetwork> {
        Some(&self.net)
    }
    fn net_mut(&mut self) -> Option<&mut AirwayNetwork> {
        Some(&mut self.net)
    }
    fn tubes(&self) -> &[(&'static str, Tube)] {
        &self.tubes
    }
    fn gain(&self) -> f64 {
        4.8e-7
    }
    fn control(&mut self, p: &[f64], gate: f64, _dt: f64) {
        let (lab_l, lab_r) = (&mut self.lab_l, &mut self.lab_r);
        self.net.set_fixed_pressure(self.sac, p[PRESSURE] * 1000.0 * gate * 10.0); // kPa → dyn/cm² (×10 000)
        lab_l.f0 = p[PITCH];
        lab_r.f0 = p[PITCH] * pow(2.0, p[INTERVAL] / 12.0);
        lab_l.drive = p[DRIVE];
        lab_r.drive = p[DRIVE];
        lab_l.tension = p[TENSION];
        lab_r.tension = p[TENSION];
        // voices: 0 = left only, 0.5 = both, 1 = right only
        lab_l.gate = if p[VOICES] <= 0.5 { 1.0 } else { js_max(0.0, 2.0 - 2.0 * p[VOICES]) };
        lab_r.gate = if p[VOICES] >= 0.5 { 1.0 } else { js_max(0.0, 2.0 * p[VOICES]) };
        // OEC tracks the (lower) fundamental for pure tones, or stays at its own size
        let f_t = js_min(lab_l.f0, lab_r.f0);
        let v_track = OEC_NECK / (OEC_LEN * pow(f_t / 5570.0, 2.0));
        let vol = js_min(2.5, js_max(0.04, p[PURITY] * v_track + (1.0 - p[PURITY]) * p[OEC]));
        self.net.set_node_area_target(self.oec, vol / self.oec_len);
        let gape = p[GAPE];
        let areas = fill_areas(self.beak, |u| 0.012 + gape * (0.05 + 0.22 * u), &mut self.beak_areas);
        self.net.set_tube_targets(self.beak, areas, None, None);
    }
    #[inline]
    fn sample(&mut self) -> f64 {
        let a = AMAX * self.lab_l.next();
        self.net.set_port_area(self.v_l, a);
        let a = AMAX * self.lab_r.next();
        self.net.set_port_area(self.v_r, a);
        self.net.step()
    }
}
