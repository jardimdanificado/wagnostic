// zig_example — Exemplo de ROM em Zig para Wagnostic
// Compilar: zig build-lib -target wasm32-freestanding -dynamic -OReleaseFast zig_example.zig

const std = @import("std");

const Rect = extern struct {
    x: i32,
    y: i32,
    w: i32,
    h: i32,
};

const State = extern struct {
    width: u32,
    height: u32,
    bpp: u32,
    scale: u32,
    title: [128]u8,
    dirty_count: u32,
    dirty_rects: [32]Rect,
    mouse_x: i32,
    mouse_y: i32,
    mouse_buttons: u32,
    mouse_wheel: i32,
    keys: [256]u8,
    gamepad_buttons: u32,
    ticks: u32,
    target_fps: u32,
    audio_size: u32,
    audio_sample_rate: u32,
    audio_bpp: u32,
    audio_channels: u32,
    audio_write: u32,
    audio_read: u32,
    audio_underrun: u32,
    audio_overrun: u32,
    vram_offset: u32,
    audio_buffer_offset: u32,
    reserved: [40]u8,
};

const state_size = @sizeOf(State);
const vram_size = 320 * 240 * 2;

const Rom = extern struct {
    s: State,
    vram: [vram_size]u8,
};

export var rom: Rom = .{
    .s = .{
        .width = 0,
        .height = 0,
        .bpp = 0,
        .scale = 0,
        .title = [_]u8{0} ** 128,
        .dirty_count = 0,
        .dirty_rects = [_]Rect{.{ .x = 0, .y = 0, .w = 0, .h = 0 }} ** 32,
        .mouse_x = 0,
        .mouse_y = 0,
        .mouse_buttons = 0,
        .mouse_wheel = 0,
        .keys = [_]u8{0} ** 256,
        .gamepad_buttons = 0,
        .ticks = 0,
        .target_fps = 0,
        .audio_size = 0,
        .audio_sample_rate = 0,
        .audio_bpp = 0,
        .audio_channels = 0,
        .audio_write = 0,
        .audio_read = 0,
        .audio_underrun = 0,
        .audio_overrun = 0,
        .vram_offset = 0,
        .audio_buffer_offset = 0,
        .reserved = [_]u8{0} ** 40,
    },
    .vram = [_]u8{0} ** vram_size,
};

fn setPixel(x: i32, y: i32, r: u8, g: u8, b: u8) void {
    if (x < 0 or x >= 320 or y < 0 or y >= 240) return;
    
    const idx = @as(usize, @intCast(y)) * 320 + @as(usize, @intCast(x));
    const pixel = @as(u16, (@as(u16, r & 0xF8) << 8) | (@as(u16, g & 0xFC) << 3) | (@as(u16, b) >> 3));
    
    const vram_ptr = @as([*]u16, @ptrCast(&rom.vram));
    vram_ptr[idx] = pixel;
}

fn fillRect(rx: i32, ry: i32, rw: i32, rh: i32, r: u8, g: u8, b: u8) void {
    var y = ry;
    while (y < ry + rh) : (y += 1) {
        var x = rx;
        while (x < rx + rw) : (x += 1) {
            setPixel(x, y, r, g, b);
        }
    }
}

fn redraw() void {
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = .{ .x = 0, .y = 0, .w = 320, .h = 240 };
}

export fn wupdate() i32 {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.scale = 2;
        rom.s.vram_offset = state_size;
        
        // Definir título
        const title = "Zig ROM Example";
        for (title, 0..) |c, i| {
            rom.s.title[i] = c;
        }
        rom.s.title[title.len] = 0;
    }

    // Limpar tela (cinza escuro)
    fillRect(0, 0, 320, 240, 32, 32, 40);

    // Desenhar grade
    var x: i32 = 0;
    while (x < 320) : (x += 1) {
        var y: i32 = 0;
        while (y < 240) : (y += 1) {
            if (@rem(x, 32) == 0 or @rem(y, 32) == 0) {
                setPixel(x, y, 60, 60, 80);
            }
        }
    }

    // Desenhar retângulo na posição do mouse
    const mx = rom.s.mouse_x;
    const my = rom.s.mouse_y;
    
    // Retângulo branco com borda
    fillRect(mx - 10, my - 10, 21, 21, 255, 255, 255);
    fillRect(mx - 8, my - 8, 17, 17, 0, 120, 255);
    
    // Indicador de botão do mouse
    if ((rom.s.mouse_buttons & 1) != 0) {
        fillRect(mx - 5, my - 5, 11, 11, 255, 50, 50);
    }
    if ((rom.s.mouse_buttons & 2) != 0) {
        fillRect(mx - 3, my - 3, 7, 7, 50, 255, 50);
    }

    redraw();
    return @intCast(@intFromPtr(&rom.s));
}
