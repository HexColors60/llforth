extern crate libc;
extern crate lib;

use libc::c_char;
use std::ffi::CString;

fn main() {
    let mut inbuf: [c_char; 1024] = [0; 1024];
    let inbuf_ptr = inbuf.as_mut_ptr();
    let mut inputs: Vec<(CString, i64)> = Vec::new();
    loop {
        match lib::read_word(inbuf_ptr, 1024) {
            0 => {
                match inbuf[0] {
                    0 => { // Empty
                        inputs.push((CString::new("").expect(""), 0));
                    },
                    10 => { // Enter
                        println!(" {:?}", inputs);
                        inputs.clear();
                    },
                    _ => return,
                }
            },
            len => {
                let c_word = unsafe { CString::from_raw(inbuf_ptr) };
                inputs.push((c_word.clone(), len));
            },
        }
    }
}