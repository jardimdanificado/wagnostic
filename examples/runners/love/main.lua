local ffi = require("ffi")

-- Wasm3 FFI Definitions
ffi.cdef[[
    typedef const char* M3Result;
    typedef struct M3Environment* IM3Environment;
    typedef struct M3Runtime* IM3Runtime;
    typedef struct M3Module* IM3Module;
    typedef struct M3Function* IM3Function;

    IM3Environment m3_NewEnvironment();
    IM3Runtime m3_NewRuntime(IM3Environment i_environment, uint32_t i_stackSizeInBytes, void* i_userdata);
    M3Result m3_ParseModule(IM3Environment i_environment, IM3Module* o_module, const uint8_t* i_wasmBytes, uint32_t i_numWasmBytes);
    M3Result m3_LoadModule(IM3Runtime io_runtime, IM3Module io_module);
    M3Result m3_FindFunction(IM3Function* o_function, IM3Runtime i_runtime, const char* i_functionName);
    M3Result m3_Call(IM3Function i_function, uint32_t i_argc, const void* i_argptrs[]);
    uint8_t* m3_GetMemory(IM3Runtime i_runtime, uint32_t* o_memorySizeInBytes, uint32_t i_memoryIndex);
]]

local m3 = ffi.load("examples/runners/love/libwasm3.so")
local bit = require("bit")
local bitand, bitor, bitshift, rshift = bit.band, bit.bor, bit.lshift, bit.rshift

-- State
local env, runtime, module, f_winit, f_wupdate, memory
local image, imageData
local rom_loaded = false
local wheel_delta = 0

-- Offsets
local OFF_WIDTH, OFF_HEIGHT, OFF_BPP, OFF_SCALE = 128, 132, 136, 140
local OFF_TICK, OFF_GAMEPAD, OFF_KEYS, OFF_SIGNALS, OFF_VRAM = 168, 172, 192, 464, 512

-- Audio State
local audioSource = nil
local audio_rate, audio_channels, audio_bpp = 0, 0, 0

-- Full USB HID Scancode Map
local scancode_map = {
    ["return"]=40, escape=41, backspace=42, tab=43, space=44,
    ["-"]=45, ["="]=46, ["["]=47, ["]"]=48, ["\\"]=49, [";"]=51, ["'"]=52, ["`"]=53, [","]=54, ["."]=55, ["/"]=56,
    capslock=57, f1=58, f2=59, f3=60, f4=61, f5=62, f6=63, f7=64, f8=65, f9=66, f10=67, f11=68, f12=69,
    printscreen=70, scrolllock=71, pause=72, insert=73, home=74, pageup=75, delete=76, ["end"]=77, pagedown=78,
    right=79, left=80, down=81, up=82, numlock=83,
    ["kp/"]=84, ["kp*"]=85, ["kp-"]=86, ["kp+"]=87, kpenter=88,
    kp1=89, kp2=90, kp3=91, kp4=92, kp5=93, kp6=94, kp7=95, kp8=96, kp9=97, kp0=98, ["kp."]=99,
    lctrl=224, lshift=225, lalt=226, lgui=227, rctrl=228, rshift=229, ralt=230, rgui=231
}
-- Letters A-Z (4-29)
for i=0,25 do scancode_map[string.char(97+i)] = 4+i end
-- Numbers 1-9 (30-38), 0 (39)
for i=1,9 do scancode_map[tostring(i)] = 29+i end
scancode_map["0"] = 39

local function update_dimensions()
    local w, h = ffi.cast("uint32_t*", memory + OFF_WIDTH)[0], ffi.cast("uint32_t*", memory + OFF_HEIGHT)[0]
    local sc = ffi.cast("uint32_t*", memory + OFF_SCALE)[0]
    if w == 0 or h == 0 then return end
    if not imageData or imageData:getWidth() ~= w or imageData:getHeight() ~= h then
        imageData = love.image.newImageData(w, h)
        image = love.graphics.newImage(imageData)
        image:setFilter("nearest", "nearest")
        love.window.setMode(w * sc, h * sc, {highdpi = false, resizable = true})
    end
end

local function update_audio_config()
    local size = ffi.cast("uint32_t*", memory + 144)[0]
    if size == 0 then return end
    local rate, bpp, channels = ffi.cast("uint32_t*", memory + 156)[0], ffi.cast("uint32_t*", memory + 160)[0], ffi.cast("uint32_t*", memory + 164)[0]
    audioSource = love.audio.newQueueableSource(rate, bpp*8, channels, 2)
    audio_rate, audio_channels, audio_bpp = rate, channels, bpp
end

function love.load(arg)
    love.graphics.setDefaultFilter("nearest", "nearest")
    love.mouse.setVisible(false)
    local f = io.open(arg[1] or "rom.wasm", "rb")
    if not f then error("ROM not found") end
    local data = f:read("*a") f:close()
    env = m3.m3_NewEnvironment()
    runtime = m3.m3_NewRuntime(env, 1024 * 1024, nil)
    local mod_ptr = ffi.new("IM3Module[1]")
    m3.m3_ParseModule(env, mod_ptr, data, #data)
    m3.m3_LoadModule(runtime, mod_ptr[0])
    local func_ptr = ffi.new("IM3Function[1]")
    m3.m3_FindFunction(func_ptr, runtime, "winit") f_winit = func_ptr[0]
    m3.m3_FindFunction(func_ptr, runtime, "wupdate") f_wupdate = func_ptr[0]
    memory = m3.m3_GetMemory(runtime, nil, 0)
    if f_winit then m3.m3_Call(f_winit, 0, nil) end
    update_dimensions()
    update_audio_config()
    rom_loaded = true
end

function love.wheelmoved(x, y) wheel_delta = y end

function love.update(dt)
    if not rom_loaded then return end
    memory = m3.m3_GetMemory(runtime, nil, 0)
    ffi.cast("uint32_t*", memory + OFF_TICK)[0] = love.timer.getTime() * 1000
    
    -- Gamepad Mask
    local mask = 0
    local gkeys = { up=0, down=1, left=2, right=3, z=4, x=5, a=6, s=7, q=8, w=9, backspace=10, ["return"]=11 }
    for k, b in pairs(gkeys) do if love.keyboard.isDown(k) then mask = bitor(mask, bitshift(1, b)) end end
    
    local joysticks = love.joystick.getJoysticks()
    if #joysticks > 0 then
        local j = joysticks[1]
        local jbtns = { dpup=0, dpdown=1, dpleft=2, dpright=3, a=4, b=5, x=6, y=7, leftshoulder=8, rightshoulder=9, start=10, back=11 }
        for jb, b in pairs(jbtns) do if j:isGamepadDown(jb) then mask = bitor(mask, bitshift(1, b)) end end
        ffi.cast("int32_t*", memory + 176)[0] = j:getGamepadAxis("leftx") * 32767
        ffi.cast("int32_t*", memory + 180)[0] = j:getGamepadAxis("lefty") * 32767
        ffi.cast("int32_t*", memory + 184)[0] = j:getGamepadAxis("rightx") * 32767
        ffi.cast("int32_t*", memory + 188)[0] = j:getGamepadAxis("righty") * 32767
    end
    ffi.cast("uint32_t*", memory + OFF_GAMEPAD)[0] = mask

    -- Full Keyboard Buffer
    local kptr = memory + OFF_KEYS
    ffi.fill(kptr, 256, 0)
    for k, id in pairs(scancode_map) do if love.keyboard.isDown(k) then kptr[id] = 1 end end

    -- Mouse
    local mx, my = love.mouse.getPosition()
    local sw, sh = love.graphics.getDimensions()
    local rw, rh = ffi.cast("uint32_t*", memory + OFF_WIDTH)[0], ffi.cast("uint32_t*", memory + OFF_HEIGHT)[0]
    local sc = math.min(sw/rw, sh/rh)
    ffi.cast("int32_t*", memory + 448)[0] = math.floor((mx - (sw - rw*sc)/2) / sc)
    ffi.cast("int32_t*", memory + 452)[0] = math.floor((my - (sh - rh*sc)/2) / sc)
    local mmask = 0
    for i=1,3 do if love.mouse.isDown(i) then mmask = bitor(mmask, bitshift(1, i-1)) end end
    ffi.cast("uint32_t*", memory + 456)[0] = mmask
    ffi.cast("int32_t*", memory + 460)[0] = wheel_delta; wheel_delta = 0

    if f_wupdate then m3.m3_Call(f_wupdate, 0, nil) end

    -- Signals
    local sigs = ffi.cast("uint8_t*", memory + OFF_SIGNALS)
    for i=0,3 do
        local s = sigs[i]
        if s == 1 then -- REDRAW
            update_dimensions()
            local w, h, bpp = ffi.cast("uint32_t*", memory + OFF_WIDTH)[0], ffi.cast("uint32_t*", memory + OFF_HEIGHT)[0], ffi.cast("uint32_t*", memory + OFF_BPP)[0]
            local dst = ffi.cast("uint32_t*", imageData:getPointer())
            if bpp == 32 then ffi.copy(dst, memory + OFF_VRAM, w * h * 4)
            elseif bpp == 16 then
                local src = ffi.cast("uint16_t*", memory + OFF_VRAM)
                for j=0, w*h-1 do
                    local p = src[j]
                    local r, g, b = bitand(rshift(p, 11), 0x1f)*8.22, bitand(rshift(p, 5), 0x3f)*4.04, bitand(p, 0x1f)*8.22
                    dst[j] = bitor(bitor(bitor(0xFF000000, bitshift(b, 16)), bitshift(g, 8)), r)
                end
            elseif bpp == 8 then
                local src = ffi.cast("uint8_t*", memory + OFF_VRAM)
                for j=0, w*h-1 do
                    local p = src[j]
                    local r, g, b = bitand(rshift(p, 5), 0x07)*36, bitand(rshift(p, 2), 0x07)*36, bitand(p, 0x03)*85
                    dst[j] = bitor(bitor(bitor(0xFF000000, bitshift(b, 16)), bitshift(g, 8)), r)
                end
            end
            image:replacePixels(imageData)
            sigs[i] = 0
        elseif s == 2 then love.event.quit()
        elseif s == 3 then love.window.setTitle(ffi.string(memory)) sigs[i] = 0
        elseif s == 4 then update_dimensions() sigs[i] = 0
        elseif s == 5 then update_audio_config() sigs[i] = 0
        elseif s >= 6 and s <= 8 then
            local prefix = {"[INFO] ", "[WARN] ", "[ERR ] "}
            print(prefix[s-5] .. ffi.string(memory))
            sigs[i] = 0
        end
    end
    
    -- Audio
    local asz = ffi.cast("uint32_t*", memory + 144)[0]
    if asz > 0 and audioSource then
        local wp, rp = ffi.cast("uint32_t*", memory + 148)[0], ffi.cast("uint32_t*", memory + 152)[0]
        local avl = (wp >= rp) and (wp - rp) or (asz - rp + wp)
        if avl > 0 then
            local smp = math.min(math.floor(avl / (audio_bpp * audio_channels)), 2048)
            local bq = smp * (audio_bpp * audio_channels)
            local sd = love.sound.newSoundData(smp, audio_rate, audio_bpp * 8, audio_channels)
            local vsz = ffi.cast("uint32_t*", memory + OFF_WIDTH)[0] * ffi.cast("uint32_t*", memory + OFF_HEIGHT)[0] * (ffi.cast("uint32_t*", memory + OFF_BPP)[0] / 8)
            local ab = memory + 512 + vsz
            if wp >= rp then ffi.copy(sd:getPointer(), ab + rp, bq)
            else local p1 = math.min(asz - rp, bq) ffi.copy(sd:getPointer(), ab + rp, p1) if p1 < bq then ffi.copy(ffi.cast("uint8_t*", sd:getPointer()) + p1, ab, bq - p1) end end
            if audioSource:queue(sd) then if not audioSource:isPlaying() then audioSource:play() end ffi.cast("uint32_t*", memory + 152)[0] = (rp + bq) % asz end
        end
    end
end

function love.draw()
    if not rom_loaded or not image then return end
    love.graphics.clear(0.05, 0.05, 0.05)
    local sw, sh = love.graphics.getDimensions()
    local iw, ih = image:getDimensions()
    local sc = math.min(sw/iw, sh/ih)
    love.graphics.draw(image, math.floor((sw - iw*sc)/2), math.floor((sh - ih*sc)/2), 0, sc, sc)
end
