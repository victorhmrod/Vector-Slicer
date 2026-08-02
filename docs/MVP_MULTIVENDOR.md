# OrcaSlicer FullSpectrum — MVP Multivendor (LAN)

**Versão:** 0.1.0-mvp · **Estado:** experimental · **Base:** Snapmaker Orca FullSpectrum 0.9.9 (AGPL-3.0)

## Objetivo

Transformar o OrcaSlicer FullSpectrum em um slicer multivendor capaz de fatiar,
conectar, enviar e iniciar impressões diretamente, via rede local e **sem
nenhuma conta cloud**, em:

- **Anycubic Kobra 3** (geração Kobra 3 / S1) com o firmware original em *LAN Mode*;
- **FlashForge Adventurer 5M e 5M Pro** com o firmware original (API LAN);
- mantendo todos os backends e perfis já existentes no projeto.

## Arquitetura

Os novos backends estendem a abstração `PrintHost` já existente
(`src/slic3r/Utils/PrintHost.hpp`) — nenhum sistema paralelo foi criado.

| Componente | Arquivo | Papel |
|---|---|---|
| `FlashforgeLan` | `src/slic3r/Utils/FlashforgeLan.{hpp,cpp}` | HTTP 8898 + descoberta de serial via TCP 8899 |
| `AnycubicLan` | `src/slic3r/Utils/AnycubicLan.{hpp,cpp}` | Handshake HTTP 18910 + MQTT TLS 9883 |
| Registro | `PrintConfig.{hpp,cpp}` (`htFlashforgeLan`, `htAnycubicLan`), `PrintHost.cpp` (factory) | Seleção por `host_type` |
| UI | `GUI/PhysicalPrinterDialog.cpp` | Campos por backend, "Testar conexão" assíncrono |
| Capacidades | `PrintHostCapabilities` em `PrintHost.hpp` | `can_upload` / `can_start_print` / `can_query_status` / `can_pause` / `can_cancel` / `can_access_camera` |

O contrato de cada backend cobre: validação de configuração
(`validate_config`), teste de conexão (`test`), upload (`upload`), início de
impressão (ação pós-upload `StartPrint`), capacidades (`get_capabilities`) e
mapeamento de erros de protocolo para mensagens compreensíveis.

O fluxo de upload usa a infraestrutura existente: `PrintHostSendDialog` →
`PrintHostJobQueue` (thread própria, com progresso e cancelamento) →
`host->upload()`.

### FlashForge LAN (Adventurer 5M / 5M Pro)

Protocolo HTTP local na porta **8898**, autenticado por `serialNumber` +
`checkCode`:

1. `POST /detail` — teste de conexão e estado (nome, status, `pid` 35=5M / 36=5M Pro).
2. `POST /uploadGcode` — multipart com headers `serialNumber`, `checkCode`,
   `fileSize`, `printNow` ("true"/"false"), `levelingBeforePrint`; campo
   `gcodeFile`. Com `printNow=true` a impressão inicia após o upload
   (não há comando separado).
3. Se o número de série não for informado, ele é descoberto automaticamente
   pelo console TCP legado na porta **8899** (`~M601 S1` → `~M115` → linha `SN:`
   → `~M602`).

### Anycubic LAN (Kobra 3 / geração S1)

Protocolo local do firmware original com *LAN Mode* ativo — implementação
própria, sem nenhuma biblioteca proprietária:

1. `GET http://IP:18910/info` → identidade (`modelId`, `cn`, `modelName`) +
   `token` + `ctrlInfoUrl` + `fileUploadurl`.
2. `POST {ctrlInfoUrl}?ts&nonce&sign&did` — requisição assinada:
   `sign = md5( md5(token[:16]) + ts + nonce )` (digests em hex minúsculo).
3. A resposta traz um blob AES-128-CBC (chave = `token[16:32]`, IV = token da
   resposta com padding para 16 bytes, PKCS7) contendo `broker`, `username`,
   `password` e `deviceId` do MQTT.
4. Upload: `POST {fileUploadurl}` multipart (campos `filename` e `gcode`,
   header `X-File-Length`).
5. Início: publicação MQTT (TLS na porta **9883**, certificado autoassinado da
   impressora, autenticação por usuário/senha derivados — sem client cert) em
   `anycubic/anycubicCloud/v1/slicer/printer/{modelId}/{deviceId}/print` com
   `{"type":"print","action":"start","data":{"taskid":"-1","filename":...,"md5":...,"filetype":1}}`,
   aguardando o ack (`.../response`) e o `print/report`.

**Nota sobre o Anycubic Slicer Next:** o código publicado do slicer oficial
não contém um backend LAN aberto (a camada de rede é um componente fechado,
como o plugin da Bambu). Por isso a rota escolhida foi a opção 2 do
enunciado: implementar o protocolo local documentado pela comunidade (fontes
abaixo), que não depende de nenhum artefato proprietário.

## Modelos suportados no MVP

| Modelo | Backend | Perfil (0.4 mm) | Processo | Filamento |
|---|---|---|---|---|
| Anycubic Kobra 3 | `anycubic_lan` | `Anycubic Kobra 3 0.4 nozzle` | `0.20mm Standard @Anycubic Kobra 3 0.4 nozzle` | `Anycubic PLA @Anycubic Kobra 3 0.4 nozzle` |
| FlashForge Adventurer 5M | `flashforge_lan` | `Flashforge Adventurer 5M 0.4 Nozzle` | `0.20mm Standard @Flashforge AD5M 0.4 Nozzle` | `Flashforge Generic PLA` |
| FlashForge Adventurer 5M Pro | `flashforge_lan` | `Flashforge Adventurer 5M Pro 0.4 Nozzle` | `0.20mm Standard @Flashforge AD5M Pro 0.4 Nozzle` | `Flashforge Generic PLA` |

Os perfis vêm do OrcaSlicer upstream (geometria, G-code inicial/final, flavor
Klipper, limites) e apenas o `host_type` padrão foi apontado para os novos
backends. **Nenhum perfil foi validado em hardware neste fork** — apenas por
geração de G-code.

## Como usar

### Ativar o LAN Mode

- **Kobra 3:** na tela da impressora, `Settings → Network → LAN Mode` (ligar).
  A impressora precisa estar acordada; em repouso ela não responde na porta
  18910.
- **Adventurer 5M/5M Pro:** na tela da impressora, ative o *LAN Mode* (menu de
  rede). A tela mostra o **CheckCode** exigido pela API.

### Cadastrar a impressora

1. Selecione o perfil da máquina (aparece no assistente de configuração).
2. Na aba da impressora, clique no ícone de conexão (impressora física).
3. Escolha o *Host Type*:
   - `Anycubic LAN (Kobra 3/S1)` — informe **apenas o IP** (as portas são
     fixas pelo protocolo; as credenciais são derivadas automaticamente).
   - `FlashForge LAN (AD5M/5M Pro)` — informe **IP** (porta opcional, padrão
     8898), o **CheckCode** no campo "API Key / Password" e, opcionalmente, o
     número de série no campo "User" (se vazio, é descoberto pela porta 8899).
4. Clique em **Test** — o teste roda em segundo plano e não trava a interface.

### Enviar uma impressão

1. Fatie o modelo normalmente.
2. Clique em enviar (ícone de upload/print host). No diálogo é possível
   escolher **somente enviar** ou **enviar e iniciar a impressão**.
3. O progresso, conclusão e erros aparecem na fila de upload
   (`PrintHostQueueDialog`) e nas notificações.

### Mensagens de erro

Os backends traduzem as falhas mais comuns: autenticação inválida (CheckCode),
timeout/host inalcançável (com dica sobre LAN Mode/repouso), impressora
ocupada, resposta inválida e recusa da impressora (código + mensagem do
firmware). Nenhuma credencial é gravada em log.

## Segurança da rede local

- Os protocolos LAN da FlashForge (HTTP/TCP) trafegam **sem criptografia**;
  o da Anycubic usa TLS com certificado autoassinado (sem validação de CA —
  não há CA pública para a impressora). Use apenas em redes confiáveis.
- Nome do arquivo remoto é sanitizado (sem path traversal, caracteres de
  controle ou nomes relativos).
- IP/porta/campos obrigatórios são validados antes de qualquer conexão.
- Timeouts finitos em todas as operações; upload cancelável; sockets fechados
  ao final; nada roda na thread da interface.
- Nenhum comando recebido da impressora é executado; nenhum shell é usado.

## Testes

`tests/slic3rutils/multivendor_lan_tests.cpp` (Catch2, target
`slic3rutils_tests`, requer `-DBUILD_TESTS=ON`):

- serialização/desserialização dos novos `host_type`;
- factory: backends antigos continuam registrados, novos respondem;
- capacidades reportadas;
- validação de IP/porta/CheckCode dos dois backends;
- sanitização de nome de arquivo (path traversal, caracteres inválidos, tamanho);
- parsing do envelope `{code,message}` da FlashForge;
- primitivas do handshake Anycubic (MD5 com vetor RFC 1321, assinatura,
  AES-CBC ida-e-volta com chave/IV derivados como no protocolo);
- servidor HTTP simulado local: teste FlashForge com sucesso/falha de
  autenticação/resposta inválida/conexão recusada e handshake Anycubic
  completo (com criptografia real) + recusa de credencial.

Os testes **nunca** contatam uma impressora real. O caminho MQTT (início de
impressão Anycubic) não tem broker simulado — ver limitações.

## Limitações conhecidas

- **Nada foi testado em hardware físico neste fork.** Toda a validação é por
  documentação comunitária + testes simulados. Itens que exigem hardware:
  - FlashForge: upload real, `printNow`, resposta do firmware a nomes de
    arquivo longos, comportamento com impressora ocupada.
  - Anycubic: handshake em firmware recente, upload real (formato exato aceito
    — G-code simples vs `.gcode.3mf` em firmwares/variantes específicos),
    `print:start` via MQTT e códigos do `print/report`.
- O início de impressão Anycubic via MQTT não é coberto por teste simulado
  (não há broker TLS mock); apenas o handshake HTTP e o upload são.
- Sem câmera, descoberta automática, pausa/cancelamento remoto, controle do
  ACE Pro/IFS ou monitoramento contínuo (fora do escopo do MVP).
- O rótulo dos campos na UI é genérico ("API Key / Password" = CheckCode;
  "User" = número de série) — melhoria de UX ficará para depois do MVP.
- Multi-nozzle: apenas os perfis 0.4 mm foram revisados para o MVP.

## Fontes técnicas

- FlashForge: [Parallel-7/flashforge-api-docs](https://github.com/Parallel-7/flashforge-api-docs)
  (`endpoints/endpoints_5m_3.2.7.yaml`), validado contra o
  [Orca-Flashforge](https://github.com/FlashForge/Orca-Flashforge) (que usa uma
  DLL de rede fechada — não redistribuída aqui; nossa implementação é
  independente).
- Anycubic: documentação do firmware pela comunidade
  [Rinkhals](https://jbatonnet.github.io/Rinkhals/firmware/mqtt/),
  [chrisfore/anycubic_ha_local](https://github.com/chrisfore/anycubic_ha_local)
  (MIT, handshake validado em hardware) e
  [ChestnutLabs/printer-protocol-orchard](https://github.com/ChestnutLabs/printer-protocol-orchard)
  (`protocols/anycubic.md`, validado em Kobra X/Kobra 3). Nenhum código foi
  copiado; apenas o protocolo documentado foi implementado.
- [AnycubicSlicerNext](https://github.com/ANYCUBIC-3D/AnycubicSlicerNext):
  referência de perfis; a camada LAN oficial é fechada (ver nota acima).

## Build (Windows x64)

Ambiente recomendado: Visual Studio 2022 (toolset v143) + CMake 3.13–3.31 +
Strawberry Perl (OpenSSL). Comando oficial: `build_release_vs2022.bat`.

Neste ambiente de desenvolvimento (apenas VS 2026 + CMake 4.2 instalados) o
baseline foi compilado com os ajustes mínimos já commitados e:

```
set CMAKE_POLICY_VERSION_MINIMUM=3.5
cd deps\build
cmake ../ -G "Visual Studio 18 2026" -T v143 -A x64 -DDESTDIR=".../OrcaSlicer_dep" -DCMAKE_BUILD_TYPE=Release -DDEP_DEBUG=OFF -DORCA_INCLUDE_DEBUG_INFO=OFF
cmake --build . --config Release --target deps -- -m
cd ..\..\build
cmake .. -G "Visual Studio 18 2026" -T v143 -A x64 -DBBL_RELEASE_TO_PUBLIC=1 -DORCA_TOOLS=ON -DCMAKE_PREFIX_PATH=".../OrcaSlicer_dep/usr/local" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target ALL_BUILD -- -m
```

Testes: configure com `-DBUILD_TESTS=ON` e rode
`ctest --test-dir build -R "slic3rutils" --output-on-failure`.
