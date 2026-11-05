//! C ABI over lindera with the embedded IPADIC dictionary.
//!
//! Tokens come back as one UTF-8 buffer: fields split by 0x1f, tokens by 0x1e.
//! Fields: surface, base form, reading, pos, pos1, conjugation type, conjugation form.

use lindera::dictionary::load_dictionary;
use lindera::mode::Mode;
use lindera::segmenter::Segmenter;
use lindera::tokenizer::Tokenizer;
use std::os::raw::c_char;

pub struct Tok(Tokenizer);

#[no_mangle]
pub extern "C" fn jptok_new() -> *mut Tok {
    let Ok(dict) = load_dictionary("embedded://ipadic") else { return std::ptr::null_mut() };
    let seg = Segmenter::new(Mode::Normal, dict, None);
    Box::into_raw(Box::new(Tok(Tokenizer::new(seg))))
}

#[no_mangle]
pub unsafe extern "C" fn jptok_free(t: *mut Tok) {
    if !t.is_null() {
        drop(Box::from_raw(t));
    }
}

/// Returns a buffer owned by the caller (free with `jptok_buf_free`), or null on error.
#[no_mangle]
pub unsafe extern "C" fn jptok_tokenize(t: *mut Tok, text: *const c_char, len: usize, out_len: *mut usize) -> *mut u8 {
    if t.is_null() || text.is_null() {
        return std::ptr::null_mut();
    }
    let bytes = std::slice::from_raw_parts(text as *const u8, len);
    let Ok(text) = std::str::from_utf8(bytes) else { return std::ptr::null_mut() };
    let Ok(mut tokens) = (*t).0.tokenize(text) else { return std::ptr::null_mut() };
    let mut out = String::new();
    for (i, tok) in tokens.iter_mut().enumerate() {
        if i > 0 {
            out.push('\u{1e}');
        }
        let surface = tok.surface.to_string();
        let d = tok.details();
        let f = |k: usize| d.get(k).copied().unwrap_or("");
        // IPADIC details: pos, pos1, pos2, pos3, conj. type, conj. form, base form, reading, pronunciation
        for (j, v) in [surface.as_str(), f(6), f(7), f(0), f(1), f(4), f(5)].iter().enumerate() {
            if j > 0 {
                out.push('\u{1f}');
            }
            out.push_str(v);
        }
    }
    let mut v = out.into_bytes().into_boxed_slice();
    *out_len = v.len();
    let p = v.as_mut_ptr();
    std::mem::forget(v);
    p
}

#[no_mangle]
pub unsafe extern "C" fn jptok_buf_free(p: *mut u8, len: usize) {
    if !p.is_null() {
        drop(Box::from_raw(std::ptr::slice_from_raw_parts_mut(p, len)));
    }
}
