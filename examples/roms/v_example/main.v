// v_example — Exemplo de ROM em V para Wagnostic
// Compilar: v -enable-globals -os wasm32 -o v_example.wasm main.v

module main

#flag -I.
#include "string_impl.h"

struct Rect {
    x int
    y int
    w int
    h int
}

struct State {
mut:
    width          u32
    height         u32
    bpp            u32
    scale          u32
    title          [128]u8
    dirty_count    u32
    dirty_rects    [32]Rect
    mouse_x        i32
    mouse_y        i32
    mouse_buttons  u32
    mouse_wheel    i32
    keys           [256]u8
    gamepad_buttons u32
    ticks          u32
    target_fps     u32
    audio_size     u32
    audio_sample_rate u32
    audio_bpp      u32
    audio_channels u32
    audio_write    u32
    audio_read     u32
    audio_underrun u32
    audio_overrun  u32
    vram_offset         u32
    audio_buffer_offset u32
    io_load             u32
    io_load_buffer      u32
    io_load_size        u32
    io_save             u32
    io_save_buffer      u32
    io_save_size        u32
    reserved            [16]u8
}

const state_size = sizeof(State)
const vram_size = 320 * 240 * 2

struct Rom {
mut:
    s    State
    vram [vram_size]u8
}

__global rom = Rom{}

fn set_pixel(x int, y int, r u8, g u8, b u8) {
    if x < 0 || x >= 320 || y < 0 || y >= 240 {
        return
    }
    idx := y * 320 + x
    pixel := u16(((u16(r) & 0xF8) << 8) | ((u16(g) & 0xFC) << 3) | (u16(b) >> 3))
    unsafe {
        p := &u16(&rom.vram[0])
        p[idx] = pixel
    }
}

fn fill_rect(rx int, ry int, rw int, rh int, r u8, g u8, b u8) {
    for y in ry .. ry + rh {
        for x in rx .. rx + rw {
            set_pixel(x, y, r, g, b)
        }
    }
}

fn redraw() {
    rom.s.dirty_count = 1
    rom.s.dirty_rects[0] = Rect{0, 0, 320, 240}
}

@[export: 'wupdate']
fn wupdate() int {
    if rom.s.width == 0 {
        rom.s.width = 320
        rom.s.height = 240
        rom.s.bpp = 16
        rom.s.scale = 2
        rom.s.vram_offset = u32(state_size)
        
        // Definir título
        title := c'V ROM Example'
        mut i := 0
        unsafe {
            for title[i] != 0 {
                rom.s.title[i] = title[i]
                i++
            }
        }
        rom.s.title[i] = 0
    }

    // Limpar tela (cinza escuro)
    fill_rect(0, 0, 320, 240, 32, 32, 40)

    // Desenhar grade
    for x in 0 .. 320 {
        for y in 0 .. 240 {
            if x % 32 == 0 || y % 32 == 0 {
                set_pixel(x, y, 60, 60, 80)
            }
        }
    }

    // Desenhar retângulo na posição do mouse
    mx := int(rom.s.mouse_x)
    my := int(rom.s.mouse_y)
    
    // Retângulo branco com borda
    fill_rect(mx - 10, my - 10, 21, 21, 255, 255, 255)
    fill_rect(mx - 8, my - 8, 17, 17, 0, 120, 255)
    
    // Indicador de botão do mouse
    if (rom.s.mouse_buttons & 1) != 0 {
        fill_rect(mx - 5, my - 5, 11, 11, 255, 50, 50)
    }
    if (rom.s.mouse_buttons & 2) != 0 {
        fill_rect(mx - 3, my - 3, 7, 7, 50, 255, 50)
    }

    redraw()
    return int(&rom.s)
}
