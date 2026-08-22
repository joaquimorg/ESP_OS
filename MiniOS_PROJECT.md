# MiniOS para ESP32 — Plano de Projeto

## 1. Objetivo

Criar um pequeno sistema operativo/runtime modular para ESP32, inspirado em conceitos do CP/M e Unix, mas adaptado a microcontroladores modernos.

O sistema deverá:

- arrancar rapidamente;
- ocupar pouca RAM e flash;
- disponibilizar um shell interativo;
- permitir acesso ao shell por múltiplos transportes;
- abstrair o hardware através de uma API própria;
- permitir configuração persistente;
- permitir adicionar drivers/módulos;
- permitir executar aplicações;
- disponibilizar Wi-Fi e rede;
- evoluir gradualmente sem obrigar a reescrever o núcleo.

O projeto será desenvolvido em **C**, usando **ESP-IDF** e **FreeRTOS** como camada base.

O FreeRTOS e as APIs ESP-IDF devem ficar escondidos atrás das APIs do MiniOS sempre que possível.

---

# 2. Filosofia do projeto

O MiniOS **não pretende substituir o ESP-IDF nem o FreeRTOS**.

A arquitetura é:

```text
+--------------------------------------+
|              Applications            |
+--------------------------------------+
|                 Shell                |
+--------------------------------------+
| MiniOS Services                      |
| FS | NET | CONFIG | DEV | PROCESS    |
+--------------------------------------+
|             MiniOS Kernel            |
+--------------------------------------+
|              MiniOS HAL              |
+--------------------------------------+
| ESP-IDF | FreeRTOS | lwIP | Drivers  |
+--------------------------------------+
|                 ESP32                |
+--------------------------------------+
```

Regra fundamental:

> Nenhuma aplicação deve depender diretamente do ESP-IDF.

Uma aplicação deverá usar:

```c
#include "minios.h"
```

e não:

```c
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
```

---

# 3. Plataforma inicial

Começar por uma plataforma simples.

Target inicial recomendado:

```text
ESP32-C3
```

O projeto deverá, no entanto, evitar dependências específicas do C3 no kernel.

Targets futuros:

```text
ESP32
ESP32-C3
ESP32-C6
ESP32-S3
```

O ESP32-S3 será particularmente interessante para versões futuras devido a:

- USB;
- PSRAM em vários módulos;
- maior capacidade para aplicações;
- possibilidade de terminal físico com display/teclado.

---

# 4. Ambiente de desenvolvimento

## Requisitos

- Visual Studio Code
- extensão oficial Espressif IDF
- ESP-IDF
- Git
- compilador/toolchain instalado pela própria configuração ESP-IDF

No VS Code pode ser criado um projeto através de:

```text
Ctrl+Shift+P
ESP-IDF: New Project
```

Selecionar inicialmente um projeto vazio/minimal.

Também deverá ser possível trabalhar por terminal com:

```bash
idf.py set-target esp32c3
idf.py build
idf.py flash
idf.py monitor
```

---

# 5. Nome provisório

Nome interno inicial:

```text
MiniOS
```

Prompt do shell:

```text
minios:/>
```

Não tornar o nome demasiado rígido no código.

Por exemplo:

```c
#define MINIOS_NAME "MiniOS"
#define MINIOS_VERSION "0.01"
```

---

# 6. Primeira versão

A primeira milestone será:

```text
MiniOS 0.01
```

A versão 0.01 deverá conter apenas:

- boot;
- inicialização do kernel;
- abstração de console;
- shell;
- registo de comandos;
- alguns comandos básicos.

Não implementar ainda:

- Wi-Fi;
- filesystem;
- módulos dinâmicos;
- ELF;
- package manager;
- Bluetooth;
- GUI;
- Lua/Python/JavaScript.

O objetivo da v0.01 é criar uma base arquitetural limpa.

---

# 7. Estrutura inicial do projeto

Estrutura proposta:

```text
minios/
│
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
│
├── main/
│   ├── CMakeLists.txt
│   └── main.c
│
├── components/
│   │
│   ├── minios_kernel/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── minios_kernel.h
│   │   │   ├── minios_version.h
│   │   │   └── minios_types.h
│   │   ├── kernel.c
│   │   ├── process.c
│   │   ├── device.c
│   │   └── event.c
│   │
│   ├── minios_hal/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── minios_hal.h
│   │   ├── hal_uart.c
│   │   ├── hal_gpio.c
│   │   ├── hal_i2c.c
│   │   ├── hal_spi.c
│   │   └── hal_timer.c
│   │
│   ├── minios_console/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── minios_console.h
│   │   ├── console.c
│   │   └── console_uart.c
│   │
│   ├── minios_shell/
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── minios_shell.h
│   │   ├── shell.c
│   │   ├── shell_parser.c
│   │   └── commands/
│   │       ├── cmd_help.c
│   │       ├── cmd_info.c
│   │       ├── cmd_mem.c
│   │       ├── cmd_uptime.c
│   │       └── cmd_reboot.c
│   │
│   └── minios_api/
│       ├── CMakeLists.txt
│       └── include/
│           └── minios.h
│
├── apps/
│
├── modules/
│
├── docs/
│   ├── architecture.md
│   ├── api.md
│   └── roadmap.md
│
└── tests/
```

Utilizar componentes ESP-IDF independentes em vez de colocar tudo dentro de `main`.

---

# 8. main.c

O `main.c` deve ser mínimo.

Objetivo:

```c
#include "minios_kernel.h"

void app_main(void)
{
    minios_kernel_start();
}
```

Não colocar lógica da aplicação dentro de `app_main()`.

---

# 9. Sequência de boot

Sequência inicial:

```text
ESP Bootloader
      |
      v
app_main()
      |
      v
minios_kernel_start()
      |
      +--> HAL
      |
      +--> Console
      |
      +--> Shell
      |
      +--> Commands
      |
      v
Shell loop
```

Boot esperado:

```text
MiniOS 0.01
Target: ESP32-C3

[ OK ] HAL
[ OK ] Console
[ OK ] Shell

Type 'help' for available commands.

minios:/>
```

---

# 10. Kernel

API inicial:

```c
#pragma once

#include <stdint.h>

void minios_kernel_start(void);

uint32_t minios_uptime_ms(void);

void minios_reboot(void);
```

O kernel deverá coordenar os subsistemas, mas não implementar diretamente toda a funcionalidade.

Evitar um `kernel.c` gigantesco.

---

# 11. API pública

Criar:

```text
components/minios_api/include/minios.h
```

Esta será futuramente a API utilizada pelas aplicações.

Inicialmente:

```c
#pragma once

#include <stdint.h>
#include <stddef.h>

#define MINIOS_API_VERSION 1

void os_print(const char *text);

uint32_t os_uptime_ms(void);

void os_sleep(uint32_t milliseconds);

size_t os_free_memory(void);
```

Mais tarde esta API crescerá para:

```text
os_file_*
os_gpio_*
os_i2c_*
os_spi_*
os_net_*
os_process_*
os_device_*
```

---

# 12. HAL

O HAL deverá esconder chamadas ESP-IDF.

Exemplo:

```c
int minios_hal_gpio_write(int gpio, int value);
int minios_hal_gpio_read(int gpio);
```

Implementação ESP32:

```c
int minios_hal_gpio_write(int gpio, int value)
{
    return gpio_set_level(gpio, value);
}
```

As camadas superiores não devem chamar diretamente `gpio_set_level()`.

---

# 13. Console

O shell não deve depender diretamente da UART.

Criar uma abstração:

```c
typedef struct minios_console {
    int (*read)(char *buffer, size_t length);
    int (*write)(const char *buffer, size_t length);
    void (*close)(void);
    void *context;
} minios_console_t;
```

Na primeira versão:

```text
console_uart
```

Mais tarde:

```text
console_uart
console_usb
console_tcp
console_websocket
console_ble
```

O shell deverá funcionar com qualquer implementação de `minios_console_t`.

---

# 14. Shell

API proposta:

```c
typedef int (*minios_command_handler_t)(
    int argc,
    char **argv
);

typedef struct {
    const char *name;
    const char *description;
    const char *usage;
    minios_command_handler_t handler;
} minios_command_t;
```

Registo:

```c
int minios_shell_register(
    const minios_command_t *command
);
```

Exemplo:

```c
static int cmd_info(int argc, char **argv)
{
    return 0;
}

static const minios_command_t info_command = {
    .name = "info",
    .description = "Show system information",
    .usage = "info",
    .handler = cmd_info
};
```

Durante a inicialização:

```c
minios_shell_register(&info_command);
```

---

# 15. Comandos da versão 0.01

Implementar:

## help

```text
minios:/> help
```

Resultado:

```text
help       Show available commands
info       Show system information
mem        Show memory information
uptime     Show system uptime
version    Show MiniOS version
reboot     Restart system
clear      Clear terminal
```

## info

Exemplo:

```text
MiniOS       : 0.01
API          : 1
Target       : ESP32-C3
CPU cores    : 1
Free memory  : 278 KB
```

## mem

Exemplo:

```text
Heap total : ...
Heap free  : ...
Heap min   : ...
```

## uptime

```text
Uptime: 00:14:27
```

## version

```text
MiniOS 0.01
API version 1
```

## reboot

Reinicia o ESP32.

---

# 16. Parser do shell

Inicialmente suportar:

```text
command arg1 arg2 arg3
```

Exemplo:

```text
gpio write 8 1
```

Não implementar inicialmente:

```text
pipes
redirection
&&
||
wildcards
environment expansion
```

Preparar, no entanto, o parser para expansão posterior.

Definir um limite inicial:

```c
#define MINIOS_SHELL_MAX_LINE 128
#define MINIOS_SHELL_MAX_ARGS 12
```

Evitar alocações dinâmicas sempre que possível.

---

# 17. Gestão de memória

Objetivo central:

> O MiniOS deve ser previsível e económico em RAM.

Princípios:

- preferir buffers estáticos pequenos;
- evitar `malloc()` frequente;
- evitar strings gigantes;
- limitar número de comandos;
- limitar número de processos;
- limitar número de dispositivos;
- usar estruturas fixas nas primeiras versões;
- medir utilização de heap desde o início.

Criar futuramente:

```text
mem
```

para monitorizar continuamente a memória.

---

# 18. Logging

Criar uma API própria:

```c
os_log_info()
os_log_warn()
os_log_error()
os_log_debug()
```

Mesmo que inicialmente utilize:

```c
ESP_LOGI
ESP_LOGW
ESP_LOGE
```

Não espalhar chamadas `ESP_LOG*` pelas aplicações.

---

# 19. Convenções de nomes

API pública:

```text
os_*
```

Internals:

```text
minios_*
```

HAL:

```text
minios_hal_*
```

Shell:

```text
minios_shell_*
```

Device manager:

```text
minios_device_*
```

Process manager:

```text
minios_process_*
```

---

# 20. Regra de dependências

Dependências desejadas:

```text
Applications
    |
    v
MiniOS API
    |
    v
Services
    |
    v
Kernel
    |
    v
HAL
    |
    v
ESP-IDF
```

Evitar:

```text
Application ---> ESP-IDF
Shell -------> ESP-IDF driver
Application ---> FreeRTOS
Module --------> esp_wifi
```

---

# 21. Device Manager — versão futura

Objetivo:

```text
/dev
```

Dispositivos previstos:

```text
/dev/uart0
/dev/gpio
/dev/i2c0
/dev/spi0
/dev/display0
/dev/temp0
/dev/wifi0
```

API conceptual:

```c
typedef struct minios_device minios_device_t;

int os_device_register(minios_device_t *device);

minios_device_t *os_device_find(const char *name);

int os_device_open(const char *name);

int os_device_read(...);

int os_device_write(...);

int os_device_control(...);
```

Não implementar isto completamente na v0.01.

---

# 22. Process Manager — versão futura

Não implementar processos tradicionais com MMU.

Um processo MiniOS será inicialmente uma abstração sobre uma FreeRTOS task.

Interface futura:

```text
ps
run
kill
jobs
```

Estrutura conceptual:

```c
typedef struct {
    uint16_t pid;
    char name[20];
    void *task;
    uint32_t stack_size;
    uint8_t state;
} minios_process_t;
```

As aplicações não devem receber diretamente `TaskHandle_t`.

---

# 23. Configuração persistente — Fase seguinte

Usar inicialmente NVS como backend.

Interface:

```text
config get wifi.ssid
config set wifi.ssid MyNetwork
config list
config delete wifi.ssid
```

API:

```c
int os_config_get(
    const char *key,
    char *value,
    size_t length
);

int os_config_set(
    const char *key,
    const char *value
);
```

---

# 24. Filesystem — Fase seguinte

Preferência inicial:

```text
LittleFS
```

Estrutura lógica:

```text
/
├── bin/
├── boot/
├── dev/
├── etc/
├── home/
├── modules/
├── tmp/
└── var/
```

Cartão SD futuramente:

```text
/sd
```

---

# 25. Shell filesystem

Comandos futuros:

```text
ls
cd
pwd
cat
echo
mkdir
rm
cp
mv
touch
df
mount
```

---

# 26. Wi-Fi

Adicionar apenas depois do kernel, shell, config e HAL estarem estabilizados.

Comandos:

```text
wifi status
wifi scan
wifi connect <ssid>
wifi disconnect
```

Configuração:

```text
wifi.ssid
wifi.password
wifi.autoconnect
```

Depois:

```text
ifconfig
ping
dns
netstat
```

---

# 27. Shell remoto

Primeira implementação remota:

```text
TCP socket
```

Exemplo:

```text
TCP port 2323
```

O servidor deverá criar uma `minios_console_t` para cada ligação.

Não duplicar o shell.

Arquitetura:

```text
UART ----------+
               |
TCP -----------+--> Console --> Shell
               |
USB -----------+
```

---

# 28. Sessões

Mais tarde:

```c
typedef struct {
    uint16_t id;

    minios_console_t *console;

    char cwd[64];

    uint8_t authenticated;
} minios_shell_session_t;
```

Isto permitirá múltiplos shells simultâneos.

---

# 29. Módulos

Inicialmente um módulo **não será código ELF dinâmico**.

Será um driver já compilado no firmware com configuração carregável.

Exemplo:

```text
module list
module load bmp280
module unload bmp280
module info bmp280
```

Manifesto futuro:

```ini
[module]
name=bmp280
version=1.0
driver=bmp280

[hardware]
bus=i2c0
address=0x76

[device]
name=temp0
```

Resultado:

```text
/dev/temp0
```

---

# 30. Aplicações

Primeira implementação:

aplicações compiladas no firmware mas registadas no MiniOS.

API conceptual:

```c
typedef int (*minios_app_main_t)(
    int argc,
    char **argv
);

int os_app_register(
    const char *name,
    minios_app_main_t main
);
```

Exemplo:

```text
run hello
```

---

# 31. Aplicações ELF

Só implementar depois de:

- API MiniOS estabilizada;
- shell funcional;
- filesystem funcional;
- process manager funcional;
- gestão de memória minimamente controlada.

Objetivo futuro:

```text
/bin/hello.elf
/bin/ping.elf
/bin/scanner.elf
```

Execução:

```text
minios:/> hello
Hello from MiniOS application.
```

A aplicação ELF deverá aceder ao sistema através da API MiniOS, nunca diretamente através do ESP-IDF.

---

# 32. ABI

Definir desde cedo:

```c
#define MINIOS_API_VERSION 1
```

Aplicações futuras deverão declarar a versão mínima da API exigida.

Exemplo conceptual:

```text
APP ABI: 1
MINIOS ABI: 1
```

Se incompatível:

```text
ERROR: application requires MiniOS API v2
```

---

# 33. Scripts de arranque

Implementado:

```text
/boot/startup.rc
```

Exemplo:

```text
set attempts 3
repeat $attempts
    wifi connect
    if $? == 0
        exit 0
    endif
    sleep 1000
endrepeat
exit 1
```

---

# 34. Package manager

Fase avançada.

Comandos:

```text
pkg search
pkg install
pkg remove
pkg update
pkg list
```

Exemplo:

```text
pkg install netscan
```

Possível repositório HTTP:

```text
MiniOS package repository
```

Pacote:

```text
manifest
binary ELF
resources
configuration
```

Não implementar até a ABI estar suficientemente estável.

---

# 35. Segurança

Antes de disponibilizar shell por Wi-Fi:

- não expor automaticamente a shell à Internet;
- permitir desativar shell TCP;
- posteriormente implementar autenticação;
- não guardar passwords Wi-Fi em texto em ficheiros comuns;
- usar NVS e mecanismos disponíveis no ESP-IDF quando apropriado;
- validar comprimentos de argumentos;
- evitar buffer overflows;
- validar módulos e aplicações antes da execução.

---

# 36. Partition table

Na primeira versão utilizar algo simples.

Mais tarde criar:

```text
NVS
Firmware A
Firmware B
LittleFS
Application storage
```

A versão futura deverá suportar OTA.

Exemplo conceptual:

```text
+----------------------+
| Bootloader           |
+----------------------+
| Partition Table      |
+----------------------+
| NVS                  |
+----------------------+
| MiniOS OTA A         |
+----------------------+
| MiniOS OTA B         |
+----------------------+
| LittleFS             |
+----------------------+
```

---

# 37. Roadmap

## Milestone 0 — Bootstrap

Implementar:

- projeto ESP-IDF;
- componentes;
- kernel;
- console UART;
- mensagem de boot.

Resultado:

```text
MiniOS 0.01
minios:/>
```

---

## Milestone 1 — Shell

Implementar:

- parser;
- command registry;
- `help`;
- `version`;
- `info`;
- `mem`;
- `uptime`;
- `reboot`;
- `clear`.

---

## Milestone 2 — Config

Implementar:

```text
config get
config set
config list
config delete
```

Backend NVS.

---

## Milestone 3 — Filesystem

Implementar LittleFS e:

```text
ls
cd
pwd
cat
echo
mkdir
rm
```

---

## Milestone 4 — Device Manager

Criar:

```text
device list
device info
```

Primeiros dispositivos:

```text
uart0
gpio
```

---

## Milestone 5 — Hardware

Implementar:

```text
gpio
i2c
spi
```

Exemplos:

```text
gpio mode 8 out
gpio write 8 1

i2c scan
```

---

## Milestone 6 — Network

Implementar:

```text
wifi scan
wifi connect
wifi status
ping
ifconfig
```

A inclusão da rede deve ser controlada por `CONFIG_MINIOS_ENABLE_NETWORK`. Em
builds sem rede, os drivers Wi-Fi, comandos de rede e `/dev/wifi0` não devem
entrar no firmware final.

---

## Milestone 7 — Remote Console

Implementado:

```text
TCP shell na porta configurável (2323 por omissão)
```

A consola TCP utiliza a mesma command registry da UART. As duas consolas
aceitam input em paralelo e a execução de comandos é serializada para manter a
saída na sessão correta. É suportado um cliente TCP remoto de cada vez.

A inclusão é controlada por `CONFIG_MINIOS_ENABLE_REMOTE_CONSOLE`, depende de
`CONFIG_MINIOS_ENABLE_NETWORK` e inclui opções para porta e stack da task. A
primeira versão não tem autenticação nem encriptação e destina-se apenas a
redes de confiança.

---

## Milestone 8 — Shell Scripting and Init Scripts

Implementado um motor de scripts limitado, incluindo:

```text
run <file>
source <file>
/boot/startup.rc
```

São suportados comentários, variáveis (`set`, `$nome`, `${nome}` e `$?`),
`sleep`, condicionais simples (`if`/`else`/`endif`), repetição limitada
(`repeat`/`endrepeat`) e `exit`, sem alocação dinâmica. `run` cria um contexto
de variáveis novo e `source` reutiliza o contexto atual. `/boot/startup.rc` é
executado durante o boot quando existe; a sua ausência não é um erro fatal.

Limites da primeira versão: 4095 bytes e 96 linhas por ficheiro, oito variáveis,
três scripts e oito blocos aninhados, 100 repetições por bloco, 1000 instruções
por contexto e 60000 ms por `sleep`.

---

## Milestone 9 — Modules

Implementado:

```text
module list
module load
module unload
```

Os drivers são compilados no firmware e geridos por um registry estático sem
alocação dinâmica. `module load` ativa o driver e regista os dispositivos que
este fornece no Device Manager; `module unload` liberta os recursos e remove os
dispositivos. Código externo e ELF continuam fora deste milestone.

`/modules` é um namespace virtual reservado, tal como `/dev`. Apenas o Module
Manager o preenche; criação, escrita e remoção são rejeitadas, enquanto
`ls /modules` apresenta os módulos registados e o respetivo estado.

O primeiro módulo é `ssd1315`, para OLED monocromático 128×64 sobre I²C. Usa um
framebuffer estático de 1024 bytes, endereço `0x3c` por omissão e regista
`/dev/display0`. O Device Manager passou a suportar escrita, controlo e remoção
de dispositivos, e a HAL I²C passou a gerir até quatro dispositivos estáticos.

---

## Milestone 10 — Applications

Implementar:

```text
app list
run
kill
ps
```

Aplicações compiladas no firmware.

---

## Milestone 11 — ELF

Carregar aplicações ELF externas.

Objetivo:

```text
/bin/*.elf
```

---

## Milestone 12 — Packages

Implementar:

```text
pkg
```

e instalação via rede.

---

# 38. Critérios para MiniOS 0.01

A milestone é considerada concluída quando:

- compila sem warnings relevantes;
- arranca consistentemente;
- apresenta o prompt;
- o shell recebe comandos;
- comandos são registados fora do parser;
- `help` lista comandos;
- `info` mostra informações do sistema;
- `mem` mostra memória disponível;
- `uptime` funciona;
- `reboot` funciona;
- `main.c` permanece mínimo;
- nenhuma aplicação depende diretamente de ESP-IDF;
- console e shell estão desacoplados.

---

# 39. Primeira estrutura CMake

## CMakeLists.txt raiz

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(minios)
```

## main/CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        minios_kernel
)
```

## components/minios_kernel/CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "kernel.c"
        "process.c"
        "device.c"
        "event.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        minios_hal
        minios_console
        minios_shell
)
```

## components/minios_console/CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "console.c"
        "console_uart.c"
    INCLUDE_DIRS
        "include"
)
```

## components/minios_shell/CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "shell.c"
        "shell_parser.c"
        "commands/cmd_help.c"
        "commands/cmd_info.c"
        "commands/cmd_mem.c"
        "commands/cmd_uptime.c"
        "commands/cmd_reboot.c"
    INCLUDE_DIRS
        "include"
)
```

---

# 40. Primeiro teste

Depois de criada a estrutura:

```bash
idf.py set-target esp32c3
idf.py build
idf.py flash monitor
```

Resultado pretendido:

```text
MiniOS 0.01

[ OK ] Kernel
[ OK ] Console
[ OK ] Shell

Type 'help' for available commands.

minios:/> help

Available commands:

help
info
mem
uptime
version
reboot
clear

minios:/>
```

---

# 41. Princípios que não devem ser quebrados

1. `main.c` deve continuar pequeno.
2. O shell não deve conhecer UART/TCP/USB diretamente.
3. Aplicações não usam ESP-IDF diretamente.
4. Drivers são acedidos através do HAL/Device Manager.
5. Cada subsistema deve poder evoluir independentemente.
6. A utilização de RAM deve ser medida continuamente.
7. Evitar funcionalidades grandes antes da arquitetura base estar estável.
8. O sistema deverá continuar funcional mesmo sem Wi-Fi.
9. O kernel não deve transformar-se num monólito.
10. A compatibilidade futura da API deve ser considerada desde o início.

---

# 42. Primeira tarefa de implementação

Começar exclusivamente pela seguinte árvore:

```text
main/
components/
    minios_kernel/
    minios_console/
    minios_shell/
    minios_api/
```

Implementar apenas:

```text
app_main
minios_kernel_start
console UART
shell command registry
shell parser
help
version
info
mem
uptime
reboot
clear
```

Não avançar para Wi-Fi ou filesystem antes desta milestone estar limpa e funcional.

---

# 43. Prompt sugerido para trabalhar com um assistente de código no VS Code

Usar este documento como especificação do projeto.

Instrução inicial sugerida:

```text
Analisa o ficheiro MiniOS_PROJECT.md antes de alterar código.

Estamos a implementar o MiniOS 0.01 para ESP32 usando ESP-IDF.

Segue rigorosamente a arquitetura definida no documento.

Nesta primeira fase implementa apenas a Milestone 0 e Milestone 1.

Regras importantes:

- main.c deve conter apenas o arranque do MiniOS;
- separar kernel, console e shell em componentes ESP-IDF;
- o shell deve utilizar uma abstração de console;
- não implementar Wi-Fi, filesystem, módulos ou ELF nesta fase;
- evitar malloc sempre que possível;
- usar buffers com limites explícitos;
- não criar dependências diretas desnecessárias com ESP-IDF;
- cada comando deve estar isolado e registado através do command registry;
- manter o código simples e adequado a um microcontrolador.

Antes de implementar, apresenta a estrutura dos ficheiros que vais criar.
Depois implementa incrementalmente e garante que `idf.py build` continua funcional.
```

---

# 44. Evolução pretendida

A longo prazo o objetivo é conseguir uma experiência semelhante a:

```text
MiniOS 1.0
ESP32-S3

minios:/> info

System      MiniOS
Version     1.0
API         3
CPU         ESP32-S3
Flash       16 MB
PSRAM       8 MB

minios:/> devices

/dev/uart0
/dev/i2c0
/dev/spi0
/dev/display0
/dev/temp0
/dev/wifi0

minios:/> wifi scan

HOME          -42 dBm
OFFICE        -67 dBm
Guest         -81 dBm

minios:/> module load bmp280

BMP280 detected at I2C 0x76
Registered /dev/temp0

minios:/> cat /dev/temp0

22.4

minios:/> run sysmon

Process 6 started

minios:/> ps

PID  NAME       STATE
1    init       RUNNING
2    shell      RUNNING
6    sysmon     RUNNING
```

Esse é o objetivo de longo prazo.

A implementação deve, no entanto, avançar milestone a milestone.
