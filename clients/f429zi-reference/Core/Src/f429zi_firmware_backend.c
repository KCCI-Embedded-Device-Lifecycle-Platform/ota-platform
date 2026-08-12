#include "f429zi_firmware_backend.h"

#include "main.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define APP_START_ADDRESS     0x08020000UL
#define APP_END_ADDRESS       0x08100000UL
#define STAGING_START_ADDRESS 0x08100000UL
#define STAGING_END_ADDRESS   0x081E0000UL
#define STAGING_CAPACITY      (STAGING_END_ADDRESS - STAGING_START_ADDRESS)

#define BACKUP_SRAM_START_ADDRESS 0x40024000UL

#define UPDATE_REQUEST_MAGIC_OFFSET   0U
#define UPDATE_REQUEST_INVERSE_OFFSET 4U
#define STAGING_MAGIC_OFFSET          8U
#define STAGING_INVERSE_OFFSET        12U
#define STAGING_SIZE_OFFSET           16U
#define STAGING_CRC32_OFFSET          20U
#define UPDATE_RESULT_OFFSET          24U

#define UPDATE_REQUEST_MAGIC         0x45565345UL
#define UPDATE_REQUEST_MAGIC_INVERSE (~UPDATE_REQUEST_MAGIC)
#define STAGING_MAGIC                0x53544745UL
#define STAGING_MAGIC_INVERSE        (~STAGING_MAGIC)
#define UPDATE_RESULT_NONE           0x00000000UL
#define UPDATE_RESULT_SUCCESS        0x53554343UL
#define UPDATE_RESULT_FAILURE        0x4641494CUL

#define CRC32_INITIAL_VALUE         0xFFFFFFFFUL
#define CRC32_FINAL_XOR_VALUE       0xFFFFFFFFUL
#define CRC32_REFLECTED_POLYNOMIAL  0xEDB88320UL

#define INSTALL_RESET_DELAY_MS      1000U

typedef struct
{
    uint32_t end_address;
    uint32_t sector;
} staging_sector_t;

static const staging_sector_t staging_sectors[] = {
    {0x08104000UL, FLASH_SECTOR_12},
    {0x08108000UL, FLASH_SECTOR_13},
    {0x0810C000UL, FLASH_SECTOR_14},
    {0x08110000UL, FLASH_SECTOR_15},
    {0x08120000UL, FLASH_SECTOR_16},
    {0x08140000UL, FLASH_SECTOR_17},
    {0x08160000UL, FLASH_SECTOR_18},
    {0x08180000UL, FLASH_SECTOR_19},
    {0x081A0000UL, FLASH_SECTOR_20},
    {0x081C0000UL, FLASH_SECTOR_21},
    {0x081E0000UL, FLASH_SECTOR_22}
};

static uint32_t crc32_update(
    uint32_t crc,
    const uint8_t *data,
    size_t length)
{
    size_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
                crc = (crc >> 1U) ^ CRC32_REFLECTED_POLYNOMIAL;
            else
                crc >>= 1U;
        }
    }

    return crc;
}

static void backup_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_BKPSRAM_CLK_ENABLE();
    __DSB();
    __ISB();
}

static volatile uint32_t *backup_word(uint32_t offset)
{
    return (volatile uint32_t *)(
        BACKUP_SRAM_START_ADDRESS + offset);
}

static void backup_write(uint32_t offset, uint32_t value)
{
    *backup_word(offset) = value;
    __DSB();
}

static uint32_t backup_read(uint32_t offset)
{
    __DMB();
    return *backup_word(offset);
}

static void clear_update_request(void)
{
    backup_write(UPDATE_REQUEST_INVERSE_OFFSET, 0U);
    backup_write(UPDATE_REQUEST_MAGIC_OFFSET, 0U);
    backup_write(STAGING_INVERSE_OFFSET, 0U);
    backup_write(STAGING_MAGIC_OFFSET, 0U);
}

static bool image_vector_is_valid(
    size_t image_size)
{
    uint32_t initial_stack =
        *(volatile const uint32_t *)STAGING_START_ADDRESS;
    uint32_t reset_vector =
        *(volatile const uint32_t *)(STAGING_START_ADDRESS + 4U);
    uint32_t reset_address = reset_vector & ~1UL;
    bool main_sram =
        initial_stack > 0x20000000UL &&
        initial_stack <= 0x20030000UL;
    bool ccmram =
        initial_stack > 0x10000000UL &&
        initial_stack <= 0x10010000UL;

    if (image_size < 8U || image_size > STAGING_CAPACITY)
        return false;

    if ((initial_stack & 7U) != 0U || (!main_sram && !ccmram))
        return false;

    if ((reset_vector & 1U) == 0U)
        return false;

    return reset_address >= APP_START_ADDRESS &&
           reset_address < APP_START_ADDRESS + image_size &&
           reset_address < APP_END_ADDRESS;
}

static bool erase_staging(size_t package_size)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0xFFFFFFFFUL;
    uint32_t last_address;
    uint32_t sector_count = 0U;
    size_t index;
    HAL_StatusTypeDef status;

    if (package_size == 0U || package_size > STAGING_CAPACITY)
        return false;

    last_address =
        STAGING_START_ADDRESS + (uint32_t)package_size - 1U;

    for (index = 0U;
         index < sizeof(staging_sectors) / sizeof(staging_sectors[0]);
         index++)
    {
        if (last_address < staging_sectors[index].end_address)
        {
            sector_count = (uint32_t)index + 1U;
            break;
        }
    }

    if (sector_count == 0U)
        return false;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = FLASH_SECTOR_12;
    erase.NbSectors = sector_count;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    status = HAL_FLASHEx_Erase(&erase, &sector_error);

    if (HAL_FLASH_Lock() != HAL_OK)
        return false;

    return status == HAL_OK && sector_error == 0xFFFFFFFFUL;
}

static firmware_backend_status_t backend_prepare(
    void *raw_context,
    size_t package_size)
{
    f429zi_firmware_backend_t *context = raw_context;

    if (context == NULL || package_size == 0U)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    if (package_size > STAGING_CAPACITY || package_size > UINT32_MAX)
        return FIRMWARE_BACKEND_STATUS_NO_STORAGE;

    backup_init();
    clear_update_request();
    backup_write(UPDATE_RESULT_OFFSET, UPDATE_RESULT_NONE);

    if (!erase_staging(package_size))
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    context->expected_size = package_size;
    context->written_size = 0U;
    context->running_crc32 = CRC32_INITIAL_VALUE;
    context->image_crc32 = 0U;
    context->prepared = true;
    context->package_ready = false;
    context->install_pending = false;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_write_chunk(
    void *raw_context,
    size_t offset,
    const uint8_t *data,
    size_t length)
{
    f429zi_firmware_backend_t *context = raw_context;
    size_t source_offset = 0U;
    uint32_t address;
    bool final_chunk;

    if (context == NULL || data == NULL || length == 0U ||
        !context->prepared || context->package_ready ||
        offset != context->written_size ||
        offset > context->expected_size ||
        length > context->expected_size - offset ||
        (offset & 3U) != 0U)
    {
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    final_chunk = offset + length == context->expected_size;

    if (!final_chunk && (length & 3U) != 0U)
        return FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    address = STAGING_START_ADDRESS + (uint32_t)offset;

    while (source_offset < length)
    {
        uint32_t word = 0xFFFFFFFFUL;
        size_t word_length = length - source_offset;

        if (word_length > 4U)
            word_length = 4U;

        memcpy(&word, data + source_offset, word_length);

        if (HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_WORD,
                address,
                word) != HAL_OK ||
            memcmp(
                (const void *)(uintptr_t)address,
                data + source_offset,
                word_length) != 0)
        {
            (void)HAL_FLASH_Lock();
            context->prepared = false;
            return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
        }

        address += 4U;
        source_offset += word_length;
    }

    if (HAL_FLASH_Lock() != HAL_OK)
    {
        context->prepared = false;
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;
    }

    context->running_crc32 = crc32_update(
        context->running_crc32,
        data,
        length);
    context->written_size += length;

    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_finish_download(
    void *raw_context)
{
    f429zi_firmware_backend_t *context = raw_context;

    if (context == NULL ||
        !context->prepared ||
        context->written_size != context->expected_size ||
        !image_vector_is_valid(context->expected_size))
    {
        return FIRMWARE_BACKEND_STATUS_UNSUPPORTED_PACKAGE;
    }

    context->image_crc32 =
        context->running_crc32 ^ CRC32_FINAL_XOR_VALUE;
    context->package_ready = true;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_install(void *raw_context)
{
    f429zi_firmware_backend_t *context = raw_context;

    if (context == NULL ||
        !context->prepared ||
        !context->package_ready ||
        context->expected_size == 0U ||
        context->expected_size > UINT32_MAX)
    {
        return FIRMWARE_BACKEND_STATUS_INSTALL_FAILURE;
    }

    backup_init();
    backup_write(UPDATE_RESULT_OFFSET, UPDATE_RESULT_NONE);
    backup_write(STAGING_SIZE_OFFSET, (uint32_t)context->expected_size);
    backup_write(STAGING_CRC32_OFFSET, context->image_crc32);
    backup_write(STAGING_MAGIC_OFFSET, STAGING_MAGIC);
    backup_write(STAGING_INVERSE_OFFSET, STAGING_MAGIC_INVERSE);
    backup_write(UPDATE_REQUEST_MAGIC_OFFSET, UPDATE_REQUEST_MAGIC);
    backup_write(
        UPDATE_REQUEST_INVERSE_OFFSET,
        UPDATE_REQUEST_MAGIC_INVERSE);

    context->install_requested_at_ms = HAL_GetTick();
    context->install_pending = true;
    printf("[OTA] install scheduled\r\n");
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_cancel(void *raw_context)
{
    f429zi_firmware_backend_t *context = raw_context;

    if (context == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    backup_init();
    clear_update_request();

    context->expected_size = 0U;
    context->written_size = 0U;
    context->running_crc32 = CRC32_INITIAL_VALUE;
    context->image_crc32 = 0U;
    context->prepared = false;
    context->package_ready = false;
    context->install_pending = false;
    return FIRMWARE_BACKEND_STATUS_OK;
}

static firmware_backend_status_t backend_recover_after_boot(
    void *raw_context,
    firmware_backend_recovery_result_t *result)
{
    f429zi_firmware_backend_t *context = raw_context;
    uint32_t stored_result;

    if (context == NULL || result == NULL)
        return FIRMWARE_BACKEND_STATUS_INTERNAL_FAILURE;

    backup_init();
    stored_result = backup_read(UPDATE_RESULT_OFFSET);

    if (stored_result == UPDATE_RESULT_SUCCESS)
        *result = FIRMWARE_BACKEND_RECOVERY_SUCCESS;
    else if (stored_result == UPDATE_RESULT_FAILURE)
        *result = FIRMWARE_BACKEND_RECOVERY_ROLLED_BACK;
    else
        *result = FIRMWARE_BACKEND_RECOVERY_NONE;

    backup_write(UPDATE_RESULT_OFFSET, UPDATE_RESULT_NONE);

    memset(context, 0, sizeof(*context));
    context->running_crc32 = CRC32_INITIAL_VALUE;
    return FIRMWARE_BACKEND_STATUS_OK;
}

bool f429zi_firmware_backend_init(
    f429zi_firmware_backend_t *context,
    firmware_update_backend_t *backend)
{
    if (context == NULL || backend == NULL)
        return false;

    memset(context, 0, sizeof(*context));
    memset(backend, 0, sizeof(*backend));

    context->running_crc32 = CRC32_INITIAL_VALUE;

    backend->context = context;
    backend->prepare = backend_prepare;
    backend->write_chunk = backend_write_chunk;
    backend->finish_download = backend_finish_download;
    backend->install = backend_install;
    backend->cancel = backend_cancel;
    backend->recover_after_boot = backend_recover_after_boot;
    return true;
}

void f429zi_firmware_backend_process(
    f429zi_firmware_backend_t *context)
{
    if (context == NULL || !context->install_pending)
        return;

    if (HAL_GetTick() - context->install_requested_at_ms <
        INSTALL_RESET_DELAY_MS)
    {
        return;
    }

    context->install_pending = false;
    printf("[OTA] rebooting for install\r\n");
    HAL_Delay(20U);
    NVIC_SystemReset();
}
