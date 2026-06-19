-- Wagnostic LÖVE Benchmark
-- Usage: love <path/to/this/dir> <rom.wasm> [num_frames]

local ffi = require("ffi")

ffi.cdef[[
    typedef const char* M3Result;
    typedef struct M3Environment* IM3Environment;
    typedef struct M3Runtime* IM3Runtime;
    typedef struct M3Module* IM3Module;
    typedef struct M3Function* IM3Function;

    IM3Environment m3_NewEnvironment();
    IM3Runtime m3_NewRuntime(IM3Environment, uint32_t, void*);
    M3Result m3_ParseModule(IM3Environment, IM3Module*, const uint8_t*, uint32_t);
    M3Result m3_LoadModule(IM3Runtime, IM3Module);
    M3Result m3_FindFunction(IM3Function*, IM3Runtime, const char*);
    M3Result m3_Call(IM3Function, uint32_t, const void*[]);
    uint8_t* m3_GetMemory(IM3Runtime, uint32_t*, uint32_t);
]]

local m3 = ffi.load("libwasm3")
local W, H, BPP = 320, 240, 8
local runtime, memory, f_wupdate
local frame_count = 0
local num_frames = 100
local start_time = 0
local total_time_ns = 0
local min_frame = 1e9
local max_frame = 0
local bench_done = false

local function now_ns()
    return love.timer.getTime() * 1e9
end

function love.load(args)
    local rom_path = args[1]  -- args[1] is the first user argument
    if not rom_path or rom_path == "" then
        print("Usage: love <script_dir> <rom.wasm> [num_frames]")
        love.event.quit()
        return
    end
    
    num_frames = tonumber(args[2]) or 100
    
    -- Load ROM
    local f = io.open(rom_path, "rb")
    if not f then
        print("ERROR: Cannot open " .. rom_path)
        love.event.quit()
        return
    end
    local data = f:read("*a")
    f:close()
    
    local env = m3.m3_NewEnvironment()
    runtime = m3.m3_NewRuntime(env, 64 * 1024 * 1024, nil)
    local mod_ptr = ffi.new("IM3Module[1]")
    local res = m3.m3_ParseModule(env, mod_ptr, data, #data)
    if res ~= nil then print("ERROR: Parse failed"); love.event.quit(); return end
    res = m3.m3_LoadModule(runtime, mod_ptr[0])
    if res ~= nil then print("ERROR: Load failed"); love.event.quit(); return end
    
    local func_ptr = ffi.new("IM3Function[1]")
    m3.m3_FindFunction(func_ptr, runtime, "winit")
    local f_winit = func_ptr[0]
    m3.m3_FindFunction(func_ptr, runtime, "wupdate")
    f_wupdate = func_ptr[0]
    
    if not f_wupdate then
        print("ERROR: ROM has no wupdate")
        love.event.quit()
        return
    end
    
    memory = m3.m3_GetMemory(runtime, nil, 0)
    
    -- Call winit
    if f_winit then m3.m3_Call(f_winit, 0, nil) end
    memory = m3.m3_GetMemory(runtime, nil, 0)
    
    -- Read config
    W = ffi.cast("uint32_t*", memory + 128)[0] or 320
    H = ffi.cast("uint32_t*", memory + 132)[0] or 240
    BPP = ffi.cast("uint32_t*", memory + 136)[0] or 8
    
    local vram_bytes = W * H * (BPP / 8)
    local sys = ffi.cast("uint8_t*", memory)
    local title = ffi.string(sys, 128)
    local has_audio = ffi.cast("uint32_t*", memory + 144)[0] > 0
    
    print("========================================")
    print("  WAGNOSTIC BENCHMARK (LÖVE + wasm3)")
    print("========================================")
    print("ROM:          " .. rom_path)
    print("Title:        " .. title)
    print(string.format("Resolution:   %dx%d @ %dbpp", W, H, BPP))
    print(string.format("VRAM:         %d KB", vram_bytes / 1024))
    if has_audio then
        print(string.format("Audio:        %d Hz, %d-bit, %d ch",
            ffi.cast("uint32_t*", memory + 156)[0],
            ffi.cast("uint32_t*", memory + 160)[0] * 8,
            ffi.cast("uint32_t*", memory + 164)[0]))
    else
        print("Audio:        none")
    end
    print(string.format("Frames:       %d", num_frames))
    print("Engine:       LÖVE + wasm3 (interpretador)")
    print("========================================")
    print("Running benchmark...")
    
    start_time = now_ns()
end

function love.update(dt)
    if bench_done then return end
    if not f_wupdate then return end
    
    for i = 1, 5 do  -- Process 5 frames per update to speed up
        if frame_count >= num_frames then break end
        
        local frame_start = now_ns()
        m3.m3_Call(f_wupdate, 0, nil)
        local frame_end = now_ns()
        
        -- Refresh memory pointer and clear signals
        memory = m3.m3_GetMemory(runtime, nil, 0)
        local sigs = ffi.cast("uint8_t*", memory + 464)
        sigs[0] = 0; sigs[1] = 0; sigs[2] = 0; sigs[3] = 0
        ffi.cast("int32_t*", memory + 460)[0] = 0
        
        local frame_ns = frame_end - frame_start
        total_time_ns = total_time_ns + frame_ns
        if frame_ns < min_frame then min_frame = frame_ns end
        if frame_ns > max_frame then max_frame = frame_ns end
        
        frame_count = frame_count + 1
        
        if frame_count % 10 == 0 or frame_count == num_frames then
            local avg = total_time_ns / frame_count / 1e6
            io.write(string.format("\r  Frame %d/%d (%.1f ms/frame avg)", frame_count, num_frames, avg))
            io.flush()
        end
    end
    
    if frame_count >= num_frames then
        bench_done = true
        
        local total_ms = total_time_ns / 1e6
        local avg_ms = total_time_ns / num_frames / 1e6
        local min_ms = min_frame / 1e6
        local max_ms = max_frame / 1e6
        local fps = 1000.0 / avg_ms
        local pixels_per_sec = (W * H * num_frames) / (total_ms / 1000.0)
        local mpps = pixels_per_sec / 1e6
        local bw = (W * H * (BPP / 8) * num_frames) / total_ms * 1000.0 / (1024.0 * 1024.0)
        
        print("\n\n")
        print("========================================")
        print("  RESULTS (LÖVE + wasm3)")
        print("========================================")
        print(string.format("Total time:       %.2f ms", total_ms))
        print(string.format("Avg frame time:   %.3f ms", avg_ms))
        print(string.format("Min frame time:   %.3f ms", min_ms))
        print(string.format("Max frame time:   %.3f ms", max_ms))
        print(string.format("Avg FPS:          %.1f", fps))
        print(string.format("Pixels/sec:       %.2f MP/s", mpps))
        print(string.format("VRAM per frame:   %.2f KB", (W * H * (BPP / 8)) / 1024.0))
        print(string.format("VRAM bandwidth:   %.2f MB/s", bw))
        print("========================================")
        
        love.event.quit()
    end
end

-- Minimal draw to avoid LÖVE warnings
function love.draw()
    love.graphics.clear(0, 0, 0)
end
