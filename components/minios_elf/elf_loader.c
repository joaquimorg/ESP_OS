#include "minios_elf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "minios.h"
#include "minios_app.h"
#include "minios_fs.h"

#define ELF_CLASS_32 1U
#define ELF_DATA_LITTLE_ENDIAN 1U
#define ELF_VERSION_CURRENT 1U
#define ELF_TYPE_DYNAMIC 3U
#define ELF_MACHINE_RISCV 243U
#define ELF_PROGRAM_LOAD 1U
#define ELF_PROGRAM_INTERP 3U
#define ELF_PROGRAM_TLS 7U
#define ELF_SECTION_STRTAB 3U
#define ELF_SECTION_RELA 4U
#define ELF_SECTION_DYNAMIC 6U
#define ELF_SECTION_REL 9U
#define ELF_SECTION_SYMTAB 2U
#define ELF_SECTION_DYNSYM 11U
#define ELF_SECTION_UNDEFINED 0U
#define ELF_SECTION_ABSOLUTE 0xfff1U
#define ELF_SYMBOL_GLOBAL 1U
#define ELF_SYMBOL_FUNCTION 2U
#define ELF_PROGRAM_FLAG_EXECUTE 1U
#define ELF_SECTION_FLAG_EXECUTE 4U
#define ELF_DYNAMIC_NEEDED 1
#define ELF_RISCV_NONE 0U
#define ELF_RISCV_32 1U
#define ELF_RISCV_RELATIVE 3U
#define ELF_RISCV_JUMP_SLOT 5U
#define ELF_RISCV_CALL 18U
#define ELF_RISCV_CALL_PLT 19U
#define ELF_RISCV_PCREL_HI20 23U
#define ELF_RISCV_PCREL_LO12_I 24U
#define ELF_RISCV_PCREL_LO12_S 25U
#define ELF_RISCV_RELAX 51U
#define ELF_MAX_PROGRAM_HEADERS 16U
#define ELF_MAX_SECTION_HEADERS 32U

typedef struct {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t program_offset;
    uint32_t section_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t section_entry_size;
    uint16_t section_count;
    uint16_t section_names;
} elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t virtual_address;
    uint32_t physical_address;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t flags;
    uint32_t alignment;
} elf_program_header_t;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t alignment;
    uint32_t entry_size;
} elf_section_header_t;

typedef struct {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    unsigned char info;
    unsigned char other;
    uint16_t section;
} elf_symbol_t;

typedef struct {
    uint32_t offset;
    uint32_t info;
    int32_t addend;
} elf_relocation_t;

typedef struct {
    int32_t tag;
    uint32_t value;
} elf_dynamic_t;

typedef struct {
    unsigned char *memory;
    unsigned char *writable;
    size_t size;
} loaded_elf_t;

typedef struct {
    size_t size;
    unsigned char *data;
    size_t used;
    int too_large;
} file_reader_t;

typedef struct {
    const char *name;
    uintptr_t address;
} elf_import_t;

#define ELF_IMPORT(function) {#function, (uintptr_t)(function)}

static const elf_import_t imports[] = {
    ELF_IMPORT(os_print),
    ELF_IMPORT(os_uptime_ms),
    ELF_IMPORT(minios_sleep),
    ELF_IMPORT(os_free_memory),
    ELF_IMPORT(os_get_memory_info),
    ELF_IMPORT(os_get_system_info),
    ELF_IMPORT(os_reboot),
    ELF_IMPORT(os_config_get),
    ELF_IMPORT(os_config_set),
    ELF_IMPORT(os_config_delete),
    ELF_IMPORT(os_config_list),
    ELF_IMPORT(os_fs_get_space_info),
    ELF_IMPORT(os_fs_resolve_path),
    ELF_IMPORT(os_fs_getcwd),
    ELF_IMPORT(os_fs_list),
    ELF_IMPORT(os_fs_read),
    ELF_IMPORT(os_fs_write),
    ELF_IMPORT(os_fs_mkdir),
    ELF_IMPORT(os_fs_remove),
    ELF_IMPORT(os_device_count),
    ELF_IMPORT(os_device_at),
    ELF_IMPORT(os_device_find),
    ELF_IMPORT(os_device_write),
    ELF_IMPORT(os_device_control),
    ELF_IMPORT(os_app_count),
    ELF_IMPORT(os_app_at),
    ELF_IMPORT(os_app_find),
    ELF_IMPORT(os_app_run),
    ELF_IMPORT(os_app_kill),
    ELF_IMPORT(os_app_should_stop),
    ELF_IMPORT(os_process_count),
    ELF_IMPORT(os_process_at),
    ELF_IMPORT(os_net_scan),
    ELF_IMPORT(os_net_connect),
    ELF_IMPORT(os_net_connect_saved),
    ELF_IMPORT(os_net_disconnect),
    ELF_IMPORT(os_net_get_info),
    ELF_IMPORT(os_net_ping),
};

static int range_is_valid(size_t offset, size_t length, size_t total)
{
    return (offset <= total) && (length <= (total - offset));
}

static int table_is_valid(size_t offset, size_t count, size_t entry_size,
                          size_t total)
{
    return (entry_size != 0U) && (offset <= total) &&
           (count <= ((total - offset) / entry_size));
}

static int count_file_data(const char *data, size_t length, void *context)
{
    file_reader_t *reader = (file_reader_t *)context;

    (void)data;
    if (length > (MINIOS_ELF_FILE_MAX - reader->size)) {
        reader->too_large = 1;
        return -1;
    }
    reader->size += length;
    return 0;
}

static int elf_name_is_valid(const char *name)
{
    size_t index;

    for (index = 0U; name[index] != '\0'; ++index) {
        unsigned char character = (unsigned char)name[index];

        if (!(((character >= 'a') && (character <= 'z')) ||
              ((character >= 'A') && (character <= 'Z')) ||
              ((character >= '0') && (character <= '9')) ||
              (character == '_') || (character == '-'))) {
            return 0;
        }
    }
    return index != 0U;
}

static int copy_file_data(const char *data, size_t length, void *context)
{
    file_reader_t *reader = (file_reader_t *)context;

    if (!range_is_valid(reader->used, length, reader->size)) {
        return -1;
    }
    memcpy(reader->data + reader->used, data, length);
    reader->used += length;
    return 0;
}

static int resolve_path(const char *path, char resolved[OS_FS_PATH_MAX],
                        char name[OS_APP_NAME_MAX + 1U])
{
    const char *base;
    size_t length;

    if ((path == NULL) ||
        (os_fs_resolve_path(path, resolved, OS_FS_PATH_MAX) != OS_FS_OK) ||
        (strncmp(resolved, "/bin/", 5U) != 0)) {
        return MINIOS_ELF_INVALID_PATH;
    }
    base = resolved + 5U;
    if ((base[0] == '\0') || (strchr(base, '/') != NULL)) {
        return MINIOS_ELF_INVALID_PATH;
    }
    length = strlen(base);
    if ((length <= 4U) || (strcmp(base + length - 4U, ".elf") != 0) ||
        ((length - 4U) > OS_APP_NAME_MAX)) {
        return MINIOS_ELF_INVALID_PATH;
    }
    memcpy(name, base, length - 4U);
    name[length - 4U] = '\0';
    return elf_name_is_valid(name) ? MINIOS_ELF_OK : MINIOS_ELF_INVALID_PATH;
}

static int read_file(const char *path, unsigned char **data, size_t *size)
{
    file_reader_t reader = {0};
    int result = os_fs_read(path, count_file_data, &reader);

    if (result == OS_FS_NOT_FOUND) {
        return MINIOS_ELF_NOT_FOUND;
    }
    if (result != OS_FS_OK) {
        return reader.too_large ? MINIOS_ELF_TOO_LARGE : MINIOS_ELF_ERROR;
    }
    if ((reader.size < sizeof(elf_header_t)) ||
        (reader.size > MINIOS_ELF_FILE_MAX)) {
        return (reader.size > MINIOS_ELF_FILE_MAX) ? MINIOS_ELF_TOO_LARGE
                                                   : MINIOS_ELF_INVALID_FORMAT;
    }
    reader.data = heap_caps_malloc(reader.size, MALLOC_CAP_8BIT);
    if (reader.data == NULL) {
        return MINIOS_ELF_NO_MEMORY;
    }
    result = os_fs_read(path, copy_file_data, &reader);
    if ((result != OS_FS_OK) || (reader.used != reader.size)) {
        heap_caps_free(reader.data);
        return MINIOS_ELF_ERROR;
    }
    *data = reader.data;
    *size = reader.size;
    return MINIOS_ELF_OK;
}

static int import_address(const char *name, uintptr_t *address)
{
    size_t index;

    for (index = 0U; index < (sizeof(imports) / sizeof(imports[0])); ++index) {
        if (strcmp(imports[index].name, name) == 0) {
            *address = imports[index].address;
            return MINIOS_ELF_OK;
        }
    }
    return MINIOS_ELF_UNKNOWN_SYMBOL;
}

static int string_is_valid(const char *strings, size_t size, uint32_t offset)
{
    return (offset < size) &&
           (memchr(strings + offset, '\0', size - offset) != NULL);
}

static int validate_header(const unsigned char *file, size_t file_size,
                           const elf_header_t **header)
{
    const elf_header_t *candidate = (const elf_header_t *)file;

    if ((candidate->ident[0] != 0x7fU) ||
        (candidate->ident[1] != 'E') || (candidate->ident[2] != 'L') ||
        (candidate->ident[3] != 'F') ||
        (candidate->ident[4] != ELF_CLASS_32) ||
        (candidate->ident[5] != ELF_DATA_LITTLE_ENDIAN) ||
        (candidate->ident[6] != ELF_VERSION_CURRENT) ||
        (candidate->type != ELF_TYPE_DYNAMIC) ||
        (candidate->machine != ELF_MACHINE_RISCV) ||
        (candidate->version != ELF_VERSION_CURRENT) ||
        (candidate->header_size != sizeof(elf_header_t)) ||
        (candidate->program_entry_size != sizeof(elf_program_header_t)) ||
        (candidate->section_entry_size != sizeof(elf_section_header_t)) ||
        (candidate->program_count == 0U) ||
        (candidate->program_count > ELF_MAX_PROGRAM_HEADERS) ||
        (candidate->section_count == 0U) ||
        (candidate->section_count > ELF_MAX_SECTION_HEADERS) ||
        !table_is_valid(candidate->program_offset, candidate->program_count,
                        sizeof(elf_program_header_t), file_size) ||
        !table_is_valid(candidate->section_offset, candidate->section_count,
                        sizeof(elf_section_header_t), file_size)) {
        return MINIOS_ELF_INVALID_FORMAT;
    }
    *header = candidate;
    return MINIOS_ELF_OK;
}

static int image_bounds(const unsigned char *file, size_t file_size,
                        const elf_header_t *header, uint32_t *minimum,
                        uint32_t *maximum)
{
    const elf_program_header_t *programs =
        (const elf_program_header_t *)(file + header->program_offset);
    uint32_t low = UINT32_MAX;
    uint32_t high = 0U;
    size_t index;
    int executable_entry = 0;

    for (index = 0U; index < header->program_count; ++index) {
        const elf_program_header_t *program = &programs[index];
        uint32_t end;

        if ((program->type == ELF_PROGRAM_INTERP) ||
            (program->type == ELF_PROGRAM_TLS)) {
            return MINIOS_ELF_UNSUPPORTED;
        }
        if (program->type != ELF_PROGRAM_LOAD) {
            continue;
        }
        if ((program->file_size > program->memory_size) ||
            !range_is_valid(program->offset, program->file_size, file_size) ||
            (program->memory_size > (UINT32_MAX - program->virtual_address))) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        end = program->virtual_address + program->memory_size;
        if (program->virtual_address < low) {
            low = program->virtual_address;
        }
        if (end > high) {
            high = end;
        }
        if (((program->flags & ELF_PROGRAM_FLAG_EXECUTE) != 0U) &&
            (header->entry >= program->virtual_address) &&
            (header->entry < end)) {
            executable_entry = 1;
        }
    }
    if ((low == UINT32_MAX) || (high <= low) ||
        ((high - low) > MINIOS_ELF_IMAGE_MAX) || !executable_entry) {
        return ((low != UINT32_MAX) && (high > low) &&
                ((high - low) > MINIOS_ELF_IMAGE_MAX))
                   ? MINIOS_ELF_TOO_LARGE
                   : MINIOS_ELF_INVALID_FORMAT;
    }
    *minimum = low;
    *maximum = high;
    return MINIOS_ELF_OK;
}

static int validate_dynamic_sections(const unsigned char *file,
                                     size_t file_size,
                                     const elf_header_t *header)
{
    const elf_section_header_t *sections =
        (const elf_section_header_t *)(file + header->section_offset);
    size_t index;

    for (index = 0U; index < header->section_count; ++index) {
        const elf_section_header_t *section = &sections[index];
        size_t entry;

        if (section->type == ELF_SECTION_REL) {
            return MINIOS_ELF_UNSUPPORTED;
        }
        if (section->type != ELF_SECTION_DYNAMIC) {
            continue;
        }
        if ((section->entry_size != sizeof(elf_dynamic_t)) ||
            ((section->size % sizeof(elf_dynamic_t)) != 0U) ||
            !range_is_valid(section->offset, section->size, file_size)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        for (entry = 0U; entry < section->size / sizeof(elf_dynamic_t);
             ++entry) {
            const elf_dynamic_t *dynamic =
                (const elf_dynamic_t *)(file + section->offset) + entry;

            if (dynamic->tag == ELF_DYNAMIC_NEEDED) {
                return MINIOS_ELF_UNSUPPORTED;
            }
        }
    }
    return MINIOS_ELF_OK;
}

static int symbol_value(const elf_symbol_t *symbol, const char *strings,
                        size_t string_size,
                        const elf_section_header_t *sections,
                        size_t section_count, uintptr_t execute_bias,
                        uintptr_t writable_bias, uint32_t minimum,
                        uint32_t maximum,
                        uintptr_t *value, size_t *import_count)
{
    if (symbol->section == ELF_SECTION_UNDEFINED) {
        if (!string_is_valid(strings, string_size, symbol->name)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        if (import_address(strings + symbol->name, value) != MINIOS_ELF_OK) {
            return MINIOS_ELF_UNKNOWN_SYMBOL;
        }
        ++*import_count;
        return MINIOS_ELF_OK;
    }
    if (symbol->section == ELF_SECTION_ABSOLUTE) {
        *value = symbol->value;
        return MINIOS_ELF_OK;
    }
    if ((symbol->value < minimum) || (symbol->value >= maximum)) {
        return MINIOS_ELF_INVALID_FORMAT;
    }
    if (symbol->section >= section_count) {
        return MINIOS_ELF_INVALID_FORMAT;
    }
    *value = (((sections[symbol->section].flags &
                ELF_SECTION_FLAG_EXECUTE) != 0U)
                  ? execute_bias
                  : writable_bias) +
             symbol->value;
    return MINIOS_ELF_OK;
}

static int virtual_address_value(uint32_t address,
                                 const elf_section_header_t *sections,
                                 size_t section_count,
                                 uintptr_t execute_bias,
                                 uintptr_t writable_bias, uintptr_t *value)
{
    size_t index;

    for (index = 0U; index < section_count; ++index) {
        const elf_section_header_t *section = &sections[index];

        if ((section->size != 0U) && (address >= section->address) &&
            (address - section->address < section->size)) {
            *value = (((section->flags & ELF_SECTION_FLAG_EXECUTE) != 0U)
                          ? execute_bias
                          : writable_bias) +
                     address;
            return MINIOS_ELF_OK;
        }
    }
    return MINIOS_ELF_INVALID_FORMAT;
}

static void patch_upper_immediate(unsigned char *target, int32_t displacement)
{
    uint32_t instruction;
    uint32_t upper =
        (uint32_t)(((int64_t)displacement + 0x800LL) >> 12U) & 0xfffffU;

    memcpy(&instruction, target, sizeof(instruction));
    instruction = (instruction & 0x00000fffU) | (upper << 12U);
    memcpy(target, &instruction, sizeof(instruction));
}

static void patch_lower_immediate_i(unsigned char *target,
                                    int32_t displacement)
{
    uint32_t instruction;

    memcpy(&instruction, target, sizeof(instruction));
    instruction = (instruction & 0x000fffffU) |
                  (((uint32_t)displacement & 0xfffU) << 20U);
    memcpy(target, &instruction, sizeof(instruction));
}

static void patch_lower_immediate_s(unsigned char *target,
                                    int32_t displacement)
{
    uint32_t instruction;
    uint32_t immediate = (uint32_t)displacement & 0xfffU;

    memcpy(&instruction, target, sizeof(instruction));
    instruction = (instruction & 0x01fff07fU) |
                  ((immediate & 0xfe0U) << 20U) |
                  ((immediate & 0x01fU) << 7U);
    memcpy(target, &instruction, sizeof(instruction));
}

static int relocate_image(const unsigned char *file, size_t file_size,
                          const elf_header_t *header,
                          unsigned char *execute_image,
                          unsigned char *writable_image,
                          uint32_t minimum, uint32_t maximum,
                          size_t *import_count)
{
    const elf_section_header_t *sections =
        (const elf_section_header_t *)(file + header->section_offset);
    uintptr_t execute_bias = (uintptr_t)execute_image - minimum;
    uintptr_t writable_bias = (uintptr_t)writable_image - minimum;
    size_t section_index;

    *import_count = 0U;
    for (section_index = 0U; section_index < header->section_count;
         ++section_index) {
        const elf_section_header_t *relocation_section =
            &sections[section_index];
        const elf_section_header_t *symbol_section;
        const elf_section_header_t *string_section;
        const elf_symbol_t *symbols;
        const char *strings;
        size_t symbol_count;
        size_t relocation_index;

        if (relocation_section->type != ELF_SECTION_RELA) {
            continue;
        }
        if ((relocation_section->entry_size != sizeof(elf_relocation_t)) ||
            ((relocation_section->size % sizeof(elf_relocation_t)) != 0U) ||
            !range_is_valid(relocation_section->offset,
                            relocation_section->size, file_size) ||
            (relocation_section->link >= header->section_count)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        symbol_section = &sections[relocation_section->link];
        if (((symbol_section->type != ELF_SECTION_DYNSYM) &&
             (symbol_section->type != ELF_SECTION_SYMTAB)) ||
            (symbol_section->entry_size != sizeof(elf_symbol_t)) ||
            ((symbol_section->size % sizeof(elf_symbol_t)) != 0U) ||
            !range_is_valid(symbol_section->offset, symbol_section->size,
                            file_size) ||
            (symbol_section->link >= header->section_count)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        string_section = &sections[symbol_section->link];
        if ((string_section->type != ELF_SECTION_STRTAB) ||
            !range_is_valid(string_section->offset, string_section->size,
                            file_size)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        symbols = (const elf_symbol_t *)(file + symbol_section->offset);
        symbol_count = symbol_section->size / sizeof(elf_symbol_t);
        strings = (const char *)(file + string_section->offset);

        for (relocation_index = 0U;
             relocation_index <
             relocation_section->size / sizeof(elf_relocation_t);
             ++relocation_index) {
            const elf_relocation_t *relocation =
                (const elf_relocation_t *)(file + relocation_section->offset) +
                relocation_index;
            uint32_t type = relocation->info & 0xffU;
            uint32_t symbol_index = relocation->info >> 8U;
            uintptr_t value;
            uintptr_t place;
            uint32_t relocated;
            unsigned char *target;
            size_t patch_size = ((type == ELF_RISCV_CALL) ||
                                 (type == ELF_RISCV_CALL_PLT))
                                    ? 8U
                                    : sizeof(uint32_t);
            int symbol_result;

            if ((relocation->offset < minimum) ||
                !range_is_valid(relocation->offset - minimum,
                                patch_size, maximum - minimum)) {
                return MINIOS_ELF_INVALID_FORMAT;
            }
            target = writable_image + relocation->offset - minimum;
            if ((relocation_section->info >= header->section_count) ||
                (relocation->offset <
                 sections[relocation_section->info].address) ||
                !range_is_valid(
                    relocation->offset -
                        sections[relocation_section->info].address,
                    patch_size,
                    sections[relocation_section->info].size)) {
                return MINIOS_ELF_INVALID_FORMAT;
            }
            place = (((sections[relocation_section->info].flags &
                       ELF_SECTION_FLAG_EXECUTE) != 0U)
                         ? execute_bias
                         : writable_bias) +
                    relocation->offset;
            if ((type == ELF_RISCV_NONE) || (type == ELF_RISCV_RELAX)) {
                continue;
            }
            if (type == ELF_RISCV_RELATIVE) {
                if (symbol_index != 0U) {
                    return MINIOS_ELF_INVALID_FORMAT;
                }
                if (virtual_address_value((uint32_t)relocation->addend,
                                          sections, header->section_count,
                                          execute_bias, writable_bias,
                                          &value) != MINIOS_ELF_OK) {
                    return MINIOS_ELF_INVALID_FORMAT;
                }
            } else {
                if (symbol_index >= symbol_count) {
                    return MINIOS_ELF_INVALID_FORMAT;
                }
                symbol_result = symbol_value(
                    &symbols[symbol_index], strings, string_section->size,
                    sections, header->section_count, execute_bias,
                    writable_bias, minimum, maximum, &value, import_count);
                if (symbol_result != MINIOS_ELF_OK) {
                    return symbol_result;
                }
                value += (uintptr_t)relocation->addend;
            }

            if ((type == ELF_RISCV_32) ||
                (type == ELF_RISCV_JUMP_SLOT) ||
                (type == ELF_RISCV_RELATIVE)) {
                if (value > UINT32_MAX) {
                    return MINIOS_ELF_UNSUPPORTED;
                }
                relocated = (uint32_t)value;
                memcpy(target, &relocated, sizeof(relocated));
            } else if ((type == ELF_RISCV_CALL) ||
                       (type == ELF_RISCV_CALL_PLT)) {
                int64_t displacement = (int64_t)value - (int64_t)place;

                if ((displacement < INT32_MIN) ||
                    (displacement > INT32_MAX)) {
                    return MINIOS_ELF_UNSUPPORTED;
                }
                patch_upper_immediate(target, (int32_t)displacement);
                patch_lower_immediate_i(target + 4U,
                                        (int32_t)displacement);
            } else if (type == ELF_RISCV_PCREL_HI20) {
                int64_t displacement = (int64_t)value - (int64_t)place;

                if ((displacement < INT32_MIN) ||
                    (displacement > INT32_MAX)) {
                    return MINIOS_ELF_UNSUPPORTED;
                }
                patch_upper_immediate(target, (int32_t)displacement);
            } else if ((type == ELF_RISCV_PCREL_LO12_I) ||
                       (type == ELF_RISCV_PCREL_LO12_S)) {
                const elf_relocation_t *high = NULL;
                size_t high_index;
                uint32_t high_symbol_index;
                uintptr_t high_value;
                uintptr_t high_place;
                int64_t displacement;

                for (high_index = 0U;
                     high_index < relocation_section->size /
                                      sizeof(elf_relocation_t);
                     ++high_index) {
                    const elf_relocation_t *candidate =
                        (const elf_relocation_t *)(
                            file + relocation_section->offset) + high_index;

                    if ((candidate->offset == symbols[symbol_index].value) &&
                        ((candidate->info & 0xffU) ==
                         ELF_RISCV_PCREL_HI20)) {
                        high = candidate;
                        break;
                    }
                }
                if (high == NULL) {
                    return MINIOS_ELF_INVALID_FORMAT;
                }
                high_symbol_index = high->info >> 8U;
                if (high_symbol_index >= symbol_count) {
                    return MINIOS_ELF_INVALID_FORMAT;
                }
                symbol_result = symbol_value(
                    &symbols[high_symbol_index], strings,
                    string_section->size, sections, header->section_count,
                    execute_bias, writable_bias, minimum, maximum,
                    &high_value, import_count);
                if (symbol_result != MINIOS_ELF_OK) {
                    return symbol_result;
                }
                high_value += (uintptr_t)high->addend;
                high_place = (((sections[relocation_section->info].flags &
                                ELF_SECTION_FLAG_EXECUTE) != 0U)
                                  ? execute_bias
                                  : writable_bias) +
                             high->offset;
                displacement = (int64_t)high_value - (int64_t)high_place;
                if ((displacement < INT32_MIN) ||
                    (displacement > INT32_MAX)) {
                    return MINIOS_ELF_UNSUPPORTED;
                }
                if (type == ELF_RISCV_PCREL_LO12_I) {
                    patch_lower_immediate_i(target,
                                            (int32_t)displacement);
                } else {
                    patch_lower_immediate_s(target,
                                            (int32_t)displacement);
                }
            } else {
                return MINIOS_ELF_UNSUPPORTED;
            }
        }
    }
    return MINIOS_ELF_OK;
}

static int validate_entry_symbol(const unsigned char *file, size_t file_size,
                                 const elf_header_t *header)
{
    const elf_section_header_t *sections =
        (const elf_section_header_t *)(file + header->section_offset);
    size_t section_index;

    for (section_index = 0U; section_index < header->section_count;
         ++section_index) {
        const elf_section_header_t *symbol_section = &sections[section_index];
        const elf_section_header_t *string_section;
        const elf_symbol_t *symbols;
        const char *strings;
        size_t symbol_index;

        if (symbol_section->type != ELF_SECTION_DYNSYM) {
            continue;
        }
        if ((symbol_section->entry_size != sizeof(elf_symbol_t)) ||
            ((symbol_section->size % sizeof(elf_symbol_t)) != 0U) ||
            !range_is_valid(symbol_section->offset, symbol_section->size,
                            file_size) ||
            (symbol_section->link >= header->section_count)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        string_section = &sections[symbol_section->link];
        if ((string_section->type != ELF_SECTION_STRTAB) ||
            !range_is_valid(string_section->offset, string_section->size,
                            file_size)) {
            return MINIOS_ELF_INVALID_FORMAT;
        }
        symbols = (const elf_symbol_t *)(file + symbol_section->offset);
        strings = (const char *)(file + string_section->offset);
        for (symbol_index = 0U;
             symbol_index < symbol_section->size / sizeof(elf_symbol_t);
             ++symbol_index) {
            const elf_symbol_t *symbol = &symbols[symbol_index];

            if (string_is_valid(strings, string_section->size, symbol->name) &&
                (strcmp(strings + symbol->name, "minios_app_main") == 0) &&
                ((symbol->info >> 4U) == ELF_SYMBOL_GLOBAL) &&
                ((symbol->info & 0x0fU) == ELF_SYMBOL_FUNCTION) &&
                (symbol->section != ELF_SECTION_UNDEFINED) &&
                (symbol->value == header->entry)) {
                return MINIOS_ELF_OK;
            }
        }
    }
    return MINIOS_ELF_INVALID_FORMAT;
}

static void release_image(void *context)
{
    loaded_elf_t *loaded = (loaded_elf_t *)context;

    if (loaded != NULL) {
        heap_caps_free(loaded->memory);
        free(loaded);
    }
}

static int load_elf(const char *path, minios_elf_info_t *info,
                    loaded_elf_t **loaded, os_app_main_t *entry)
{
    unsigned char *file = NULL;
    size_t file_size = 0U;
    char resolved[OS_FS_PATH_MAX];
    char name[OS_APP_NAME_MAX + 1U];
    const elf_header_t *header;
    const elf_program_header_t *programs;
    loaded_elf_t *image = NULL;
    uint32_t minimum;
    uint32_t maximum;
    size_t import_count;
    size_t index;
    int result;

    result = resolve_path(path, resolved, name);
    if (result != MINIOS_ELF_OK) {
        return result;
    }
    result = read_file(resolved, &file, &file_size);
    if (result != MINIOS_ELF_OK) {
        return result;
    }
    result = validate_header(file, file_size, &header);
    if (result == MINIOS_ELF_OK) {
        result = image_bounds(file, file_size, header, &minimum, &maximum);
    }
    if (result == MINIOS_ELF_OK) {
        result = validate_dynamic_sections(file, file_size, header);
    }
    if (result == MINIOS_ELF_OK) {
        result = validate_entry_symbol(file, file_size, header);
    }
    if (result != MINIOS_ELF_OK) {
        heap_caps_free(file);
        return result;
    }

    image = calloc(1U, sizeof(*image));
    if (image == NULL) {
        heap_caps_free(file);
        return MINIOS_ELF_NO_MEMORY;
    }
    image->size = maximum - minimum;
    image->memory = heap_caps_malloc(image->size, MALLOC_CAP_EXEC);
    if (image->memory == NULL) {
        free(image);
        heap_caps_free(file);
        return MINIOS_ELF_NO_MEMORY;
    }
    image->writable = esp_ptr_diram_iram_to_dram(image->memory);
    if (image->writable == NULL) {
        release_image(image);
        heap_caps_free(file);
        return MINIOS_ELF_NO_MEMORY;
    }
    memset(image->writable, 0, image->size);
    programs =
        (const elf_program_header_t *)(file + header->program_offset);
    for (index = 0U; index < header->program_count; ++index) {
        if ((programs[index].type == ELF_PROGRAM_LOAD) &&
            (programs[index].file_size != 0U)) {
            memcpy(image->writable +
                       programs[index].virtual_address - minimum,
                   file + programs[index].offset,
                   programs[index].file_size);
        }
    }
    result = relocate_image(file, file_size, header, image->memory,
                            image->writable, minimum, maximum, &import_count);
    if (result != MINIOS_ELF_OK) {
        release_image(image);
        heap_caps_free(file);
        return result;
    }
    __asm__ volatile("fence.i" ::: "memory");

    if (info != NULL) {
        (void)snprintf(info->name, sizeof(info->name), "%s", name);
        info->file_size = file_size;
        info->image_size = image->size;
        info->imports = import_count;
    }
    *entry = (os_app_main_t)(uintptr_t)
        (image->memory + header->entry - minimum);
    *loaded = image;
    heap_caps_free(file);
    return MINIOS_ELF_OK;
}

int minios_elf_inspect(const char *path, minios_elf_info_t *info)
{
    loaded_elf_t *loaded;
    os_app_main_t entry;
    int result;

    if (info == NULL) {
        return MINIOS_ELF_INVALID_ARGUMENT;
    }
    result = load_elf(path, info, &loaded, &entry);
    if (result == MINIOS_ELF_OK) {
        release_image(loaded);
    }
    return result;
}

int minios_elf_run(const char *path, int argc, char **argv, uint16_t *pid)
{
    minios_elf_info_t info;
    loaded_elf_t *loaded;
    os_app_main_t entry;
    int result;

    if (pid == NULL) {
        return MINIOS_ELF_INVALID_ARGUMENT;
    }
    result = load_elf(path, &info, &loaded, &entry);
    if (result != MINIOS_ELF_OK) {
        return result;
    }
    result = minios_app_run_external(info.name, entry, argc, argv, pid,
                                     release_image, loaded);
    if (result != OS_APP_OK) {
        release_image(loaded);
        return (result == OS_APP_PROCESS_LIMIT) ? MINIOS_ELF_PROCESS_LIMIT
                                                : MINIOS_ELF_INVALID_ARGUMENT;
    }
    return MINIOS_ELF_OK;
}

const char *minios_elf_error_string(int result)
{
    switch (result) {
    case MINIOS_ELF_OK:
        return "OK";
    case MINIOS_ELF_INVALID_ARGUMENT:
        return "invalid argument";
    case MINIOS_ELF_NOT_FOUND:
        return "file not found";
    case MINIOS_ELF_INVALID_PATH:
        return "ELF applications must be direct children of /bin";
    case MINIOS_ELF_TOO_LARGE:
        return "file or loaded image exceeds 32 KiB";
    case MINIOS_ELF_INVALID_FORMAT:
        return "invalid ELF or missing minios_app_main";
    case MINIOS_ELF_UNSUPPORTED:
        return "unsupported ELF feature or relocation";
    case MINIOS_ELF_UNKNOWN_SYMBOL:
        return "application imports a symbol outside the MiniOS API";
    case MINIOS_ELF_NO_MEMORY:
        return "not enough executable memory";
    case MINIOS_ELF_PROCESS_LIMIT:
        return "process limit reached";
    default:
        return "loader error";
    }
}
