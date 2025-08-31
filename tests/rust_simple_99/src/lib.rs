#![no_std]

#[no_mangle]
pub extern "C" fn get_number() -> i32 { 99 }

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! { loop {} }