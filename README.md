# MiniOS

> Português | [English](#english)

MiniOS é um pequeno sistema operativo/runtime modular para microcontroladores ESP32, desenvolvido em C sobre ESP-IDF e FreeRTOS. O projeto inspira-se em conceitos do CP/M e Unix, adaptados às limitações e necessidades de um microcontrolador.

A versão atual é a **MiniOS 0.01**, direcionada inicialmente ao **ESP32-C3**.

## Estado do projeto

As Milestones 0, 1 e 2 estão implementadas:

- arranque do MiniOS;
- kernel mínimo;
- abstração de console;
- console UART;
- shell interativo;
- parser sem alocação dinâmica;
- command registry estático;
- comandos básicos do sistema;
- configuração persistente com backend NVS.

Ainda não estão implementados Wi-Fi, filesystem, módulos, aplicações externas ou carregamento ELF.

## Arquitetura

```text
+----------------------------+
|        MiniOS Shell        |
+----------------------------+
|        MiniOS API          |
+----------------------------+
|       MiniOS Kernel        |
+----------------------------+
|      Console Abstraction   |
+----------------------------+
|   ESP-IDF | FreeRTOS       |
+----------------------------+
|          ESP32             |
+----------------------------+
```

Princípios atuais:

- `main.c` contém apenas o arranque do kernel;
- kernel, configuração, console, shell e API são componentes ESP-IDF separados;
- o shell utiliza `minios_console_t` e não depende diretamente da UART;
- dependências ESP-IDF e FreeRTOS ficam confinadas às camadas de implementação dos componentes;
- cada comando está isolado e é adicionado através do command registry;
- não são utilizadas alocações dinâmicas pelo código MiniOS atual;
- todos os buffers e registries têm limites explícitos.

## Estrutura do projeto

```text
ESP_OS/
├── CMakeLists.txt
├── sdkconfig.defaults
├── MiniOS_PROJECT.md
├── README.md
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    ├── minios_api/
    ├── minios_config/
    ├── minios_console/
    ├── minios_kernel/
    └── minios_shell/
        └── commands/
```

O documento [`MiniOS_PROJECT.md`](MiniOS_PROJECT.md) contém a arquitetura completa, as decisões de design e o roadmap do projeto.

## Shell

Depois do arranque, o sistema apresenta:

```text
MiniOS 0.01
Copyright 2026 joaquim.org
[ OK ] Kernel
[ OK ] Console
[ OK ] Config
[ OK ] Shell

Type 'help' for available commands.

minios:/>
```

Comandos disponíveis:

| Comando | Descrição |
| --- | --- |
| `help` | Lista os comandos registados |
| `version` | Mostra as versões do MiniOS e da API |
| `info` | Mostra informações do sistema e do target |
| `mem` | Mostra estatísticas da heap |
| `uptime` | Mostra o tempo desde o arranque |
| `reboot` | Reinicia o microcontrolador |
| `clear` | Limpa um terminal compatível com sequências ANSI |
| `config` | Gere configuração persistente em NVS |

Operações de configuração:

```text
config set wifi.ssid MyNetwork
config get wifi.ssid
config list
config delete wifi.ssid
```

As chaves aceitam letras, números, `.`, `_` e `-`, até 63 caracteres. Os valores têm no máximo 127 caracteres e, devido às limitações atuais do parser, não podem conter espaços.

Limites atuais do shell:

```c
#define MINIOS_SHELL_MAX_LINE     128
#define MINIOS_SHELL_MAX_ARGS      12
#define MINIOS_SHELL_MAX_COMMANDS  16
```

O parser suporta apenas comandos e argumentos separados por espaços. Pipes, redirecionamento, wildcards e expansão de variáveis ainda não são suportados.

## Requisitos

- ESP-IDF 5.5;
- toolchain ESP32-C3 compatível com essa versão do ESP-IDF;
- Python e dependências instaladas pelo ESP-IDF;
- CMake e Ninja instalados/configurados pelo ESP-IDF.

## Compilar

Abra uma shell ESP-IDF e execute na raiz do projeto:

```bash
idf.py set-target esp32c3
idf.py build
```

Para gravar e abrir o monitor série:

```bash
idf.py -p PORT flash monitor
```

Substitua `PORT` pela porta série correspondente à placa.

O build atual gera `build/minios.bin` e ocupa aproximadamente 195 KiB com a configuração ESP-IDF atual.

## Adicionar um comando

Cada comando deve permanecer num ficheiro próprio dentro de `components/minios_shell/commands/`.

Um comando define um handler e uma descrição estática:

```c
static int cmd_example(int argc, char **argv)
{
    return 0;
}

static const minios_command_t example_command = {
    .name = "example",
    .description = "Example command",
    .usage = "example",
    .handler = cmd_example,
};
```

O ficheiro deve disponibilizar uma função de registo que utilize `minios_shell_register()` e ser incluído no `CMakeLists.txt` do componente do shell.

## Copyright e licença

Copyright © 2026 [joaquim.org](https://joaquim.org/).

Este projeto é disponibilizado gratuitamente sob a **Apache License 2.0**. É permitida a utilização, cópia, modificação e distribuição, incluindo em projetos comerciais, desde que sejam cumpridos os termos da licença.

Ao redistribuir o MiniOS ou trabalhos derivados, é necessário:

- incluir uma cópia do ficheiro [`LICENSE`](LICENSE);
- preservar os avisos de copyright e atribuição aplicáveis;
- incluir uma cópia legível do ficheiro [`NOTICE`](NOTICE);
- identificar claramente os ficheiros que tenham sido modificados.

O aviso de origem a preservar é:

```text
MiniOS
Copyright 2026 joaquim.org
Project origin: https://joaquim.org/
```

Identificador SPDX: `Apache-2.0`.

## Roadmap resumido

| Milestone | Objetivo | Estado |
| --- | --- | --- |
| 0 | Bootstrap, kernel e console UART | Concluída |
| 1 | Shell, parser, registry e comandos básicos | Concluída |
| 2 | Configuração persistente com NVS | Concluída |
| 3 | Filesystem LittleFS | Planeada |
| 4–5 | Device Manager e hardware | Planeadas |
| 6–7 | Rede e console remoto | Planeadas |
| 8–12 | Init scripts, módulos, aplicações, ELF e packages | Futuras |

O desenvolvimento deve continuar milestone a milestone, preservando o desacoplamento entre componentes e a utilização previsível de memória.

---

<a id="english"></a>

## English

MiniOS is a small modular operating system/runtime for ESP32 microcontrollers, written in C on top of ESP-IDF and FreeRTOS. It takes inspiration from CP/M and Unix concepts while adapting them to the constraints and requirements of a microcontroller.

The current release is **MiniOS 0.01**, initially targeting the **ESP32-C3**.

### Project status

Milestones 0, 1, and 2 are implemented:

- MiniOS boot sequence;
- minimal kernel;
- console abstraction;
- UART console;
- interactive shell;
- allocation-free parser;
- static command registry;
- basic system commands;
- persistent configuration backed by NVS.

Wi-Fi, filesystems, modules, external applications, and ELF loading are intentionally not implemented yet.

### Architecture

```text
+----------------------------+
|        MiniOS Shell        |
+----------------------------+
|        MiniOS API          |
+----------------------------+
|       MiniOS Kernel        |
+----------------------------+
|      Console Abstraction   |
+----------------------------+
|   ESP-IDF | FreeRTOS       |
+----------------------------+
|          ESP32             |
+----------------------------+
```

Current design principles:

- `main.c` only starts the kernel;
- kernel, configuration, console, shell, and API are separate ESP-IDF components;
- the shell uses `minios_console_t` and has no direct UART dependency;
- ESP-IDF and FreeRTOS dependencies are confined to component implementation layers;
- every command is isolated and added through the command registry;
- the current MiniOS code does not perform dynamic allocations;
- all buffers and registries have explicit limits.

### Project structure

```text
ESP_OS/
├── CMakeLists.txt
├── sdkconfig.defaults
├── MiniOS_PROJECT.md
├── README.md
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    ├── minios_api/
    ├── minios_config/
    ├── minios_console/
    ├── minios_kernel/
    └── minios_shell/
        └── commands/
```

See [`MiniOS_PROJECT.md`](MiniOS_PROJECT.md) for the complete architecture, design decisions, and project roadmap.

### Shell

The system displays the following after boot:

```text
MiniOS 0.01
Copyright 2026 joaquim.org
[ OK ] Kernel
[ OK ] Console
[ OK ] Config
[ OK ] Shell

Type 'help' for available commands.

minios:/>
```

Available commands:

| Command | Description |
| --- | --- |
| `help` | Lists registered commands |
| `version` | Shows the MiniOS and API versions |
| `info` | Shows system and target information |
| `mem` | Shows heap statistics |
| `uptime` | Shows the time elapsed since boot |
| `reboot` | Restarts the microcontroller |
| `clear` | Clears a terminal that supports ANSI sequences |
| `config` | Manages persistent configuration in NVS |

Configuration operations:

```text
config set wifi.ssid MyNetwork
config get wifi.ssid
config list
config delete wifi.ssid
```

Keys accept letters, digits, `.`, `_`, and `-`, up to 63 characters. Values are limited to 127 characters and, because of the current parser limitations, cannot contain spaces.

Current shell limits:

```c
#define MINIOS_SHELL_MAX_LINE     128
#define MINIOS_SHELL_MAX_ARGS      12
#define MINIOS_SHELL_MAX_COMMANDS  16
```

The parser currently supports commands and space-separated arguments only. Pipes, redirection, wildcards, and variable expansion are not supported yet.

### Requirements

- ESP-IDF 5.5;
- an ESP32-C3 toolchain compatible with this ESP-IDF version;
- Python and dependencies installed by ESP-IDF;
- CMake and Ninja installed/configured by ESP-IDF.

### Building

Open an ESP-IDF shell and run from the project root:

```bash
idf.py set-target esp32c3
idf.py build
```

To flash the board and open the serial monitor:

```bash
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port assigned to the board.

The current build generates `build/minios.bin` and uses approximately 195 KiB with the current ESP-IDF configuration.

### Adding a command

Each command should remain in its own file under `components/minios_shell/commands/`.

A command defines a handler and a static descriptor:

```c
static int cmd_example(int argc, char **argv)
{
    return 0;
}

static const minios_command_t example_command = {
    .name = "example",
    .description = "Example command",
    .usage = "example",
    .handler = cmd_example,
};
```

The file must expose a registration function that calls `minios_shell_register()` and must be added to the shell component's `CMakeLists.txt`.

### Copyright and license

Copyright © 2026 [joaquim.org](https://joaquim.org/).

This project is made available free of charge under the **Apache License 2.0**. Use, copying, modification, and distribution—including commercial use—are permitted subject to the terms of the license.

When redistributing MiniOS or derivative works, you must:

- include a copy of the [`LICENSE`](LICENSE) file;
- retain all applicable copyright and attribution notices;
- include a readable copy of the [`NOTICE`](NOTICE) file;
- clearly identify files that have been modified.

The following origin notice must be preserved:

```text
MiniOS
Copyright 2026 joaquim.org
Project origin: https://joaquim.org/
```

SPDX identifier: `Apache-2.0`.

### Roadmap summary

| Milestone | Goal | Status |
| --- | --- | --- |
| 0 | Bootstrap, kernel, and UART console | Complete |
| 1 | Shell, parser, registry, and basic commands | Complete |
| 2 | Persistent configuration using NVS | Complete |
| 3 | LittleFS filesystem | Planned |
| 4–5 | Device Manager and hardware | Planned |
| 6–7 | Networking and remote console | Planned |
| 8–12 | Init scripts, modules, applications, ELF, and packages | Future |

Development should continue one milestone at a time while preserving component decoupling and predictable memory usage.
