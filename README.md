# Pact Supply Tracker

Addon de Guild Wars 2 para Nexus que mostra um botao flutuante em forma de barril. Ao clicar, ele identifica o dia logico do Pact Supply, copia os waypoints do dia para o clipboard e mostra uma notificacao curta na tela.

## Requisitos

- Windows x64
- Visual Studio 2022 com workload "Desktop development with C++"
- CMake 3.24 ou mais novo
- Nexus instalado no Guild Wars 2

O arquivo `.vsconfig` ajuda o Visual Studio a sugerir os componentes certos.

## Dependencias

As APIs externas entram como submodules Git, sem copiar os arquivos base para o codigo do addon:

```powershell
git submodule update --init --recursive
```

Submodules usados:

```text
dependencies/nexus-api -> https://github.com/RaidcoreGG/nexus-api.git
dependencies/imgui     -> https://github.com/ocornut/imgui.git
```

O repositorio principal rastreia apenas os gitlinks dos submodules e `.gitmodules`. O conteudo baixado em `dependencies/nexus-api` e `dependencies/imgui` pertence aos proprios submodules.

## Build pelo Visual Studio

1. Abra o Visual Studio 2022.
2. Use `File > Open > Folder...` e selecione esta pasta do projeto.
3. O Visual Studio deve detectar `CMakePresets.json`.
4. Escolha `msvc-debug` ou `msvc-release`.
5. Compile o alvo `gw2_nexus_addon`.

A DLL Debug sai em:

```text
build/msvc-debug/bin/Debug/pact_supply_tracker.dll
```

A DLL Release sai em:

```text
build/msvc-release/bin/Release/pact_supply_tracker.dll
```

## Build pelo PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

Para release:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release
```

## Instalar no Nexus

Copie a DLL gerada para a pasta de addons do Nexus no diretorio do GW2. Normalmente:

```text
Guild Wars 2/addons/
```

Depois abra o jogo com o Nexus instalado. O addon deve aparecer como `Pact Supply Tracker`.

As imagens do barril sao embutidas na DLL como recursos `PNG`, entao nao e necessario distribuir `barrel.png` ou `barrel_hover.png` junto do addon.

## Uso

- Clique no barril para copiar os waypoints do dia.
- Segure `Alt` e arraste com o mouse para mover o barril.
- Depois de copiar, o botao entra em delay aleatorio de 20 a 25 segundos.

## Estrutura

- `src/addon.cpp`: entrada do addon, export `GetAddonDef`, `Load`, `Unload`, render ImGui, clipboard e regra de dia.
- `include/shared.hpp`: estado compartilhado e metadados do addon.
- `include/pact_supply_data.hpp`: textos e waypoints por dia.
- `include/resource.h` e `resources.rc`: IDs e recursos PNG embutidos na DLL.
- `dependencies/nexus-api/Nexus.h`: API oficial do Nexus via submodule.
- `dependencies/imgui`: Dear ImGui via submodule, compilado como biblioteca estatica pelo CMake.
- `build.ps1`: script para configurar e compilar Debug ou Release.

## Notas de port

- `AddBezierCurve` foi atualizado para `AddBezierCubic`, que e a API atual do ImGui.
- O delay nao usa mais `std::thread(...).detach()`. Agora ele roda como timer dentro do render loop, evitando thread viva apos unload/hot reload.
- `Load` configura contexto e allocators do ImGui fornecidos pelo Nexus antes de registrar `RT_Render`.

