#include "drv_flash.h"

#include <string.h>
#include "spi_flash.h"
#include "lsiwdg.h"

#define LOG_TAG    "drv_flash"
#include "app_log.h"

struct page_buffer {
	uint8_t buff[FLASH_PAGE_SIZE];
	size_t  page_addr;
	int     is_dirty;
};
static struct page_buffer page_buffers[FLASH_AREA_COUNTS] = {0};

static void flash_erase_sector(uint32_t sector_addr)
{
    if (sector_addr % FLASH_SECTOR_SIZE == 0) {
        LOG_D("erase sector: 0x%x\n", sector_addr);
        // HAL_IWDG_Refresh();
        spi_flash_sector_erase(sector_addr);
    }
	else {
		LOG_D("erase sector error: 0x%x\n", sector_addr);
	}
}

int drv_flash_write(enum flash_area area, size_t offset, const void *data, size_t data_size)
{
	struct page_buffer *pagebuf = &page_buffers[area];
	uint32_t addr = 0;
	uint32_t page_addr, offset_to_page;

	switch (area) {
		case FLASH_AREA_OTA:
			addr = OTA_FLASH_OFFSET;
			break;
		default:
			return -1;
	}
	ASSERT((addr & (FLASH_SECTOR_SIZE-1)) == 0);

	addr += offset;
	page_addr = addr & ~(FLASH_PAGE_SIZE-1);
	offset_to_page = addr & (FLASH_PAGE_SIZE-1);
	// LOG_D("write page_buff... page_addr=0x%x, offset_to_page=0x%x, size=%d\n", page_addr, offset_to_page, data_size);

	if (data_size > FLASH_PAGE_SIZE) {
		return -1;
	}

	// sector首部，执行擦除
	if (0 == (addr & (FLASH_SECTOR_SIZE-1))) {
		flash_erase_sector(addr);
	}

	// 切换page buffer，执行flush
	if (page_addr != pagebuf->page_addr) {
		drv_flash_flush(area);
	}

	// 写入page bufer
	pagebuf->page_addr = page_addr;
	memcpy(pagebuf->buff + offset_to_page, data, data_size);
	pagebuf->is_dirty = 1;

	// page尾部，执行写入
	if (((addr + data_size) & (FLASH_PAGE_SIZE-1)) == 0) {
		drv_flash_flush(area);
	}
	return 0;
}

int drv_flash_flush(enum flash_area area)
{
	struct page_buffer *pagebuf = &page_buffers[area];
	if (pagebuf->is_dirty) {
		LOG_D("write page: 0x%x\n", pagebuf->page_addr);
		spi_flash_quad_page_program(pagebuf->page_addr, pagebuf->buff, FLASH_PAGE_SIZE);
		memset(pagebuf->buff, 0xFF, FLASH_PAGE_SIZE);
		pagebuf->is_dirty = 0;
	}
	return 0;
}
