# VibeLoader // Advanced Manual Map Injection Core

Um loader moderno, de alta performance e com estética **dark / neon red vibe code**, baseado no injetor Manual Mapping do DLLium com **remoção total da injeção LoadLibrary**, suporte a múltiplos alvos configuráveis, auto-updater assíncrono via GitHub Releases e layout inspirado no clássico Aimware com cartões 3D, efeitos de glow e cantos arredondados suavizados.

---

## ✨ Características Principais

1. **Injeção Exclusiva Manual Map (Stealth)**:
   - Toda a injeção via `LoadLibraryA` foi **completamente removida**.
   - Mapeamento direto de seções PE (`IMAGE_SECTION_HEADER`).
   - Aplicação de relocações de base de imagem (`IMAGE_BASE_RELOCATION` para x64/x86).
   - Resolução remota da IAT (Import Address Table) com suporte a ordinais e nomes.
   - Suporte completo a callbacks TLS (`IMAGE_DIRECTORY_ENTRY_TLS`).
   - Registro de tabela de exceções estruturadas x64 SEH (`RtlAddFunctionTable`).
   - Limpeza automática de shellcode e parâmetros na memória do processo alvo após execução.
   - Habilitação de privilégios de depuração (`SeDebugPrivilege`).

2. **Interface Visual "Vibe Code" & 3D Cards**:
   - Layout inspirado no Aimware (Header com Logo/Tagline, VIP Status, lista de cheats/perfis à esquerda, informações do programa alvo e da DLL à direita, opções e botões de ação na barra inferior).
   - **Paleta Escura com Destaques Carmesim / Neon Red** (não é o vermelho sólido antigo, mas fundo carvão/obsidiana `#0d0f14` com linhas de luz, brilhos volumétricos e sombras suaves).
   - **Cards 3D**: Efeito de bisel especular no topo de cada card (1px highlight), drop shadow e bordas sutis.
   - **Efeitos de Glow**: Botão `LOAD` com pulsação suave e radiação de luz neon vermelha ao passar o mouse.
   - **Cantos Arredondados Suavizados**: Anti-aliasing completo nas bordas, inputs, botões e janela DWM arredondada nativa do Windows 11.

3. **Alvos e DLLs Configuráveis**:
   - O usuário pode definir qualquer programa alvo digitando o nome do executável (ex: `cs2.exe`, `target.exe`, `game.exe`) ou usando o **seletor de processos ativos** integrado.
   - Suporte a múltiplos perfis pré-configurados ou criação de perfis customizados.
   - Caminho da DLL selecionável via caixa de texto ou diálogo nativo do Windows Explorer (`Browse...`).
   - Persistência automática em `loader_config.json`.

4. **Auto-Updater Assíncrono com GitHub Releases**:
   - Modal de verificação de atualização inspirado na segunda imagem ("Checking for new client updates...").
   - Consulta a API de releases do repositório GitHub configurado (`api.github.com/repos/{owner}/{repo}/releases/latest`).
   - Barra de progresso animada com glow neon.
   - Baixa e atualiza a DLL de payload automaticamente sem travar a interface.

---

## 📁 Estrutura do Projeto

```
c:\loader\
├── bin\
│   ├── VibeLoader.exe          # Executável principal compilado (x64)
│   ├── test_injector.exe       # Injetor CLI para testes diretos
│   ├── loader_config.json      # Configurações do loader e perfis
│   └── payloads\               # Pasta de DLLs de payload
│       ├── cs2_module.dll
│       └── test_payload.dll
├── src\
│   ├── injection.h             # Cabeçalho da injeção manual map
│   ├── injection.cpp           # Motor manual mapping puro (LoadLibrary removido)
│   ├── updater.h               # Cabeçalho do auto-updater
│   ├── updater.cpp             # Download assíncrono e parser de releases do GitHub
│   ├── config.h                # Cabeçalho do gerenciador de configuração
│   ├── config.cpp              # Parser e serializador JSON
│   ├── ui.h                    # Motor de interface gráfica
│   ├── ui.cpp                  # Telas, cards 3D, efeitos de glow e modais
│   ├── main.cpp                # Ponto de entrada Win32 / DirectX 11
│   ├── dummy_dll.cpp           # DLL de teste com MessageBox demonstrativa
│   └── test_injector.cpp       # Testador headless de linha de comando
├── imgui\                      # Dear ImGui + DirectX11 / Win32 backends
├── build.bat                   # Script de compilação automatizada com MSVC x64
├── loader_config.json          # Configuração raiz
└── AGENTS.md                   # Diretrizes operacionais
```

---

## 🚀 Como Compilar e Executar

### 1. Compilação
Abra o PowerShell ou Prompt de Comando e execute o script `build.bat`:
```cmd
cd C:\loader
.\build.bat
```
O executável final será gerado em: `bin\VibeLoader.exe`.

### 2. Executando o Loader
```cmd
.\bin\VibeLoader.exe
```
*Nota: O loader solicitará privilégios de Administrador (`UAC: requireAdministrator`) necessários para abrir handles e alocar memória no processo alvo.*

### 3. Testando a Injeção Manual Map via CLI
Você também pode testar a injeção diretamente em qualquer processo usando o `test_injector.exe`:
```cmd
# 1. Abra um processo qualquer (exemplo: Bloco de Notas)
notepad.exe

# 2. Execute o injetor
.\bin\test_injector.exe notepad.exe payloads\test_payload.dll
```
Saída esperada:
```
Found notepad.exe with PID 33500
Injecting payloads\test_payload.dll via Manual Mapping...
[SUCCESS] Manual Map Injection SUCCESS! Base: 0x1ca56c50000
```

---

## ⚙️ Configuração (`loader_config.json`)

Você pode editar as opções tanto diretamente pela interface visual quanto pelo arquivo JSON:

```json
{
  "operatorName": "spawnyk1ng",
  "activeProfileIndex": 0,
  "autoCloseOnInject": true,
  "waitForProcess": false,
  "saveSelection": true,
  "autoCheckUpdates": true,
  "profiles": [
    {
      "name": "Counter-Strike 2",
      "exeName": "cs2.exe",
      "dllPath": "payloads/cs2_module.dll",
      "githubRepo": "Paxai/DLLium",
      "githubAsset": ".dll",
      "directUrl": "",
      "version": "Latest",
      "statusText": "Undetected"
    },
    {
      "name": "Custom Program",
      "exeName": "notepad.exe",
      "dllPath": "payloads/test_payload.dll",
      "githubRepo": "",
      "githubAsset": ".dll",
      "directUrl": "",
      "version": "v1.0.0",
      "statusText": "Testing"
    }
  ]
}
```
