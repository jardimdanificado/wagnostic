// as_example — Exemplo de ROM em AssemblyScript para Wagnostic
// Compilar: asc main.ts --outFile as_example.wasm --use abort=main/abort

// @ts-ignore
@global
function abort(message: usize, fileName: usize, line: u32, column: u32): void {
  unreachable();
}

const STATE_SIZE: usize = 1024;
const VRAM_SIZE: usize = 320 * 240 * 2;
const MEM_SIZE: usize = STATE_SIZE + VRAM_SIZE;

// Allocate a static array to hold the state and VRAM
const memory_buffer = new StaticArray<u8>(MEM_SIZE);
const state_ptr: usize = changetype<usize>(memory_buffer);
const vram_ptr: usize = state_ptr + STATE_SIZE;

function init(): void {
  
  // Inicializar state
  store<u32>(state_ptr + 0, 320);   // width
  store<u32>(state_ptr + 4, 240);   // height
  store<u32>(state_ptr + 8, 16);    // bpp
  store<u32>(state_ptr + 12, 2);    // scale
  store<u32>(state_ptr + 976, STATE_SIZE); // vram_offset
  
  // Definir título
  const title = "AS ROM Example";
  for (let i = 0; i < title.length; i++) {
    store<u8>(state_ptr + 16 + i, title.charCodeAt(i));
  }
  store<u8>(state_ptr + 16 + title.length, 0);
}

function setPixel(x: i32, y: i32, r: u8, g: u8, b: u8): void {
  if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
  
  const idx = y * 320 + x;
  const pixel = <u16>((<u16>(r & 0xF8) << 8) | (<u16>(g & 0xFC) << 3) | (<u16>(b) >> 3));
  store<u16>(vram_ptr + idx * 2, pixel);
}

function fillRect(rx: i32, ry: i32, rw: i32, rh: i32, r: u8, g: u8, b: u8): void {
  for (let y = ry; y < ry + rh; y++) {
    for (let x = rx; x < rx + rw; x++) {
      setPixel(x, y, r, g, b);
    }
  }
}

function redraw(): void {
  store<u32>(state_ptr + 144, 1);  // dirty_count
  // dirty_rects[0] = {0, 0, 320, 240}
  store<i32>(state_ptr + 148, 0);  // x
  store<i32>(state_ptr + 152, 0);  // y
  store<i32>(state_ptr + 156, 320); // w
  store<i32>(state_ptr + 160, 240); // h
}

let initialized = false;

export function wupdate(): usize {
  if (!initialized) {
    init();
    initialized = true;
  }
  
  // Limpar tela (cinza escuro)
  fillRect(0, 0, 320, 240, 32, 32, 40);

  // Desenhar grade
  for (let x: i32 = 0; x < 320; x++) {
    for (let y: i32 = 0; y < 240; y++) {
      if (x % 32 == 0 || y % 32 == 0) {
        setPixel(x, y, 60, 60, 80);
      }
    }
  }

  // Desenhar retângulo na posição do mouse
  const mx = load<i32>(state_ptr + 660); // mouse_x
  const my = load<i32>(state_ptr + 664); // mouse_y
  
  // Retângulo branco com borda
  fillRect(mx - 10, my - 10, 21, 21, 255, 255, 255);
  fillRect(mx - 8, my - 8, 17, 17, 0, 120, 255);
  
  // Indicador de botão do mouse
  const mouse_buttons = load<u32>(state_ptr + 668);
  if ((mouse_buttons & 1) != 0) {
    fillRect(mx - 5, my - 5, 11, 11, 255, 50, 50);
  }
  if ((mouse_buttons & 2) != 0) {
    fillRect(mx - 3, my - 3, 7, 7, 50, 255, 50);
  }

  redraw();
  return <i32>state_ptr;
}
