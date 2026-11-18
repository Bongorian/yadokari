//! Lip reed from CHIMERA Bestiary.

use crate::math::PI;

/// Outward-striking lip valve: m·h'' + r·h' + k·(h − h0) = S·(p_up − p_down). Opening area = w·max(h, 0).
#[derive(Clone, Debug)]
pub struct LipReed {
    h: f64,
    v: f64,
    /// natural frequency (Hz) of the lip mass–spring
    pub f: f64,
    /// damping ratio
    pub zeta: f64,
    /// rest opening (cm): >0 leaks, <0 pressed together
    pub h0: f64,
    /// lip width (cm)
    pub width: f64,
    /// effective pressure surface per unit mass (cm²/g)
    pub surface_per_mass: f64,
    dt: f64,
    pub opening: f64,
}

impl LipReed {
    pub fn new(fs: f64) -> Self {
        LipReed { h: 0.0, v: 0.0, f: 400.0, zeta: 0.12, h0: -0.01, width: 1.2, surface_per_mass: 0.08, dt: 1.0 / fs, opening: 0.0 }
    }

    /// Advance one sample with the pressure difference across the lips; returns the opening area (cm²).
    #[inline]
    pub fn next(&mut self, dp: f64) -> f64 {
        let w = 2.0 * PI * self.f;
        let acc = self.surface_per_mass * dp - 2.0 * self.zeta * w * self.v - w * w * (self.h - self.h0);
        // semi-implicit Euler (stable for w·dt < 1 at the internal rate)
        self.v += acc * self.dt;
        self.h += self.v * self.dt;
        // the lips cannot interpenetrate: inelastic collision at h = 0 with tissue compliance
        if self.h < -0.05 {
            self.h = -0.05;
            if self.v < 0.0 {
                self.v = 0.0;
            }
        }
        if self.h > 0.8 {
            self.h = 0.8;
            if self.v > 0.0 {
                self.v = 0.0;
            }
        }
        let a = if self.h > 0.0 { self.h * self.width } else { 0.0 };
        self.opening = a;
        a
    }
}
