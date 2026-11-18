use crate::anatomy::human::{ArticKey as A, NEUTRAL};
use crate::control::score::{ControlKey as C, Gesture, Targets};
use crate::Dsp;
use core::{mem, slice, str};

const PHRASES: usize = 8;
const PHONES: usize = 16;
const ROW_BYTES: usize = 320;
const _: () = assert!(mem::size_of::<Gesture>() <= ROW_BYTES && mem::align_of::<Gesture>() <= 8);

#[repr(C, align(8))]
struct ScoreRow([mem::MaybeUninit<u8>; ROW_BYTES]);

#[repr(C)]
pub struct Phrase {
    note: u32,
    count: u32,
    seconds: f32,
    lead: f32,
    score: [ScoreRow; PHONES],
}

#[repr(C)]
pub struct Speech {
    count: u32,
    phrases: [Phrase; PHRASES],
}

fn vowel(x: f64, y: f64, jaw: f64, round: f64) -> Targets {
    Targets::default()
        .with(A::TbX, x).with(A::TbY, y).with(A::Jaw, jaw)
        .with(A::LipAp, 0.8 - round * 0.5).with(A::LipProt, 0.8 * round)
        .with(A::LipWidth, 0.9 - round * 0.6).with(A::Velum, -0.15)
        .with(C::BREATH, 0.9)
}

fn phone(ch: char) -> Option<(Targets, f64, bool)> {
    let v = match ch {
        'a' | 'ɑ' => vowel(2.15, 5.75, 1.45, 0.0),
        'i' => vowel(4.7, 8.1, 0.45, 0.0),
        'u' | 'ɯ' => vowel(2.1, 7.65, 0.42, if ch == 'u' { 0.95 } else { 0.15 }),
        'e' => vowel(3.85, 7.75, 0.68, 0.0),
        'o' => vowel(2.35, 6.65, 0.8, 0.8),
        'ə' => vowel(NEUTRAL.tb_x, NEUTRAL.tb_y, NEUTRAL.jaw, 0.0),
        'ɛ' | 'æ' => vowel(3.65, 6.55, 1.15, 0.0),
        'ɔ' => vowel(2.0, 6.0, 1.05, 0.7),
        'ɪ' => vowel(3.8, 7.5, 0.65, 0.0),
        'ʊ' => vowel(2.6, 7.3, 0.6, 0.75),
        'ɐ' | 'ʌ' => vowel(2.75, 6.0, 1.05, 0.0),
        _ => {
            let mut target = Targets::default().with(A::Velum, -0.15);
            let mut duration = 0.06;
            let mut breath = 0.7;
            match ch {
                'p' | 'b' | 'm' => { target.set(A::LipAp, -0.15); }
                't' | 'd' | 'n' | 'ɾ' | 'l' => {
                    target.set(A::TtX, 6.95).set(A::TtY, 8.9).set(A::TtCurl, 0.6);
                }
                'k' | 'ɡ' | 'g' | 'ŋ' => { target.set(A::TbX, 3.7).set(A::TbY, 8.65); }
                's' | 'z' | 'ʃ' | 'ʒ' => {
                    let x = if matches!(ch, 'ʃ' | 'ʒ') { 6.3 } else { 6.85 };
                    target.set(A::TtX, x).set(A::TtY, 8.72).set(A::Jaw, 0.55);
                    duration = 0.095;
                }
                'f' | 'v' => { target.set(A::Labiodental, 0.9).set(A::LipAp, 0.35); duration = 0.095; }
                'h' | 'ç' | 'x' => { target.set(A::TbY, if ch == 'h' { 6.9 } else { 8.05 }); duration = 0.075; }
                'w' => return Some((vowel(2.3, 7.75, 0.5, 1.0), 0.07, false)),
                'j' => return Some((vowel(4.9, 8.4, 0.5, 0.0), 0.065, false)),
                ' ' | '.' => { breath = 0.0; duration = 0.075; }
                _ => return None,
            }
            if matches!(ch, 'p' | 't' | 'k' | 's' | 'ʃ' | 'f' | 'h' | 'ç' | 'x') {
                target.set(C::ABDUCTION, 0.32);
            }
            if matches!(ch, 'm' | 'n' | 'ŋ') {
                target.set(A::Velum, 1.0);
            }
            return Some((target.with(C::BREATH, breath), duration, false));
        }
    };
    Some((v, 0.125, true))
}

fn compile_phrase(text: &str, phrase: &mut Phrase) -> bool {
    let mut time = 0.0;
    let mut found_vowel = false;
    for ch in text.chars() {
        let Some((targets, duration, is_vowel)) = phone(ch) else { return false };
        if phrase.count as usize == PHONES { return false; }
        if is_vowel && !found_vowel {
            phrase.lead = time as f32;
            found_vowel = true;
        }
        let mut label = [0; 4];
        let gesture = Gesture::new(time, time + duration, 0.009, 0.013, 10.0, targets)
            .speed(if is_vowel { 1.75 } else { 3.0 })
            .label(ch.encode_utf8(&mut label));
        let row = &mut phrase.score[phrase.count as usize];
        // The row is aligned for Gesture and owned exclusively by the producer.
        unsafe { row.0.as_mut_ptr().cast::<Gesture>().write(gesture) };
        phrase.count += 1;
        time += duration;
    }
    phrase.seconds = (time + 0.045) as f32;
    phrase.count != 0
}

fn phrase_key(value: &str) -> Option<u32> {
    if let Some((pattern, step)) = value.split_once('/') {
        let pattern = pattern.parse::<u32>().ok()?;
        let step = step.parse::<u32>().ok()?;
        if (1..=16).contains(&pattern) && (1..=256).contains(&step) {
            return Some(128 + (pattern - 1) * 256 + step - 1);
        }
        return None;
    }
    let note = value.parse::<u32>().ok()?;
    (note < 128).then_some(note)
}

fn parse(bytes: *const u8, len: usize, mut row: impl FnMut(usize, u32, &str) -> bool) -> bool {
    if bytes.is_null() || len == 0 || len > 1024 { return false; }
    let Ok(text) = str::from_utf8(unsafe { slice::from_raw_parts(bytes, len) }) else { return false };
    let mut keys = [0; PHRASES];
    let mut count = 0;
    for line in text.trim_start_matches('\u{feff}').lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') { continue; }
        let Some((key, ipa)) = line.split_once('\t') else { return false };
        let Some(key) = phrase_key(key) else { return false };
        if count == PHRASES || keys[..count].contains(&key) ||
           !row(count, key, ipa.trim()) { return false; }
        keys[count] = key;
        count += 1;
    }
    count != 0
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_valid_ipa(bytes: *const u8, len: u32) -> bool {
    parse(bytes, len as usize, |_, _, ipa| {
        !ipa.is_empty() && ipa.chars().count() <= PHONES && ipa.chars().all(|ch| phone(ch).is_some())
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_compile_ipa(bytes: *const u8, len: usize, out: &mut Speech) -> bool {
    out.count = 0;
    parse(bytes, len, |index, note, ipa| {
        let phrase = &mut out.phrases[index];
        phrase.note = note;
        phrase.count = 0;
        phrase.lead = 0.0;
        if !compile_phrase(ipa, phrase) { return false; }
        out.count += 1;
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn yadokari_chimera_speak(model: &mut Dsp, phrase: &Phrase, elapsed: f32, timed: bool) -> u32 {
    let Dsp::Human(voice) = model else { return 0 };
    voice.cancel_score();
    voice.reset_airflow();
    voice.score_time = if timed { Some(f64::from(elapsed)) } else { None };
    let origin = if timed { 0.0 } else { voice.clock + 0.001 - f64::from(elapsed) };
    for row in &phrase.score[..phrase.count as usize] {
        // Every submitted row was constructed by compile_phrase before publication.
        let mut gesture = unsafe { row.0.as_ptr().cast::<Gesture>().read() };
        gesture.t0 += origin;
        gesture.t1 += origin;
        if gesture.end() > voice.score_time.unwrap_or(voice.clock) { voice.schedule(&[gesture]); }
    }
    ((phrase.seconds - elapsed).max(0.0) * 48000.0) as u32
}
