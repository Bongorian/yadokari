//! Real-time human voice: lungs → parametric glottis → articulated airway, rendered at an
//! oversampled internal rate and decimated. The same code renders in real time (the browser's
//! AudioWorklet, a DAW's audio thread) and offline, sample for sample.

use super::human_airway::HumanAirway;
use crate::airway::decimator::Decimator;
use crate::anatomy::geom::Vec2;
use crate::anatomy::human::{ArticKey, ArticPatch, BODY_MALE, HumanBody};
use crate::control::articulators::ArticulatorBank;
use crate::control::score::{Blended, ControlKey, Gesture, GestureScore, N_CONTROL};
use crate::math::{PI, clamp, exp, js_max, js_min, js_sign, js_trunc_i32, pow, tanh};
use crate::physics::CM_H2O;

#[derive(Clone, Copy, Debug, PartialEq)]

pub struct VoiceSource {
    /// target fundamental frequency (Hz)
    pub f0: f64,
    /// pitch glide time constant (s)
    pub glide: f64,
    /// lung pressure while breathing out (cmH2O); negative = ingressive
    pub pressure: f64,
    /// 0..1 breathy (abducted, chink)
    pub breathiness: f64,
    /// 0 lax … 1 pressed
    pub tension: f64,
    /// vocal fry 0..1
    pub fry: f64,
    /// jitter / shimmer / period doubling 0..1
    pub roughness: f64,
    pub vibrato_depth: f64,
    pub vibrato_rate: f64,
    /// random pitch drift (cents)
    pub drift: f64,
    /// false → whisper (no fold vibration)
    pub voiced: bool,
    /// 0 dark … 1 bright: how abruptly the folds close, at any tension. The default (0.25) leaves
    /// it to tension; it sits low because a spoken or sung voice already closes nearly as softly as
    /// the model can, so there is little room to darken and a lot to brighten.

    pub brightness: f64,
}

fn default_brightness() -> f64 {
    DEFAULT_SOURCE.brightness
}

pub const DEFAULT_SOURCE: VoiceSource = VoiceSource { f0: 110.0, glide: 0.06, pressure: 8.0, breathiness: 0.15, tension: 0.5, fry: 0.0, roughness: 0.1, vibrato_depth: 0.0, vibrato_rate: 6.0, drift: 5.0, voiced: true, brightness: 0.25 };

/// `Partial<VoiceSource>`.
#[derive(Clone, Copy, Debug, Default, PartialEq)]

pub struct SourcePatch {

    pub f0: Option<f64>,

    pub glide: Option<f64>,

    pub pressure: Option<f64>,

    pub breathiness: Option<f64>,

    pub tension: Option<f64>,

    pub fry: Option<f64>,

    pub roughness: Option<f64>,

    pub vibrato_depth: Option<f64>,

    pub vibrato_rate: Option<f64>,

    pub drift: Option<f64>,

    pub voiced: Option<bool>,

    pub brightness: Option<f64>,
}

impl SourcePatch {
    pub fn f0(f0: f64) -> Self {
        SourcePatch { f0: Some(f0), ..Default::default() }
    }
    /// `Object.assign(source, patch)`
    pub fn apply(&self, s: &mut VoiceSource) {
        macro_rules! set {
            ($($f:ident),*) => { $( if let Some(x) = self.$f { s.$f = x; } )* };
        }
        set!(f0, glide, pressure, breathiness, tension, fry, roughness, vibrato_depth, vibrato_rate, drift, voiced, brightness);
    }
    pub fn merge(&mut self, o: &SourcePatch) {
        macro_rules! set {
            ($($f:ident),*) => { $( if o.$f.is_some() { self.$f = o.$f; } )* };
        }
        set!(f0, glide, pressure, breathiness, tension, fry, roughness, vibrato_depth, vibrato_rate, drift, voiced, brightness);
    }
    pub fn from_source(s: &VoiceSource) -> Self {
        SourcePatch {
            f0: Some(s.f0),
            glide: Some(s.glide),
            pressure: Some(s.pressure),
            breathiness: Some(s.breathiness),
            tension: Some(s.tension),
            fry: Some(s.fry),
            roughness: Some(s.roughness),
            vibrato_depth: Some(s.vibrato_depth),
            vibrato_rate: Some(s.vibrato_rate),
            drift: Some(s.drift),
            voiced: Some(s.voiced),
            brightness: Some(s.brightness),
        }
    }
}

#[derive(Clone, Debug)]
pub struct HumanVoice {
    pub fs_out: f64,
    pub m: usize,
    pub fs: f64,
    pub airway: HumanAirway,
    pub glottis: crate::sources::glottis::ParametricGlottis,
    pub artic: ArticulatorBank,
    pub source: VoiceSource,
    dec: Decimator,
    /// internal samples per control step
    ctrl_every: usize,
    ctrl_counter: i64,
    /// breath gate 0..1 (note on/off)
    pub gate: f64,
    gate_target: f64,
    /// time left (s) during which a release of the gate follows the score's own breath ramp
    score_breath_hold: f64,
    p_lung: f64,
    /// lung pressure applied this sample, ramped toward `p_lung` across the control block
    p_lung_now: f64,
    p_lung_step: f64,
    f0: f64,
    pub output_gain: f64,
    dc_x: f64,
    dc_y: f64,
    dc_coef: f64,
    // telemetry accumulators
    /// gestural score and voice clock (s)
    pub score: GestureScore,
    pub clock: f64,
    pub score_time: Option<f64>,
    blend: [Blended; N_CONTROL],
    base_abduction: f64,
    base_closure: f64,
    score_f0: Option<f64>,
    lim_env: f64,
    lim_hold: i64,
    lim_hold_n: i64,
    lim_release: f64,
    /// tissue may compress this far (cm) before the palate stops the articulator
    pub max_penetration: f64,
}

/// How many times the airway is oversampled at this output rate: the smallest factor that puts
/// the simulation above 60 kHz (2× at 44.1 and 48 kHz, 1× at 88.2 and 96 kHz). The tubes keep a
/// minimum section count, so a lower simulation rate makes the sections shorter than one step of
/// sound and the airway blows up: at 2× a 16 kHz output (a Bluetooth headset's) went NaN within a
/// second, and 24 kHz ran at the limiter ceiling (issue #33).
pub fn oversample_for(fs_out: f64) -> usize {
    js_max(1.0, libm::floor(60000.0 / fs_out) + 1.0) as usize
}

impl HumanVoice {
    pub fn reset_airflow(&mut self) {
        self.airway.net.reset();
        self.glottis.reset();
        self.dec.reset();
        self.gate = 0.0;
        self.gate_target = 0.0;
        self.p_lung = 0.0;
        self.p_lung_now = 0.0;
        self.p_lung_step = 0.0;
        self.ctrl_counter = 0;
        self.dc_x = 0.0;
        self.dc_y = 0.0;
        self.lim_env = 0.0;
        self.lim_hold = 0;
    }

    /// A voice rendering at `fs_out`, simulating at `fs_out · oversample`.
    pub fn new(fs_out: f64, body: HumanBody, oversample: usize) -> Self {
        let m = oversample;
        let fs = fs_out * m as f64;
        let airway = HumanAirway::new(fs, body);
        let mut v = HumanVoice {
            fs_out,
            m,
            fs,
            airway,
            glottis: crate::sources::glottis::ParametricGlottis::new(fs),
            artic: ArticulatorBank::new(),
            source: DEFAULT_SOURCE,
            dec: Decimator::new(m),
            ctrl_every: 64 * m,
            ctrl_counter: 0,
            gate: 0.0,
            gate_target: 0.0,
            score_breath_hold: 0.0,
            p_lung: 0.0,
            p_lung_now: 0.0,
            p_lung_step: 0.0,
            f0: DEFAULT_SOURCE.f0,
            output_gain: 5.5e-7,
            dc_x: 0.0,
            dc_y: 0.0,
            dc_coef: exp((-2.0 * PI * 25.0) / fs_out),
            score: GestureScore::new(),
            clock: 0.0, score_time: None,
            blend: [Blended::default(); N_CONTROL],
            base_abduction: 0.0,
            base_closure: 0.0,
            score_f0: None,
            lim_env: 0.0,
            lim_hold: 0,
            lim_hold_n: crate::math::js_round(0.025 * fs_out) as i64,
            lim_release: exp(-1.0 / (0.2 * fs_out)),
            max_penetration: 0.1,
        };
        let x = v.artic.x;
        v.airway.apply(&x, 0.0);
        v.airway.net.snap_to_targets();
        // smaller bodies radiate more high-frequency (derivative) energy at their higher f0
        // (a ramp from the adult female's scale up to the adult male's, so blended bodies have no step)
        let sc = v.airway.body.scale;
        v.output_gain *= if sc >= 1.0 {
            1.0
        } else if sc <= 0.9 {
            0.55
        } else {
            0.55 + 0.45 * (sc - 0.9) / 0.1
        };
        v.apply_source();
        v
    }

    /// The defaults the browser uses: male body, oversampled by [`oversample_for`].
    pub fn with_defaults(fs_out: f64) -> Self {
        Self::new(fs_out, BODY_MALE, oversample_for(fs_out))
    }

    pub fn set_articulation(&mut self, values: &ArticPatch, tip_free: Option<bool>) {
        if let Some(t) = tip_free {
            self.artic.tip_free = t;
        }
        self.artic.set_target(values);
    }

    pub fn set_source(&mut self, values: &SourcePatch) {
        values.apply(&mut self.source);
        self.apply_source();
    }

    /// Add gestures (times on the voice clock).
    pub fn schedule(&mut self, gestures: &[Gesture]) {
        self.score.add_all(gestures);
    }

    /// End the open-ended gestures with this tag now (a sung note's key was released).
    pub fn release(&mut self, tag: f64) {
        self.score.release(tag, self.clock);
    }

    /// Cancel scheduled gestures (speech stops; the voice returns to the performer's posture).
    pub fn cancel_score(&mut self) {
        self.score.clear();
        self.score_time = None;
    }

    /// breath/voice on (1) or off (0); intermediate values scale the pressure
    pub fn set_gate(&mut self, level: f64) {
        self.gate_target = clamp(level, 0.0, 1.5);
    }

    fn apply_source(&mut self) {
        let s = self.source;
        let g = &mut self.glottis.params;
        let k2 = self.airway.body.scale * self.airway.body.scale;
        g.amplitude = 0.17 * k2 * (if s.voiced { 1.0 } else { 0.0 });
        g.abduction = (if s.voiced { 0.1 * s.breathiness * s.breathiness } else { 0.2 + 0.1 * s.breathiness }) * k2;
        self.base_abduction = g.abduction;
        self.base_closure = 0.0;
        // Most voices, women's and trained singers' included, leave a small posterior gap between
        // the arytenoids through the closed phase (Södersten & Lindestad 1990). It leaks a little
        // air and damps the tract, which lifts H1 and the band below 500 Hz.
        g.chink = (0.008 + 0.03 * s.breathiness) * k2;
        g.open_quotient = 0.78 - 0.36 * s.tension;
        // The folds close along their length and depth over a return phase, not all at once, so
        // a sung or spoken voice closes softly (0.22 at tension 0.55): an abrupt closure put
        // 3-8 dB too much energy above 2 kHz. Pressing harder than that brings the abrupt closure
        // back, up to 0.95 at full tension, so a pressed voice still has its edge. Brightness moves
        // the closure from there, and shortens or lengthens the closing phase: 0 closes as softly as
        // the model allows, 1 about as abruptly as a pressed voice.
        let bright = s.brightness - DEFAULT_SOURCE.brightness;
        let by_tension = 0.4 * s.tension + 1.375 * js_max(0.0, s.tension - 0.6);
        g.closure_sharpness = clamp(by_tension + 0.8 * bright, 0.0, 1.0);
        g.skew = 0.56 + 0.16 * s.tension + 0.2 * bright;
        g.fry = s.fry;
        // Jitter of a healthy, trained voice at the default roughness (0.1): about 0.45 %, well
        // under the clinical threshold (MDVP: 1.04 %).
        g.jitter = 0.0021 + 0.0245 * s.roughness;
        g.shimmer = 0.02 + 0.22 * s.roughness;
        g.diplophonia = if s.roughness > 0.5 { (s.roughness - 0.5) * 1.4 } else { 0.0 };
        g.vibrato_depth = s.vibrato_depth;
        g.vibrato_rate = s.vibrato_rate;
        g.drift = s.drift;
        g.effort = clamp((s.pressure.abs() - 3.0) / 10.0, 0.0, 1.0);
    }

    /// Render `out.len()` output samples.
    pub fn render(&mut self, out: &mut [f32]) {
        let m = self.m;
        let lungs = self.airway.lungs;
        let sub_node = self.airway.trachea.last_node();
        let supra = self.airway.pharynx.first_node();
        let gp = self.airway.glottis_port;
        for o in out.iter_mut() {
            for _ in 0..m {
                if self.ctrl_counter <= 0 {
                    self.control();
                }
                self.ctrl_counter -= 1;
                let net = &mut self.airway.net;
                self.p_lung_now += self.p_lung_step;
                net.set_fixed_pressure(lungs, self.p_lung_now);
                let a = self.glottis.next(net.p[sub_node] - net.p[supra]);
                net.set_port_area(gp, a);
                let y = net.step();
                if self.dec.push(y) {
                    // output sample
                    let mut v = self.dec.value * self.output_gain;
                    // DC blocker
                    let hp = v - self.dc_x + self.dc_coef * self.dc_y;
                    self.dc_x = v;
                    self.dc_y = hp;
                    v = hp;
                    // output limiter: a peak envelope held for longer than a glottal period, so a loud
                    // sung vowel is turned down by a steady gain instead of being squared off; only a
                    // rising level touches the soft clip
                    let av = v.abs();
                    if av >= self.lim_env {
                        self.lim_env = av;
                        self.lim_hold = self.lim_hold_n;
                    } else if self.lim_hold > 0 {
                        self.lim_hold -= 1;
                    } else {
                        self.lim_env *= self.lim_release;
                    }
                    if self.lim_env > 0.75 {
                        v *= 0.75 / self.lim_env;
                    }
                    let al = v.abs();
                    if al > 0.85 {
                        v = js_sign(v) * (0.85 + 0.15 * tanh((al - 0.85) / 0.15));
                    }
                    *o = v as f32;
                }
            }
        }
    }

    /// Control-rate update: articulator dynamics, geometry → areas, breath, pitch.
    fn control(&mut self) {
        let dt = self.ctrl_every as f64 / self.fs;
        let t = self.score_time.unwrap_or(self.clock);
        let mut breath_act = 0.0;
        let mut breath_val = 0.0;
        let mut f0_target = self.source.f0;
        if self.score.size() > 0 {
            self.score.blend_all(t, &mut self.blend);
            let b = &self.blend;
            // performer's base posture keeps a small say on keys the score controls
            self.artic.step(dt, Some(&b[..]), 0.05);
            let gp = &mut self.glottis.params;
            let k2 = self.airway.body.scale * self.airway.body.scale;
            let ab = b[ControlKey::ABDUCTION.index()];
            let ab_act = if ab.weight > 0.0 { js_min(1.0, ab.weight / 2.0) } else { 0.0 };
            gp.abduction = self.base_abduction + ab.value * ab_act * k2;
            let cl = b[ControlKey::CLOSURE.index()];
            gp.closure = js_max(self.base_closure, if cl.weight > 0.0 { cl.value * js_min(1.0, cl.weight / 2.0) } else { 0.0 });
            let br = b[ControlKey::BREATH.index()];
            if br.weight > 0.0 {
                breath_act = js_min(1.0, br.weight);
                breath_val = br.value;
            }
            let vb = b[ControlKey::VIBRATO.index()];
            gp.vibrato_depth = if vb.weight > 0.0 { (self.source.vibrato_depth * 0.05 + vb.value * vb.weight) / (0.05 + vb.weight) } else { self.source.vibrato_depth };
            let pf = b[ControlKey::F0.index()];
            if pf.weight > 0.0 {
                f0_target = (self.source.f0 * 0.02 + pf.value * pf.weight) / (0.02 + pf.weight);
                self.score_f0 = Some(f0_target);
            } else {
                self.score_f0 = None;
            }
            if js_trunc_i32(self.clock * 10.0) != js_trunc_i32((self.clock - dt) * 10.0) {
                self.score.prune(t - 0.2);
            }
        } else {
            self.artic.step(dt, None, 1.0);
            self.glottis.params.abduction = self.base_abduction;
            self.glottis.params.closure = self.base_closure;
            self.score_f0 = None;
        }
        self.clock += dt;
        // contact constraint on the frame computed anyway; re-apply only if a correction was made
        let x = self.artic.x;
        self.airway.apply(&x, dt);
        if self.enforce_contacts() {
            let x = self.artic.x;
            self.airway.reapply(&x);
        }
        self.airway.net.begin_block(self.ctrl_every);
        self.ctrl_counter = self.ctrl_every as i64;
        // breath: fast attack, slower release; a speaking score breathes on its own
        // a negative breath gesture (ingressive: inward K snares, gasps) overrides the performer's breath
        let score_breath = breath_val * breath_act;
        let gate_goal = if score_breath < 0.0 { score_breath } else { js_max(self.gate_target, score_breath) };
        // a score shapes its own breath ramps (beatbox hits end their breath abruptly, phrases end
        // into silence), including the moments just after its breath gesture has ended; the slow
        // release is for the performer's note-offs
        self.score_breath_hold = if breath_act > 0.0 { 0.15 } else { js_max(0.0, self.score_breath_hold - dt) };
        let tau = if gate_goal > self.gate {
            0.025
        } else if self.score_breath_hold > 0.0 {
            0.02
        } else {
            0.07
        };
        self.gate += (gate_goal - self.gate) * (1.0 - exp(-dt / tau));
        // a breath let go decays toward zero for ever, and a denormal lung pressure makes denormals
        // of the whole airway (see `AirwayNetwork::flush_tiny`): nothing is nothing
        crate::airway::network::tiny(&mut self.gate);
        self.glottis.flush_tiny();
        crate::airway::network::tiny(&mut self.dc_x);
        crate::airway::network::tiny(&mut self.dc_y);
        crate::airway::network::tiny(&mut self.lim_env);
        self.p_lung = self.source.pressure * CM_H2O * self.gate;
        if self.p_lung == 0.0 {
            crate::airway::network::tiny(&mut self.p_lung_now);
        }
        self.p_lung_step = (self.p_lung - self.p_lung_now) / self.ctrl_every as f64;
        // vocal effort follows the pressure actually applied: a harder breath (a louder sung note)
        // gives wider, sharper-closing pulses, as in real phonation, not just more flow
        self.glottis.params.effort = clamp(((self.source.pressure * self.gate).abs() - 3.0) / 10.0, 0.0, 1.0);
        // pitch glide
        let g = js_max(0.002, if self.score_f0.is_some() { 0.03 } else { self.source.glide });
        self.f0 *= pow(f0_target / self.f0, 1.0 - exp(-dt / g));
        self.glottis.params.f0 = self.f0;
    }

    /// Contact constraint: articulators cannot travel into the palate. Beyond `max_penetration` of
    /// tissue compression, the tongue tip/body is projected back along the contact normal and its
    /// inward velocity removed (inelastic). Keeps held closures still (no air pumping), makes
    /// releases start at the surface, and keeps the drawn anatomy physical.
    fn enforce_contacts(&mut self) -> bool {
        let tract = &self.airway.tract;
        let k = tract.scale();
        let lim = -self.max_penetration * k;
        let mut changed = false;
        let mut fix = |artic: &mut ArticulatorBank, d: f64, n: Vec2, kx: ArticKey, ky: ArticKey| {
            if !(d < lim) {
                return;
            }
            changed = true;
            let depth = lim - d; // > 0, body cm
            // body → template displacement (oral x is compressed by 0.86 in the template map)
            let sx = (if artic.x.get(kx) > 0.5 { 1.0 / 0.86 } else { 1.0 }) / k;
            let sy = 1.0 / k;
            *artic.x.get_mut(kx) += n.x * depth * sx;
            *artic.x.get_mut(ky) += n.y * depth * sy;
            let (ix, iy) = (kx.index(), ky.index());
            let vn = artic.v[ix] * n.x / sx + artic.v[iy] * n.y / sy; // normal velocity (body units)
            if vn < 0.0 {
                artic.v[ix] -= vn * n.x * sx;
                artic.v[iy] -= vn * n.y * sy;
            }
        };
        let (ct, ctn, cb, cbn) = (tract.contact_tip, tract.contact_tip_n, tract.contact_body, tract.contact_body_n);
        fix(&mut self.artic, ct, ctn, ArticKey::TtX, ArticKey::TtY);
        fix(&mut self.artic, cb, cbn, ArticKey::TbX, ArticKey::TbY);
        changed
    }
}
