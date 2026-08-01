# Wagnostic Node.js SDL2 Host (`wagnostic-node`)

Host nativo para jogos e ROMs do **Wagnostic** (`.wasm`) escrito em Node.js utilizando a biblioteca **SDL2** (`@kmamal/sdl`) e a engine de WebAssembly nativa do V8.

## Características

- ⚡ **Desempenho Nativo**: Executa a memória linear e o código WASM através do V8, com suporte a aceleração 2D da SDL2 para upload de texturas/VRAM.
- 🎮 **Suporte Completo a Input**: Mapeamento direto de scancodes de teclado (WASM `keys[256]`), coordenadas/botões/scroll de mouse e gamepad virtual (WASD + Setas + Z/X/Tab/Enter).
- 🔊 **Áudio PCM de Baixa Latência**: Reprodução de áudio sincronizado via dispositivo SDL2.
- 📦 **Zero Servidores Web**: Interface nativa direta em janela desktop OS.

## Instalação

```bash
cd emulators/node
npm install
```

## Como Executar

Para rodar qualquer ROM compilada em WASM:

```bash
node index.js ../../roms/display_test/display_test.wasm
```

Ou usando o binário:

```bash
npm start -- ../../roms/input_test/input_test.wasm
```

### Opções de Linha de Comando

- `--scale=N`: Define a escala inicial da janela (ex: `--scale=2` para 2x).
- `--fps=N`: Força o limite de quadros por segundo (ex: `--fps=60`).
- `--no-audio`: Desativa a inicialização do dispositivo de áudio.

Exemplo:

```bash
node index.js ../../roms/full_test/full_test.wasm --scale=2 --fps=60
```
