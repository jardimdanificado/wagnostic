// Wagnostic Runner — V + wasm3 + SDL2
//
// Build (from this directory):
//   cc -c -O2 -I../wasm3/wasm3/source ../wasm3/wasm3/source/*.c && \\
//     ar rcs libwasm3.a *.o && rm -f *.o
//   v -enable-globals -cc gcc host.v -o wagnostic-v
//
// Or from project root:
//   make -C runners/v
//
// Usage:
//   ./wagnostic-v <rom.wasm>

module main

import os

#flag -I../wasm3/wasm3/source
#flag -I/usr/include/SDL2
#flag -L.
#flag -lwasm3
#flag -lSDL2
#flag -lm

#include "wasm3.h"
#include "m3_env.h"
#include "SDL.h"
#include "miniz.h"

@[typedef]
struct C.mz_zip_archive {}

@[typedef]
struct C.mz_zip_archive_file_stat {
	m_uncomp_size u64
}

fn C.mz_zip_reader_init_file(pZip &C.mz_zip_archive, pFilename &char, flags u32) int
fn C.mz_zip_reader_locate_file(pZip &C.mz_zip_archive, pName &char, pComment &char, flags u32) int
fn C.mz_zip_reader_file_stat(pZip &C.mz_zip_archive, file_index u32, pStat &C.mz_zip_archive_file_stat) int
fn C.mz_zip_reader_extract_to_mem(pZip &C.mz_zip_archive, file_index u32, pBuf voidptr, buf_size usize, flags u32) int
fn C.mz_zip_reader_end(pZip &C.mz_zip_archive) int

// ============================================================
// wasm3 C declarations
// ============================================================

struct C.M3Environment {}

struct C.M3Runtime {}

struct C.M3Module {}

struct C.M3Function {}

fn C.m3_NewEnvironment() &C.M3Environment
fn C.m3_NewRuntime(env &C.M3Environment, stack_size u32, userdata voidptr) &C.M3Runtime
fn C.m3_ParseModule(env &C.M3Environment, module &&C.M3Module, wasm &u8, size u32) &char
fn C.m3_LoadModule(runtime &C.M3Runtime, module &C.M3Module) &char
fn C.m3_FindFunction(func &&C.M3Function, runtime &C.M3Runtime, name &char) &char
fn C.m3_CallV(func &C.M3Function, ...) &char
fn C.m3_GetResultsV(func &C.M3Function, ...) &char
fn C.m3_GetMemory(runtime &C.M3Runtime, out_len &u32, index u32) &u8
fn C.m3_FreeRuntime(runtime &C.M3Runtime)
fn C.m3_FreeEnvironment(env &C.M3Environment)

// ============================================================
// SDL2 C declarations
// ============================================================

fn C.SDL_LockTexture(texture voidptr, rect voidptr, pixels &voidptr, pitch &int) int
fn C.SDL_UnlockTexture(texture voidptr)
fn C.SDL_RenderClear(renderer voidptr) int
fn C.SDL_RenderCopy(renderer voidptr, texture voidptr, srcrect voidptr, dstrect voidptr) int
fn C.SDL_RenderPresent(renderer voidptr) int
fn C.SDL_GetWindowSize(window voidptr, w &int, h &int)
fn C.SDL_SetWindowSize(window voidptr, w int, h int)
fn C.SDL_SetWindowTitle(window voidptr, title &char)
fn C.SDL_DestroyTexture(texture voidptr)
fn C.SDL_SetTextureScaleMode(texture voidptr, mode int) int
fn C.SDL_GetTicks() u32
fn C.SDL_Delay(ms u32)
fn C.SDL_CreateMutex() voidptr
fn C.SDL_DestroyMutex(mutex voidptr)
fn C.SDL_LockMutex(mutex voidptr) int
fn C.SDL_UnlockMutex(mutex voidptr) int
fn C.SDL_CreateWindow(title &char, x int, y int, w int, h int, flags u32) voidptr
fn C.SDL_DestroyWindow(window voidptr)
fn C.SDL_CreateRenderer(window voidptr, index int, flags u32) voidptr
fn C.SDL_DestroyRenderer(renderer voidptr)
fn C.SDL_CreateTexture(renderer voidptr, format u32, access int, w int, h int) voidptr
fn C.SDL_PollEvent(event voidptr) int
fn C.SDL_OpenAudioDevice(device voidptr, iscapture int, desired voidptr, obtained voidptr, allowed_changes int) u32
fn C.SDL_CloseAudioDevice(dev u32)
fn C.SDL_PauseAudioDevice(dev u32, pause_on int)
fn C.SDL_Init(flags u32) int
fn C.SDL_Quit()
fn C.memset(dst voidptr, val int, len u64) voidptr
fn C.memcpy(dst voidptr, src voidptr, len u64) voidptr

// C structs from SDL2 headers
struct C.SDL_Rect {
	x int
	y int
	w int
	h int
}

struct C.SDL_AudioSpec {
mut:
	freq     int
	format   u16
	channels u8
	silence  u8
	samples  u16
	padding  u16
	size     u32
	callback voidptr
	userdata voidptr
}

// V equivalents of SDL defines
const sdl_init_video = 0x00000020
const sdl_init_audio = 0x00000010
const sdl_init_gamecontroller = 0x00002000
const sdl_windowpos_centered = 0x2FFF0000
const sdl_window_shown = 0x00000004
const sdl_window_resizable = 0x00000020
const sdl_renderer_accelerated = 0x00000002
const sdl_renderer_presentvsync = 0x00000004
const sdl_pixelfomat_abgr8888 = 0x16762004
const sdl_textureaccess_streaming = 1
const sdl_scalemode_nearest = 0
const audio_f32 = 0x8120
const sdl_quit = 0x100
const sdl_keydown = 0x300
const sdl_keyup = 0x301
const sdl_mousemotion = 0x400
const sdl_mousebuttondown = 0x401
const sdl_mousebuttonup = 0x402
const sdl_mousewheel = 0x403
const sdl_button_left = 1
const sdl_button_right = 3

// ============================================================
// Types
// ============================================================

struct Rect {
	x int
	y int
	w int
	h int
}

struct WagnosticState {
mut:
	width               u32
	height              u32
	bpp                 u32
	scale               u32
	title               [128]u8
	dirty_count         u32
	dirty_rects         [32]Rect
	mouse_x             i32
	mouse_y             i32
	mouse_buttons       u32
	mouse_wheel         i32
	keys                [256]u8
	gamepad_buttons     u32
	ticks               u32
	target_fps          u32
	audio_size          u32
	audio_sample_rate   u32
	audio_bpp           u32
	audio_channels      u32
	audio_write         u32
	audio_read          u32
	audio_underrun      u32
	audio_overrun       u32
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

struct LutTables {
mut:
	rgb332      [256]u32
	rgb565      [65536]u32
	initialized bool
}

// ============================================================
// Helpers
// ============================================================

fn get_state(mem voidptr, state_ptr u32) &WagnosticState {
	if state_ptr == 0 || mem == unsafe { nil } {
		return unsafe { nil }
	}
	return unsafe { &WagnosticState(voidptr(usize(mem) + usize(state_ptr))) }
}

fn get_vram(s &WagnosticState) voidptr {
	if s == unsafe { nil } || s.vram_offset == 0 {
		return unsafe { nil }
	}
	return unsafe { voidptr(usize(s) + usize(s.vram_offset)) }
}

fn get_audio_buffer(s &WagnosticState) voidptr {
	if s == unsafe { nil } || s.audio_buffer_offset == 0 {
		return unsafe { nil }
	}
	return unsafe { voidptr(usize(s) + usize(s.audio_buffer_offset)) }
}

fn read_screen_config(s &WagnosticState, mut gfx_w &u32, mut gfx_h &u32, mut gfx_bpp &u32, mut gfx_scale &u32) {
	unsafe {
		*gfx_w = if s != nil && s.width > 0 { s.width } else { 320 }
		*gfx_h = if s != nil && s.height > 0 { s.height } else { 240 }
		*gfx_bpp = if s != nil && s.bpp > 0 { s.bpp } else { 32 }
		*gfx_scale = if s != nil && s.scale > 0 { s.scale } else { 1 }
	}
}

// ============================================================
// Pixel conversion LUTs
// ============================================================

fn init_luts(mut luts LutTables) {
	if luts.initialized { return }
	for i in 0 .. 256 {
		r := u32((i >> 5) & 7) * 36
		g := u32((i >> 2) & 7) * 36
		b := u32(i & 3) * 85
		luts.rgb332[i] = 0xFF000000 | (b << 16) | (g << 8) | r
	}
	for i in 0 .. 65536 {
		r := u32((i >> 11) & 0x1F) * 255 / 31
		g := u32((i >> 5) & 0x3F) * 255 / 63
		b := u32(i & 0x1F) * 255 / 31
		luts.rgb565[i] = 0xFF000000 | (b << 16) | (g << 8) | r
	}
	luts.initialized = true
}

// ============================================================
// Render helpers
// ============================================================

fn render_rect_to_texture(texture voidptr, vram &u8, rx int, ry int, rw int, rh int,
	gfx_w u32, gfx_h u32, gfx_bpp u32, luts &LutTables) {
	unsafe {
		mut rx2 := rx
		mut ry2 := ry
		mut rw2 := rw
		mut rh2 := rh
		if rx2 < 0 {
			rw2 += rx2
			rx2 = 0
		}
		if ry2 < 0 {
			rh2 += ry2
			ry2 = 0
		}
		if rx2 + rw2 > int(gfx_w) { rw2 = int(gfx_w) - rx2 }
		if ry2 + rh2 > int(gfx_h) { rh2 = int(gfx_h) - ry2 }
		if rw2 <= 0 || rh2 <= 0 { return }

		mut pixels := nil
		mut pitch := 0
		sdl_rect := &C.SDL_Rect{rx2, ry2, rw2, rh2}
		C.SDL_LockTexture(texture, sdl_rect, &pixels, &pitch)

		if gfx_bpp == 8 {
			for y in ry2 .. ry2 + rh2 {
				src := vram + y * int(gfx_w) + rx2
				dst := &u32(&u8(pixels) + (y - ry2) * pitch)
				for x in 0 .. rw2 {
					dst[x] = luts.rgb332[src[x]]
				}
			}
		} else if gfx_bpp == 16 {
			for y in ry2 .. ry2 + rh2 {
				src := &u16(vram + (y * int(gfx_w) + rx2) * 2)
				dst := &u32(&u8(pixels) + (y - ry2) * pitch)
				for x in 0 .. rw2 {
					dst[x] = luts.rgb565[src[x]]
				}
			}
		} else if gfx_bpp == 32 {
			for y in ry2 .. ry2 + rh2 {
				src := vram + (y * int(gfx_w) + rx2) * 4
				dst := &u32(&u8(pixels) + (y - ry2) * pitch)
				C.memcpy(dst, src, rw2 * 4)
			}
		}

		C.SDL_UnlockTexture(texture)
	}
}

fn render_fullscreen(texture voidptr, vram &u8, gfx_w u32, gfx_h u32, gfx_bpp u32, luts &LutTables) {
	unsafe {
		mut pixels := nil
		mut pitch := 0
		C.SDL_LockTexture(texture, nil, &pixels, &pitch)

		if gfx_bpp == 8 {
			src := vram
			dst := &u32(pixels)
			total := gfx_w * gfx_h
			for i in 0 .. total {
				dst[i] = luts.rgb332[src[i]]
			}
		} else if gfx_bpp == 16 {
			src := &u16(vram)
			dst := &u32(pixels)
			total := gfx_w * gfx_h
			for i in 0 .. total {
				dst[i] = luts.rgb565[src[i]]
			}
		} else if gfx_bpp == 32 {
			C.memcpy(pixels, vram, gfx_w * gfx_h * 4)
		}

		C.SDL_UnlockTexture(texture)
	}
}

// ============================================================
// Letterbox
// ============================================================

fn calc_letterbox(win_w int, win_h int, gw u32, gh u32, dst &C.SDL_Rect) {
	aspect_rom := f32(gw) / f32(gh)
	aspect_win := f32(win_w) / f32(win_h)
	unsafe {
		if aspect_win > aspect_rom {
			dst.h = win_h
			dst.w = int(f32(win_h) * aspect_rom)
			dst.x = (win_w - dst.w) / 2
			dst.y = 0
		} else {
			dst.w = win_w
			dst.h = int(f32(win_w) / aspect_rom)
			dst.x = 0
			dst.y = (win_h - dst.h) / 2
		}
	}
}

// ============================================================
// Mouse conversion
// ============================================================

fn convert_mouse_coords(wx int, wy int, mut rx &int, mut ry &int, win_w int, win_h int, gw u32, gh u32) {
	mut dst := C.SDL_Rect{}
	calc_letterbox(win_w, win_h, gw, gh, &dst)
	scale_x := f32(gw) / f32(dst.w)
	scale_y := f32(gh) / f32(dst.h)
	unsafe {
		*rx = int((f32(wx - dst.x) * scale_x))
		*ry = int((f32(wy - dst.y) * scale_y))
		if *rx < 0 { *rx = 0 }
		if *rx >= int(gw) { *rx = int(gw) - 1 }
		if *ry < 0 { *ry = 0 }
		if *ry >= int(gh) { *ry = int(gh) - 1 }
	}
}

// ============================================================
// Audio callback
// ============================================================

__global g_state_ptr u32
__global g_mem voidptr
__global g_audio_mutex voidptr

fn host_audio_callback(_userdata voidptr, stream_ptr &u8, len_bytes int) {
	unsafe {
		if g_audio_mutex != nil {
			C.SDL_LockMutex(g_audio_mutex)
		}

		s := get_state(g_mem, g_state_ptr)
		if s == nil {
		} else {
		}
		abuf := &u8(get_audio_buffer(s))
		if abuf == nil {
		} else {
		}

		mut size := u32(0)
		mut bpp := u32(0)
		mut r_off := u32(0)
		mut w_off := u32(0)
		if s != nil {
			size = s.audio_size
			bpp = s.audio_bpp
			r_off = s.audio_read
			w_off = s.audio_write
		}

		if abuf == nil || size == 0 || bpp == 0 || bpp > 4 {
			C.memset(stream_ptr, 0, u64(len_bytes))
			if g_audio_mutex != nil {
				C.SDL_UnlockMutex(g_audio_mutex)
			}
			return
		}

		if r_off >= size { r_off = 0 }

		stream := &f32(stream_ptr)
		nsamples := len_bytes / 4

		mut stream_idx := 0
		for stream_idx < nsamples {
			if r_off == w_off {
				stream[stream_idx] = 0.0
				stream_idx++
				continue
			}
			if bpp == 1 {
				stream[stream_idx] = (f32(abuf[r_off]) - 128.0) / 128.0
				r_off = (r_off + 1) % size
			} else if bpp == 2 {
				mut s16 := i16(0)
				if r_off + 1 < size {
					C.memcpy(&s16, &abuf[r_off], 2)
				}
				stream[stream_idx] = f32(s16) / 32768.0
				r_off = (r_off + 2) % size
			} else {
				mut sample := f32(0)
				if r_off + 3 < size {
					C.memcpy(&sample, &abuf[r_off], 4)
				}
				stream[stream_idx] = sample
				r_off = (r_off + 4) % size
			}
			stream_idx++
		}

		if s != nil {
			s.audio_read = r_off
		}

		if g_audio_mutex != nil {
			C.SDL_UnlockMutex(g_audio_mutex)
		}
	}
}

// ============================================================
// main
// ============================================================

fn main() {
	if C.SDL_Init(u32(sdl_init_video | sdl_init_audio | sdl_init_gamecontroller)) < 0 {
		eprintln('SDL_Init failed')
		return
	}
	defer { C.SDL_Quit() }


	if os.args.len < 2 {
		println('Usage: wagnostic-v <rom.wasm>')
		return
	}

	rom_path := os.args[1]
	mut is_zip := false
	mut zip_archive := C.mz_zip_archive{}
	mut rom_bytes := []u8{}
	
	if C.mz_zip_reader_init_file(&zip_archive, &char(rom_path.str), 0) != 0 {
		is_zip = true
		file_index := C.mz_zip_reader_locate_file(&zip_archive, c"main.wasm", unsafe { nil }, 0)
		if file_index < 0 {
			eprintln('Cannot find main.wasm in WAG file: ${rom_path}')
			return
		}
		mut file_stat := C.mz_zip_archive_file_stat{}
		C.mz_zip_reader_file_stat(&zip_archive, u32(file_index), &file_stat)
		rom_bytes = []u8{len: int(file_stat.m_uncomp_size)}
		if C.mz_zip_reader_extract_to_mem(&zip_archive, u32(file_index), unsafe { &rom_bytes[0] }, usize(file_stat.m_uncomp_size), 0) == 0 {
			eprintln('Failed to extract main.wasm')
			return
		}
	} else {
		rom_bytes = os.read_bytes(rom_path) or {
			println('Failed to read ROM file')
			return
		}
	}

	// Init Wasm3
	env := C.m3_NewEnvironment()
	if env == unsafe { nil } {
		eprintln('m3_NewEnvironment failed')
		return
	}
	defer { C.m3_FreeEnvironment(env) }

	runtime := C.m3_NewRuntime(env, u32(64 * 1024 * 1024), unsafe { nil })
	if runtime == unsafe { nil } {
		eprintln('m3_NewRuntime failed')
		return
	}
	defer { C.m3_FreeRuntime(runtime) }

	mut module_ := &C.M3Module(unsafe { nil })
	mut res := C.m3_ParseModule(env, &module_, unsafe { &rom_bytes[0] }, u32(rom_bytes.len))
	if res != unsafe { nil } {
		eprintln('Parse error: ${unsafe { cstring_to_vstring(res) }}')
		return
	}

	res = C.m3_LoadModule(runtime, module_)
	if res != unsafe { nil } {
		eprintln('Load error: ${unsafe { cstring_to_vstring(res) }}')
		return
	}

	mut f_wupdate := &C.M3Function(unsafe { nil })
	res3 := C.m3_FindFunction(&f_wupdate, runtime, c'wupdate')
	if res3 != unsafe { nil } || f_wupdate == unsafe { nil } {
		eprintln('ROM does not export wupdate()')
		return
	}

	// Get memory
	println("Get memory")
	mut mem_len := u32(0)
	g_mem = voidptr(C.m3_GetMemory(runtime, &mem_len, 0))
	println("Get memory returned")

	// Pixel LUTs
	mut luts := LutTables{}
	init_luts(mut luts)

	// Audio mutex
	g_audio_mutex = C.SDL_CreateMutex()
	if g_audio_mutex == unsafe { nil } {
		eprintln('SDL_CreateMutex failed')
		return
	}
	defer {
		C.SDL_DestroyMutex(g_audio_mutex)
		g_audio_mutex = unsafe { nil }
	}

	// Call wupdate() once for initial state
	mut keep := i32(0)
	unsafe {
		C.SDL_LockMutex(g_audio_mutex)
		println("Calling initial wupdate")
		C.m3_CallV(f_wupdate)
		println("Initial wupdate returned")
		C.m3_GetResultsV(f_wupdate, &keep)
		println("m3_GetResultsV returned")
		g_state_ptr = u32(keep)
		g_mem = voidptr(C.m3_GetMemory(runtime, &mem_len, 0))
		println("m3_GetMemory returned. g_mem: ${g_mem}")
		C.SDL_UnlockMutex(g_audio_mutex)
	}
	println("get_state")
	mut state := get_state(g_mem, g_state_ptr)
	println("get_state returned. g_state_ptr: ${g_state_ptr}, mem_len: ${mem_len}")
	if state == unsafe { nil } {
		println("state is nil")
	} else {
		println("state is not nil")
	}

	// Initial config
	mut gfx_w := u32(0)
	mut gfx_h := u32(0)
	mut gfx_bpp := u32(0)
	mut gfx_scale := u32(0)
	read_screen_config(state, mut &gfx_w, mut &gfx_h, mut &gfx_bpp, mut &gfx_scale)
	println("read_screen_config returned")

	mut title_arr := [128]u8{}
	if state != unsafe { nil } {
		unsafe { C.memcpy(&title_arr[0], &state.title[0], 128) }
	}
	
	mut title_len := 0
	for i in 0 .. 128 {
		if title_arr[i] == 0 {
			title_len = i
			break
		}
	}
	if title_len == 0 && title_arr[0] != 0 { title_len = 128 }
		mut window_title_c := [129]char{}
		for i in 0 .. title_len {
			window_title_c[i] = char(title_arr[i])
		}
		window_title_c[title_len] = 0
		mut window_title := title_arr[0..title_len].bytestr()
		println("Parsed window title")

		println("Creating window with title: '${window_title}' (${gfx_w}x${gfx_h} scale ${gfx_scale})")
		window := C.SDL_CreateWindow(&char(&window_title_c[0]), sdl_windowpos_centered, sdl_windowpos_centered,
			int(gfx_w * gfx_scale), int(gfx_h * gfx_scale),
			u32(sdl_window_shown))
		println("Created window")
	if window == unsafe { nil } {
		eprintln('SDL_CreateWindow failed')
		return
	}
	defer { C.SDL_DestroyWindow(window) }

	renderer := C.SDL_CreateRenderer(window, -1,
		u32(sdl_renderer_accelerated | sdl_renderer_presentvsync))
	if renderer == unsafe { nil } {
		eprintln('SDL_CreateRenderer failed')
		return
	}
	defer { C.SDL_DestroyRenderer(renderer) }

	mut texture := C.SDL_CreateTexture(renderer, sdl_pixelfomat_abgr8888,
		sdl_textureaccess_streaming, int(gfx_w), int(gfx_h))
	if texture == unsafe { nil } {
		eprintln('SDL_CreateTexture failed')
		return
	}
	defer { C.SDL_DestroyTexture(texture) }
	C.SDL_SetTextureScaleMode(texture, sdl_scalemode_nearest)

	// Audio device
	mut audio_dev := u32(0)
	mut prev_audio_size := u32(0)
	mut prev_audio_rate := u32(0)
	mut prev_audio_bpp := u32(0)
	mut prev_audio_channels := u32(0)
	if state != unsafe { nil } {
		prev_audio_size = state.audio_size
		prev_audio_rate = state.audio_sample_rate
		prev_audio_bpp = state.audio_bpp
		prev_audio_channels = state.audio_channels
	}

	if prev_audio_size > 0 {
		mut wanted := C.SDL_AudioSpec{
			freq: if prev_audio_rate > 0 { int(prev_audio_rate) } else { 44100 }
			format: u16(audio_f32)
			channels: if prev_audio_channels > 0 { u8(prev_audio_channels) } else { 1 }
			samples: 1024
			callback: voidptr(host_audio_callback)
		}
		unsafe {
			audio_dev = C.SDL_OpenAudioDevice(nil, 0, &wanted, nil, 0)
		}
		if audio_dev > 0 {
			C.SDL_PauseAudioDevice(audio_dev, 0)
		}
	}
	defer {
		if audio_dev > 0 {
			C.SDL_CloseAudioDevice(audio_dev)
		}
	}

	mut prev_gfx_w := gfx_w
	mut prev_gfx_h := gfx_h
	mut prev_gfx_bpp := gfx_bpp
	mut prev_gfx_scale := gfx_scale

	mut keys_state := [256]u8{}
	mut mouse_buttons := u32(0)
	mut mouse_x := 0
	mut mouse_y := 0
	mut mouse_wheel := 0
	mut gamepad_buttons := u32(0)

	mut running := true
	mut frame_start := u32(0)

	for running {
		// Process SDL events
		mut ev := [56]u8{} // SDL_Event is 56 bytes
		for C.SDL_PollEvent(voidptr(&ev[0])) != 0 {
			ev_type := unsafe { *(&u32(&ev[0])) }
			if ev_type == sdl_quit {
				running = false
			} else if ev_type == sdl_keydown || ev_type == sdl_keyup {
				// scancode at offset 16: type(4) + timestamp(4) + windowID(4) + state(1) + repeat(1) + padding(2)
				sc := unsafe { *(&u32(&ev[16])) }
				if sc < 256 {
					keys_state[sc] = if ev_type == sdl_keydown { u8(1) } else { u8(0) }
				}
			} else if ev_type == sdl_mousemotion {
				mx := unsafe { *(&int(&ev[20])) }
				my := unsafe { *(&int(&ev[24])) }
				mut win_w := 0
				mut win_h := 0
				C.SDL_GetWindowSize(window, &win_w, &win_h)
				convert_mouse_coords(mx, my, mut &mouse_x, mut &mouse_y, win_w, win_h, gfx_w, gfx_h)
			} else if ev_type == sdl_mousebuttondown || ev_type == sdl_mousebuttonup {
				pressed := ev_type == sdl_mousebuttondown
				btn := unsafe { *(&u8(&ev[16])) }
				if btn == sdl_button_left {
					if pressed {
						mouse_buttons |= 1
					} else {
						mouse_buttons &= ~u32(1)
					}
				} else if btn == sdl_button_right {
					if pressed {
						mouse_buttons |= 2
					} else {
						mouse_buttons &= ~u32(2)
					}
				}
			} else if ev_type == sdl_mousewheel {
				scroll := unsafe { *(&int(&ev[20])) }
				mouse_wheel += scroll
			}
		}

		// Write input
		state = get_state(g_mem, g_state_ptr)
		if state != unsafe { nil } {
			unsafe {
				C.memcpy(&state.keys[0], &keys_state[0], 256)
				state.mouse_x = i32(mouse_x)
				state.mouse_y = i32(mouse_y)
				state.mouse_buttons = mouse_buttons
				state.mouse_wheel = i32(mouse_wheel)
				state.gamepad_buttons = gamepad_buttons
				state.ticks = C.SDL_GetTicks()
			}
		}

		// Call wupdate()
		unsafe {
			C.SDL_LockMutex(g_audio_mutex)
			C.m3_CallV(f_wupdate)
			C.m3_GetResultsV(f_wupdate, &keep)
			if keep != 0 {
				g_state_ptr = u32(keep)
				// Refresh memory (ROM may have grown it)
				g_mem = voidptr(C.m3_GetMemory(runtime, &mem_len, 0))
			}
			C.SDL_UnlockMutex(g_audio_mutex)
		}
		if keep == 0 { break }
		
		state = get_state(g_mem, g_state_ptr)

		// Read config
		read_screen_config(state, mut &gfx_w, mut &gfx_h, mut &gfx_bpp, mut &gfx_scale)

		config_changed := gfx_w != prev_gfx_w || gfx_h != prev_gfx_h || gfx_bpp != prev_gfx_bpp
			|| gfx_scale != prev_gfx_scale

		mut new_title_arr := [128]u8{}
		if state != unsafe { nil } {
			unsafe { C.memcpy(&new_title_arr[0], &state.title[0], 128) }
		}
		new_title_str := unsafe { tos_clone(&u8(&new_title_arr[0])) }
		title_changed := new_title_str != window_title

		if config_changed || title_changed {
			C.SDL_SetWindowSize(window, int(gfx_w * gfx_scale), int(gfx_h * gfx_scale))
			if title_changed {
				C.SDL_SetWindowTitle(window, new_title_str.str)
				window_title = new_title_str
			}
			C.SDL_DestroyTexture(texture)
			texture = C.SDL_CreateTexture(renderer, sdl_pixelfomat_abgr8888,
				sdl_textureaccess_streaming, int(gfx_w), int(gfx_h))
			C.SDL_SetTextureScaleMode(texture, sdl_scalemode_nearest)
			prev_gfx_w = gfx_w
			prev_gfx_h = gfx_h
			prev_gfx_bpp = gfx_bpp
			prev_gfx_scale = gfx_scale

			mut win_w := 0
			mut win_h := 0
			C.SDL_GetWindowSize(window, &win_w, &win_h)
			convert_mouse_coords(mouse_x, mouse_y, mut &mouse_x, mut &mouse_y, win_w, win_h, gfx_w,
				gfx_h)
		}

		// Audio config change
		if state != unsafe { nil } {
			cur_size := state.audio_size
			cur_rate := state.audio_sample_rate
			cur_bpp := state.audio_bpp
			cur_channels := state.audio_channels

			if cur_size != prev_audio_size || cur_rate != prev_audio_rate
				|| cur_bpp != prev_audio_bpp || cur_channels != prev_audio_channels {
				if audio_dev > 0 {
					C.SDL_CloseAudioDevice(audio_dev)
					audio_dev = 0
				}
				if cur_size > 0 && cur_rate > 0 && cur_channels > 0 {
					mut wanted := C.SDL_AudioSpec{
						freq: int(cur_rate)
						format: u16(audio_f32)
						channels: u8(cur_channels)
						samples: 1024
						callback: voidptr(host_audio_callback)
					}
					unsafe {
						audio_dev = C.SDL_OpenAudioDevice(nil, 0, &wanted, nil, 0)
					}
					if audio_dev > 0 {
						C.SDL_PauseAudioDevice(audio_dev, 0)
					}
				}
				prev_audio_size = cur_size
				prev_audio_rate = cur_rate
				prev_audio_bpp = cur_bpp
				prev_audio_channels = cur_channels
			}
		}

		// Render
		if state != unsafe { nil } {
			vram := unsafe { &u8(get_vram(state)) }
			dirty_count := state.dirty_count

			if vram != unsafe { nil } && dirty_count > 0 {
				if dirty_count == 1 {
					rx := state.dirty_rects[0].x
					ry := state.dirty_rects[0].y
					rw := state.dirty_rects[0].w
					rh := state.dirty_rects[0].h
					if rx == 0 && ry == 0 && u32(rw) == gfx_w && u32(rh) == gfx_h {
						render_fullscreen(texture, vram, gfx_w, gfx_h, gfx_bpp, &luts)
					} else {
						render_rect_to_texture(texture, vram, rx, ry, rw, rh, gfx_w, gfx_h,
							gfx_bpp, &luts)
					}
				} else {
					count := if dirty_count > 32 { 32 } else { int(dirty_count) }
					for i in 0 .. count {
						rx := state.dirty_rects[i].x
						ry := state.dirty_rects[i].y
						rw := state.dirty_rects[i].w
						rh := state.dirty_rects[i].h
						render_rect_to_texture(texture, vram, rx, ry, rw, rh, gfx_w, gfx_h,
							gfx_bpp, &luts)
					}
				}

				mut win_w := 0
				mut win_h := 0
				C.SDL_GetWindowSize(window, &win_w, &win_h)
				mut dst := C.SDL_Rect{}
				calc_letterbox(win_w, win_h, gfx_w, gfx_h, &dst)

				C.SDL_RenderClear(renderer)
				C.SDL_RenderCopy(renderer, texture, unsafe { nil }, &dst)
				C.SDL_RenderPresent(renderer)

				state.dirty_count = 0
			}
		}

		// Process IO
		if state != unsafe { nil } && is_zip {
			if state.io_load != 0 && state.io_load < mem_len {
				path_ptr := unsafe { &char(usize(g_mem) + usize(state.io_load)) }
				file_index := C.mz_zip_reader_locate_file(&zip_archive, path_ptr, unsafe { nil }, 0)
				if file_index >= 0 {
					mut file_stat := C.mz_zip_archive_file_stat{}
					C.mz_zip_reader_file_stat(&zip_archive, u32(file_index), &file_stat)
					file_sz := u32(file_stat.m_uncomp_size)
					
					if state.io_load_buffer == 0 {
						state.io_load_size = file_sz
					} else if state.io_load_buffer + file_sz <= mem_len {
						dest_ptr := unsafe { voidptr(usize(g_mem) + usize(state.io_load_buffer)) }
						C.mz_zip_reader_extract_to_mem(&zip_archive, u32(file_index), dest_ptr, usize(file_sz), 0)
					}
				} else {
					if state.io_load_buffer == 0 {
						state.io_load_size = 0
					}
				}
				state.io_load = 0
			}

			if state.io_save != 0 && state.io_save < mem_len {
				state.io_save = 0
			}
		}

		// Reset mouse wheel
		if state != unsafe { nil } {
			state.mouse_wheel = 0
		}
		mouse_wheel = 0

		// FPS limit
		if state != unsafe { nil } && state.target_fps > 0 {
			now := C.SDL_GetTicks()
			elapsed := now - frame_start
			delay := (1000 / state.target_fps) - elapsed
			if delay > 0 {
				C.SDL_Delay(delay)
			}
			frame_start = C.SDL_GetTicks()
		} else {
			C.SDL_Delay(1)
		}
	}
}
