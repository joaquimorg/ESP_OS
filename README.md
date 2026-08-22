# MiniOS

> Português | [English](#english)

MiniOS é um pequeno sistema operativo/runtime modular para microcontroladores ESP32, desenvolvido em C sobre ESP-IDF e FreeRTOS. O projeto inspira-se em conceitos do CP/M e Unix, adaptados às limitações e necessidades de um microcontrolador.

A versão atual é a **MiniOS 1.00**, direcionada inicialmente ao **ESP32-C3**.

## Estado do projeto

As Milestones 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 e 10 estão implementadas:

- arranque do MiniOS;
- kernel mínimo;
- abstração de console;
- console UART;
- shell interativo;
- parser sem alocação dinâmica;
- command registry estático;
- comandos básicos do sistema;
- configuração persistente com backend NVS;
- filesystem LittleFS e comandos de ficheiros;
- Device Manager com registry estático;
- HAL e comandos para GPIO, I²C e SPI;
- Wi-Fi station, configuração IPv4, DNS e ping ICMP;
- shell remota TCP com a mesma command registry da consola UART;
- shell scripting com variáveis, controlo de fluxo e `/boot/startup.rc`;
- gestão de módulos compilados e módulo OLED SSD1315 128×64 sobre I²C;
- aplicações compiladas, processos limitados e comandos `app`, `run`, `ps` e
  `kill`.

Ainda não estão implementadas aplicações externas ou carregamento ELF.

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
- kernel, configuração, filesystem, Device Manager, console, shell e API são componentes ESP-IDF separados;
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
    ├── minios_app/
    ├── minios_config/
    ├── minios_console/
    ├── minios_device/
    ├── minios_fs/
    ├── minios_hal/
    ├── minios_kernel/
    ├── minios_module/
    ├── minios_net/
    └── minios_shell/
        └── commands/
```

O documento [`MiniOS_PROJECT.md`](MiniOS_PROJECT.md) contém a arquitetura completa, as decisões de design e o roadmap do projeto.

## Shell

Depois do arranque, o sistema apresenta:

```text
MiniOS 1.00
Copyright 2026 joaquim.org
[ OK ] Kernel
[ OK ] Console
[ OK ] HAL
[ OK ] Config
[ OK ] Network
[ OK ] Filesystem
[ OK ] Device Manager
[ OK ] Modules
[ OK ] Applications
[ OK ] Shell
[----] /boot/startup.rc not found

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
| `device` | Lista e descreve os dispositivos registados |
| `module` | Lista, carrega e descarrega módulos compilados |
| `app` | Lista e descreve aplicações compiladas |
| `ps` | Lista os processos de aplicações |
| `kill` | Solicita a paragem cooperativa de um processo |
| `gpio` | Configura, lê e escreve pinos GPIO |
| `i2c` | Configura e pesquisa o bus I²C |
| `spi` | Configura e transfere bytes por SPI |
| `wifi` | Pesquisa, liga e desliga redes Wi-Fi |
| `ifconfig` | Mostra a configuração IPv4 de `wifi0` |
| `ping` | Envia pedidos ICMP echo |
| `ls` | Lista o conteúdo de um diretório |
| `cd` | Muda o diretório atual |
| `pwd` | Mostra o diretório atual |
| `cat` | Mostra o conteúdo de um ficheiro |
| `echo` | Escreve texto num ficheiro |
| `mkdir` | Cria um diretório |
| `rm` | Remove um ficheiro ou diretório vazio |
| `run` | Executa uma aplicação compilada ou um script |
| `source` | Executa um script no contexto atual |

Operações de configuração:

```text
config set wifi.ssid MyNetwork
config get wifi.ssid
config list
config delete wifi.ssid
```

As chaves aceitam letras, números, `.`, `_` e `-`, até 63 caracteres. Os valores têm no máximo 127 caracteres e, devido às limitações atuais do parser, não podem conter espaços.

O LittleFS surge como `/` no MiniOS. No primeiro arranque são criados `/bin`,
`/boot`, `/dev`, `/etc`, `/home`, `/modules`, `/tmp` e `/var`:

```text
mkdir /home/demo
cd /home/demo
echo hello note.txt
echo world >> note.txt
cat note.txt
ls
pwd
rm note.txt
```

`echo <text> <file>` e `>` substituem o conteúdo; `>>` acrescenta. Todos os
argumentos entre `echo` e o operador/ficheiro são unidos com espaços, o que
permite criar scripts diretamente na shell:

```text
echo set attempts 3 > /boot/startup.rc
echo sleep 1000 >> /boot/startup.rc
```

Operações do Device Manager:

```text
device list
device info uart0
device info /dev/gpio
device write display0 MiniOS 1.00
device control display0 clear
ls /dev
```

O registry aceita até oito dispositivos sem alocação dinâmica e contém `uart0`,
`gpio`, `i2c0`, `spi0` e `wifi0`. O comando `ls` combina o conteúdo persistente do
LittleFS com os dispositivos virtuais quando lista `/dev`. Os namespaces `/dev`
e `/modules` são reservados: comandos de ficheiros não podem criar, alterar ou
remover entradas. `ls /modules` mostra exclusivamente os módulos registados no
Module Manager e o respetivo estado.

## Módulos e OLED SSD1315

Os módulos do Milestone 9 estão compilados no firmware. `module load` ativa um
módulo e os dispositivos que fornece; não carrega código externo do filesystem
(isso pertence ao Milestone 11). O primeiro módulo incluído controla displays
OLED monocromáticos SSD1315 128×64 através de I²C.

Um novo driver compilado fornece um `minios_module_descriptor_t`, com callbacks
de `load` e `unload`, e é adicionado ao registry através de
`minios_module_register()`. Assim pode reservar recursos na HAL durante o load,
registar um ou mais dispositivos, e desfazer essas operações no unload.

Ligação e utilização, usando GPIO 8/9 apenas como exemplo:

```text
i2c init 8 9
i2c scan
module list
ls /modules
module load ssd1315
device info display0
device write display0 Ola MiniOS
device control display0 newline
device write display0 Segunda linha
device write display0 --at 12 24 Texto em x12 y24
device control display0 contrast 160
device control display0 invert
device control display0 normal
device control display0 clear
module unload ssd1315
```

O endereço I²C por omissão é `0x3c`; pode ser substituído, por exemplo, com
`module load ssd1315 0x3d`. O módulo regista `/dev/display0`, mantém um framebuffer
estático de 1024 bytes e disponibiliza `clear`, `refresh`, `newline`, `position`,
`contrast`, `on`, `off`, `invert` e `normal`. `position <x> <y>` usa coordenadas
em píxeis; para manter um carácter 5×7 totalmente visível, aceita `x=0..122` e
`y=0..57`. Também é possível posicionar e escrever numa só operação com
`device write display0 --at <x> <y> <texto>`. A fonte compacta converte
minúsculas em maiúsculas e suporta `A-Z`, `0-9`, espaço e pontuação básica.
Enquanto o módulo estiver carregado, o bus I²C não pode ser reconfigurado.

## Aplicações e processos

O Milestone 10 adiciona aplicações compiladas no firmware e registadas através
de `os_app_register()`. Cada aplicação recebe apenas `argc`/`argv` e usa
`minios.h`; tipos do ESP-IDF e do FreeRTOS não fazem parte da API pública.

Estão incluídas três aplicações:

- `hello`: mostra uma saudação e os argumentos recebidos;
- `counter`: processo demorado para testar `ps` e `kill`;
- `welcome`: mostra uma mensagem de boas-vindas e o IP atual em `/dev/display0`.

Exemplo de gestão de processos:

```text
app list
app info counter
run hello MiniOS
run counter 30 1000
ps
kill 2
ps
```

O runtime tem quatro workers e stacks estáticos de 3072 bytes. Pode executar no
máximo quatro processos simultâneos, com oito argumentos de até 31 caracteres
por processo. `kill` é cooperativo: muda o processo para `stopping`, e a
aplicação termina quando observa `os_app_should_stop()`. Isto evita destruir uma
task enquanto mantém recursos. Processos terminados aparecem como `exited` até
o respetivo slot ser reutilizado.

Para mostrar a saudação e o IP no OLED:

```text
i2c init 8 9
module load ssd1315
wifi connect
run welcome
```

`run /caminho/script.rc` continua a executar scripts do Milestone 8; nomes que
correspondam a aplicações registadas iniciam uma aplicação em background.

## Shell scripting

`run <file>` executa um script com variáveis novas. `source <file>` partilha as
variáveis com o script que o invocou. Depois de inicializar a shell, o kernel
tenta executar `/boot/startup.rc`; a ausência do ficheiro não interrompe o boot.

```text
# /boot/startup.rc
set attempts 3
repeat $attempts
    wifi connect
    if $? == 0
        exit 0
    else
        sleep 1000
    endif
endrepeat
exit 1
```

A linguagem suporta comentários `#`, `set <nome> <valor>`, expansão
`$nome`/`${nome}`, o estado do último comando em `$?`, `sleep <ms>`,
`if`/`else`/`endif`, `repeat`/`endrepeat` e `exit [estado]`. As condições aceitam
`if <valor>`, `if <esquerda> == <direita>` e `!=`.

Não há alocação dinâmica. Os limites são 4095 bytes e 96 linhas por ficheiro,
oito variáveis, três scripts e oito blocos aninhados, 100 repetições por bloco,
1000 instruções por contexto e 60000 ms por `sleep`. Valores com espaços entre
aspas ainda não são suportados.

## Hardware

GPIO:

```text
gpio list
gpio info 8
gpio mode 8 out
gpio write 8 1
gpio mode 3 pullup
gpio read 3
gpio reset 8
```

O I²C não assume pinos por omissão, porque estes dependem do SoC, módulo e
placa. É necessário configurá-lo antes da pesquisa (8 e 9 são apenas um
exemplo de ligação):

```text
i2c status
i2c init 8 9
i2c scan
```

O SPI também exige configuração explícita. O modo é 0 e a frequência omitida
é 1 MHz; os pinos seguintes são apenas um exemplo:

```text
spi status
spi init 6 5 4 7 4000000
spi transfer 9f 00 00 00
```

`gpio list` e `gpio info` obtêm as capacidades do target selecionado no
ESP-IDF: existência do pino, entrada, saída e pull-ups/pull-downs. Um pino sem
capacidade de saída é recusado em modo `out` e em sinais de saída de I²C/SPI.
Os pinos usados pelo sistema ou por drivers são apresentados como `reserved` e
não podem ser reconfigurados. Os buses reservam os seus pinos quando são
inicializados. A pesquisa I²C é feita a 100 kHz pelo driver ESP-IDF e requer
pull-ups externos adequados para funcionamento fiável.

## Rede

O MiniOS funciona em modo Wi-Fi station. Uma ligação temporária pode ser feita
diretamente; sem argumentos, `wifi connect` usa as credenciais guardadas em
NVS:

```text
wifi scan
wifi connect MinhaRede palavra-passe
wifi status
ifconfig
ping 1.1.1.1
ping example.com 3
wifi disconnect
```

Para guardar uma rede e ativar a ligação durante o arranque:

```text
config set wifi.ssid MinhaRede
config set wifi.password palavra-passe
config set wifi.autoconnect true
wifi connect
```

`wifi.autoconnect` aceita `1`, `true` ou `yes`. Uma falha de inicialização ou
ligação Wi-Fi é não fatal e o shell continua disponível. O scan apresenta no
máximo 20 redes e `ping` aceita entre 1 e 10 pedidos. `config get` e
`config list` ocultam o valor de `wifi.password`.

### Incluir ou excluir a rede

A rede é uma opção de build e está ativa por omissão nos targets que suportam
Wi-Fi. Pode ser desativada em:

```text
idf.py menuconfig
MiniOS -> Enable Wi-Fi networking
```

Também está disponível um perfil sem rede:

```bash
idf.py -B build-no-network -D "SDKCONFIG=build-no-network/sdkconfig" -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.no-network.defaults" build
```

Com `CONFIG_MINIOS_ENABLE_NETWORK` desativada, `wifi`, `ifconfig`, `ping` e
`/dev/wifi0` não são incluídos. O arranque apresenta `Network disabled` e as
dependências Wi-Fi não entram no firmware final. No build ESP32-C3 medido, o
binário desceu de cerca de 928 KiB para 326 KiB, poupando cerca de 601 KiB.

## Consola remota TCP

Com o ESP32 ligado à rede, obtém o endereço através de `ifconfig` e abre uma
ligação para a porta TCP 2323 com `nc`, `ncat` ou um cliente Telnet:

```bash
nc 192.168.1.50 2323
```

A sessão remota usa os mesmos comandos da UART. A UART continua operacional e
um comando é executado de cada vez para evitar que a saída de duas sessões se
misture. O transporte deteta e filtra automaticamente a negociação Telnet,
mantendo compatibilidade com clientes TCP raw. O servidor aceita um cliente
remoto de cada vez e volta a aguardar uma nova ligação quando o cliente fecha a
sessão.

A funcionalidade pode ser configurada em:

```text
idf.py menuconfig
MiniOS remote console -> Enable TCP remote console
```

`CONFIG_MINIOS_REMOTE_CONSOLE_PORT` define a porta e
`CONFIG_MINIOS_REMOTE_CONSOLE_STACK_SIZE` define a stack da task. A consola
remota depende de `CONFIG_MINIOS_ENABLE_NETWORK`; não é compilada no perfil sem
rede.

Aviso: esta primeira versão usa TCP sem autenticação nem encriptação. Deve ser
ativada apenas em redes de confiança e nunca exposta diretamente à Internet.

Limites atuais do shell:

```c
#define MINIOS_SHELL_MAX_LINE     128
#define MINIOS_SHELL_MAX_ARGS      36
#define MINIOS_SHELL_MAX_COMMANDS  28
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

O build atual gera `build/minios.bin` e ocupa aproximadamente 928 KiB com a
configuração ESP-IDF/Wi-Fi atual, deixando 9% livre na partição de aplicação.

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
| 0 | Bootstrap, kernel e console UART/USB Serial-JTAG | Concluída |
| 1 | Shell, parser, registry e comandos básicos | Concluída |
| 2 | Configuração persistente com NVS | Concluída |
| 3 | LittleFS e comandos de filesystem | Concluída |
| 4 | Device Manager | Concluída |
| 5 | GPIO, I²C e SPI | Concluída |
| 6 | Wi-Fi, ping e configuração de rede | Concluída |
| 7 | Shell remota por TCP | Concluída |
| 8 | Shell scripting e `/boot/startup.rc` | Concluída |
| 9 | Gestão de módulos compilados | Concluída |
| 10 | Gestão de aplicações e processos | Concluída |
| 11 | Carregamento de aplicações ELF | Futura |
| 12 | Package manager e instalação via rede | Futura |

O desenvolvimento deve continuar milestone a milestone, preservando o desacoplamento entre componentes e a utilização previsível de memória.

---

<a id="english"></a>

## English

MiniOS is a small modular operating system/runtime for ESP32 microcontrollers, written in C on top of ESP-IDF and FreeRTOS. It takes inspiration from CP/M and Unix concepts while adapting them to the constraints and requirements of a microcontroller.

The current release is **MiniOS 1.00**, initially targeting the **ESP32-C3**.

### Project status

Milestones 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, and 10 are implemented:

- MiniOS boot sequence;
- minimal kernel;
- console abstraction;
- UART console;
- interactive shell;
- allocation-free parser;
- static command registry;
- basic system commands;
- persistent configuration backed by NVS;
- LittleFS and filesystem commands;
- a static Device Manager registry;
- HAL and commands for GPIO, I²C, and SPI;
- Wi-Fi station mode, IPv4 configuration, DNS, and ICMP ping;
- a TCP remote shell sharing the UART command registry;
- shell scripting with variables, control flow, and `/boot/startup.rc`;
- compiled module management and an I²C SSD1315 128×64 OLED module;
- compiled applications, bounded processes, and `app`, `run`, `ps`, and `kill`.

External applications and ELF loading are not implemented yet.

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
- kernel, configuration, filesystem, Device Manager, console, shell, and API are separate ESP-IDF components;
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
    ├── minios_app/
    ├── minios_config/
    ├── minios_console/
    ├── minios_device/
    ├── minios_fs/
    ├── minios_hal/
    ├── minios_kernel/
    ├── minios_module/
    ├── minios_net/
    └── minios_shell/
        └── commands/
```

See [`MiniOS_PROJECT.md`](MiniOS_PROJECT.md) for the complete architecture, design decisions, and project roadmap.

### Shell

The system displays the following after boot:

```text
MiniOS 1.00
Copyright 2026 joaquim.org
[ OK ] Kernel
[ OK ] Console
[ OK ] HAL
[ OK ] Config
[ OK ] Filesystem
[ OK ] Device Manager
[ OK ] Modules
[ OK ] Applications
[ OK ] Shell
[----] /boot/startup.rc not found

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
| `device` | Lists and describes registered devices |
| `module` | Lists, loads, and unloads compiled modules |
| `app` | Lists and describes compiled applications |
| `ps` | Lists application processes |
| `kill` | Cooperatively requests a process to stop |
| `gpio` | Configures, reads, and writes GPIO pins |
| `i2c` | Configures and scans the I²C bus |
| `spi` | Configures and transfers bytes over SPI |
| `wifi` | Scans, connects, and disconnects Wi-Fi networks |
| `ifconfig` | Shows the IPv4 configuration for `wifi0` |
| `ping` | Sends ICMP echo requests |
| `ls` | Lists directory contents |
| `cd` | Changes the working directory |
| `pwd` | Prints the working directory |
| `cat` | Displays a file |
| `echo` | Writes text to a file |
| `mkdir` | Creates a directory |
| `rm` | Removes a file or empty directory |
| `run` | Runs a compiled application or a script |
| `source` | Runs a script in the current context |

Configuration operations:

```text
config set wifi.ssid MyNetwork
config get wifi.ssid
config list
config delete wifi.ssid
```

Keys accept letters, digits, `.`, `_`, and `-`, up to 63 characters. Values are limited to 127 characters and, because of the current parser limitations, cannot contain spaces.

LittleFS appears as `/` in MiniOS. On first boot, `/bin`, `/boot`, `/dev`,
`/etc`, `/home`, `/modules`, `/tmp`, and `/var` are created:

```text
mkdir /home/demo
cd /home/demo
echo hello note.txt
echo world >> note.txt
cat note.txt
ls
pwd
rm note.txt
```

`echo <text> <file>` and `>` replace the contents; `>>` appends. All arguments
between `echo` and the operator/file are joined with spaces, allowing scripts
to be created directly from the shell:

```text
echo set attempts 3 > /boot/startup.rc
echo sleep 1000 >> /boot/startup.rc
```

Device Manager operations:

```text
device list
device info uart0
device info /dev/gpio
device write display0 MiniOS 1.00
device control display0 clear
ls /dev
```

The registry holds up to eight devices without dynamic allocation and contains
`uart0`, `gpio`, `i2c0`, `spi0`, and `wifi0`. When listing `/dev`, `ls` combines persistent
LittleFS entries with virtual devices. The `/dev` and `/modules` namespaces are
reserved, so filesystem commands cannot create, modify, or remove entries.
`ls /modules` shows only modules registered with the Module Manager and their
current state.

### Modules and SSD1315 OLED

Milestone 9 modules are compiled into the firmware. `module load` activates a
module and its devices; it does not load external code from the filesystem
(that belongs to Milestone 11). The first included module controls monochrome
SSD1315 128×64 OLED displays over I²C.

A new compiled driver provides a `minios_module_descriptor_t` with `load` and
`unload` callbacks and adds it to the registry through
`minios_module_register()`. It can therefore reserve HAL resources and register
one or more devices during load, then reverse those operations during unload.

Setup and usage, with GPIO 8/9 only as an example:

```text
i2c init 8 9
i2c scan
module list
ls /modules
module load ssd1315
device info display0
device write display0 Hello MiniOS
device control display0 newline
device write display0 Second line
device write display0 --at 12 24 Text at x12 y24
device control display0 contrast 160
device control display0 invert
device control display0 normal
device control display0 clear
module unload ssd1315
```

The default I²C address is `0x3c`; override it with, for example,
`module load ssd1315 0x3d`. The module registers `/dev/display0`, keeps a static
1024-byte framebuffer, and provides `clear`, `refresh`, `newline`, `position`,
`contrast`, `on`, `off`, `invert`, and `normal`. `position <x> <y>` uses pixel
coordinates; to keep a complete 5×7 character visible, it accepts `x=0..122`
and `y=0..57`. Positioning and writing can also be combined with
`device write display0 --at <x> <y> <text>`. Its compact font converts lowercase
to uppercase and supports `A-Z`, `0-9`, spaces, and basic punctuation. The I²C
bus cannot be reconfigured while the module is loaded.

### Applications and processes

Milestone 10 adds applications compiled into the firmware and registered through
`os_app_register()`. Each application receives only `argc`/`argv` and uses
`minios.h`; ESP-IDF and FreeRTOS types are not exposed through the public API.

Three applications are included:

- `hello`: prints a greeting and its arguments;
- `counter`: a long-running process for testing `ps` and `kill`;
- `welcome`: shows a welcome message and the current IP on `/dev/display0`.

Process management example:

```text
app list
app info counter
run hello MiniOS
run counter 30 1000
ps
kill 2
ps
```

The runtime uses four workers with static 3072-byte stacks. It supports at most
four concurrent processes, each with eight arguments of up to 31 characters.
`kill` is cooperative: it changes the process state to `stopping`, and the
application exits after observing `os_app_should_stop()`. This avoids destroying
a task while it owns resources. Completed processes remain visible as `exited`
until their slot is reused.

To show the greeting and IP address on the OLED:

```text
i2c init 8 9
module load ssd1315
wifi connect
run welcome
```

`run /path/script.rc` remains compatible with Milestone 8 scripts; a name that
matches a registered application starts that application in the background.

### Shell scripting

`run <file>` executes a script with fresh variables. `source <file>` shares
variables with its calling script. After shell initialization, the kernel tries
to execute `/boot/startup.rc`; a missing file does not stop boot.

```text
# /boot/startup.rc
set attempts 3
repeat $attempts
    wifi connect
    if $? == 0
        exit 0
    else
        sleep 1000
    endif
endrepeat
exit 1
```

The language supports `#` comments, `set <name> <value>`, `$name`/`${name}`
expansion, the last command status in `$?`, `sleep <ms>`,
`if`/`else`/`endif`, `repeat`/`endrepeat`, and `exit [status]`. Conditions accept
`if <value>`, `if <left> == <right>`, and `!=`.

There is no dynamic allocation. Limits are 4095 bytes and 96 lines per file,
eight variables, three nested scripts, eight nested blocks, 100 iterations per
repeat block, 1000 instructions per context, and 60000 ms per `sleep`. Quoted
values containing spaces are not supported yet.

### Hardware

GPIO:

```text
gpio list
gpio info 8
gpio mode 8 out
gpio write 8 1
gpio mode 3 pullup
gpio read 3
gpio reset 8
```

I²C has no default pins because they depend on the SoC, module, and board. It
must be configured before scanning (8 and 9 are only a wiring example):

```text
i2c status
i2c init 8 9
i2c scan
```

SPI also requires explicit configuration. It uses mode 0 and defaults to 1 MHz
when the frequency is omitted; these pins are only an example:

```text
spi status
spi init 6 5 4 7 4000000
spi transfer 9f 00 00 00
```

`gpio list` and `gpio info` derive capabilities from the selected ESP-IDF
target: pin existence, input, output, pull-up, and pull-down support. A pin
without output capability is rejected for `out` mode and for I²C/SPI output
signals. Pins used by the system or drivers are shown as `reserved` and cannot
be reconfigured. Buses reserve their pins when initialized. The ESP-IDF driver
scans I²C at 100 kHz and requires suitable external pull-ups for reliable
operation.

### Network

MiniOS operates in Wi-Fi station mode. A temporary connection can be made
directly; without arguments, `wifi connect` uses credentials stored in NVS:

```text
wifi scan
wifi connect MyNetwork password
wifi status
ifconfig
ping 1.1.1.1
ping example.com 3
wifi disconnect
```

To save a network and enable connection during boot:

```text
config set wifi.ssid MyNetwork
config set wifi.password password
config set wifi.autoconnect true
wifi connect
```

`wifi.autoconnect` accepts `1`, `true`, or `yes`. Wi-Fi initialization or
connection failure is non-fatal and the shell remains available. Scans show at
most 20 networks and `ping` accepts between 1 and 10 requests. `config get` and
`config list` redact the value of `wifi.password`.

#### Including or excluding networking

Networking is a build option and defaults to enabled on targets with Wi-Fi.
It can be disabled under:

```text
idf.py menuconfig
MiniOS -> Enable Wi-Fi networking
```

A network-free build profile is also provided:

```bash
idf.py -B build-no-network -D "SDKCONFIG=build-no-network/sdkconfig" -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.no-network.defaults" build
```

When `CONFIG_MINIOS_ENABLE_NETWORK` is disabled, `wifi`, `ifconfig`, `ping`,
and `/dev/wifi0` are omitted. Boot reports `Network disabled` and the Wi-Fi
dependencies are not linked into the final firmware. In the measured ESP32-C3
build, the binary dropped from about 928 KiB to 326 KiB, saving about 601 KiB.

### TCP remote console

Once the ESP32 is connected, obtain its address with `ifconfig` and connect to
TCP port 2323 with `nc`, `ncat`, or a Telnet client:

```bash
nc 192.168.1.50 2323
```

The remote session exposes the same commands as UART. UART remains available,
and commands are serialized so output from two sessions cannot be mixed. The
transport automatically detects and filters Telnet negotiation while remaining
compatible with raw TCP clients. The server accepts one remote client at a time
and listens again after the client disconnects.

Configure it under:

```text
idf.py menuconfig
MiniOS remote console -> Enable TCP remote console
```

`CONFIG_MINIOS_REMOTE_CONSOLE_PORT` selects the port and
`CONFIG_MINIOS_REMOTE_CONSOLE_STACK_SIZE` selects the task stack size. The
remote console depends on `CONFIG_MINIOS_ENABLE_NETWORK` and is omitted from
the network-free profile.

Warning: this initial version uses unencrypted, unauthenticated TCP. Enable it
only on trusted networks and never expose it directly to the Internet.

Current shell limits:

```c
#define MINIOS_SHELL_MAX_LINE     128
#define MINIOS_SHELL_MAX_ARGS      36
#define MINIOS_SHELL_MAX_COMMANDS  28
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

The current build generates `build/minios.bin` and uses approximately 928 KiB
with the current ESP-IDF/Wi-Fi configuration, leaving 9% of the application
partition free.

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
| 0 | Bootstrap, kernel, and UART/USB Serial-JTAG console | Complete |
| 1 | Shell, parser, registry, and basic commands | Complete |
| 2 | Persistent configuration using NVS | Complete |
| 3 | LittleFS and filesystem commands | Complete |
| 4 | Device Manager | Complete |
| 5 | GPIO, I²C, and SPI | Complete |
| 6 | Wi-Fi, ping, and network configuration | Complete |
| 7 | Remote TCP shell | Complete |
| 8 | Shell scripting and `/boot/startup.rc` | Complete |
| 9 | Compiled module management | Complete |
| 10 | Application and process management | Complete |
| 11 | ELF application loading | Future |
| 12 | Package manager and network installation | Future |

Development should continue one milestone at a time while preserving component decoupling and predictable memory usage.
