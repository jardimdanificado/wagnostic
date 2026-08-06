# Plano de Implementação: Padrão de Extensibilidade `wextension` no Wagnostic

Adicionar um padrão de extensibilidade genérico e opcional ao Wagnostic, através de uma única função importada pelo WASM: `void* wextension(const char* name, void* ptr)`. 

Esta função permite que hosts customizados forneçam funcionalidades específicas para ROMs sem alterar a struct base do `WagnosticState` (1024 bytes) e mantendo 100% de compatibilidade retroativa.

---

## User Review Required

> [!NOTE]
> **Convenção de Retorno**: Caso o host não possua a extensão solicitada ou não haja dado de retorno, o retorno será **`NULL` (`0`)**.
> 
> **Zero Copia/Schemas**: O Wagnostic **não especifica nenhuma struct ou formato** para os ponteiros. `ptr` e o valor retornado são ponteiros brutos (`void*`). O contrato de interpretação de dados é de livre escolha entre quem cria a ROM e o Host customizado.

---

## Proposed Changes

### Especificação da ABI

#### [MODIFY] [ABI.md](file:///home/jardel/repos/wagnostic/ABI.md)
- Adicionar a seção **Extensibilidade (Host Extensions)**.
- Definir a assinatura da função importada:
  ```c
  void* wextension(const char* name, void* ptr);
  ```
- Especificar o import em WebAssembly: `(import "env" "wextension" (func (param i32 i32) (result i32)))`.
- Documentar as regras para o Host:
  - Se a extensão `name` for reconhecida, o host executa a ação e pode retornar um ponteiro para os dados de resposta ou o próprio `ptr`.
  - Se a extensão `name` não existir ou o host for um host minimalista sem suporte a extensões, deve retornar `0` (`NULL`).

---

### Emuladores / Hosts de Referência

#### [MODIFY] [host.c](file:///home/jardel/repos/wagnostic/emulators/wasm3/host.c)
- Vincular a função `wextension` no ambiente Wasm3 via `m3_LinkRawFunction`.
- Implementar um handler padrão que retorna `0` (`NULL`) para extensões não registradas.

#### [MODIFY] [wagnostic.js](file:///home/jardel/repos/wagnostic/emulators/node/wagnostic.js)
- Adicionar `wextension` ao objeto `importObject.env` fornecido ao `WebAssembly.instantiate`.
- Retornar `0` por padrão caso nenhuma extensão customizada seja fornecida.

#### [MODIFY] [runner.js](file:///home/jardel/repos/wagnostic/emulators/web/runner.js)
- Adicionar `wextension` às opções de imports do WebAssembly no navegador.

#### [MODIFY] [gifnostic.c](file:///home/jardel/repos/wagnostic/emulators/gifnostic/gifnostic.c)
- Registrar stub para `env.wextension` no engine WASM do gifnostic.

---

### ROM de Teste e Demonstração

#### [NEW] [main.c](file:///home/jardel/repos/wagnostic/roms/wextension_test/main.c)
- Criar uma ROM de teste simples para validar:
  1. A importação e chamada de `wextension("unkown:feature", NULL)` retornando `NULL`.
  2. Chamada de uma extensão fictícia e verificação de fluxo sem quebrar o loop `wupdate()`.

---

## Verification Plan

### Manual Verification
1. **Compilação da ROM de teste**:
   ```bash
   clang --target=wasm32 -nostdlib -O3 -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined roms/wextension_test/main.c -o roms/wextension_test.wasm
   ```
2. **Execução no Emulator Wasm3**:
   Confirmar que a ROM inicia e executa sem erros de vinculação (unlinked import errors).
3. **Execução no Node.js & Web**:
   Testar a execução do `.wasm` nos ambientes Node e Browser.
