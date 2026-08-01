# Wagnostic Single-File Node.js SDL2 Host (`wagnostic.js`)

Host nativo em arquivo único para jogos e ROMs do **Wagnostic** (`.wasm`) escrito em Node.js utilizando a biblioteca **SDL2** (`@kmamal/sdl`) e a engine de WebAssembly nativa do V8.

## Características

- 📄 **Single-File Host**: Todo o runtime, decodificador do struct da ABI (1024 bytes), driver de áudio PCM e loop de renderização estão contidos no arquivo executável `wagnostic.js`.
- ⚡ **Desempenho Nativo**: Executa a memória linear e o código WASM através do V8, com suporte a aceleração 2D da SDL2 para upload de texturas/VRAM.
- 🎮 **Suporte Completo a Input**: Mapeamento direto de scancodes de teclado (WASM `keys[256]`), coordenadas/botões/scroll de mouse e gamepad virtual (WASD + Setas + Z/X/Tab/Enter).
- 🔊 **Áudio PCM de Baixa Latência**: Reprodução de áudio sincronizado via dispositivo SDL2.

## Instalação

```bash
cd emulators/node
npm install
```

## Como Executar

Por conter um Shebang (`#!/usr/bin/env node`), você pode executá-lo diretamente como um script binário:

```bash
./wagnostic.js ../../roms/display_test.wasm
```

Ou com `node`:

```bash
node wagnostic.js ../../roms/input_test.wasm --scale=2
```

### Opções de Linha de Comando

- `--scale=N`: Define a escala inicial da janela (ex: `--scale=2` para 2x).
- `--fps=N`: Força o limite de quadros por segundo (ex: `--fps=60`).
- `--no-audio`: Desativa a inicialização do dispositivo de áudio.
