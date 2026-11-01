#include "archive.h"
#include "main.h"
#include "serial.h"
#include <string.h>
/* Linker excludes pages 14 and 15. No emulated flash delay or RAM fallback. */
#define ARCHIVE_BASE 0x08007000u
static bool read_media(void *context, uint32_t offset, void *out, size_t n)
{
    (void)context;
    if (offset>4096 || n>4096-offset) return false;
    memcpy(out,(const void *)(ARCHIVE_BASE+offset),n); return true;
}
static bool erase_media(void *context, unsigned page)
{
    (void)context;
    if (page>1) return false;
    uintptr_t address=ARCHIVE_BASE+page*2048;
    uint32_t before=*(volatile const uint32_t *)address;
    uint32_t cr_before=FLASH->CR;
    HAL_StatusTypeDef unlocked=HAL_FLASH_Unlock();
    if (unlocked!=HAL_OK) {
        serial_line("LB FLASH op=unlock hal=%u sr=0x%08lx cr=0x%08lx\r\n",
            unlocked,(unsigned long)FLASH->SR,(unsigned long)FLASH->CR);
        return false;
    }
    FLASH_EraseInitTypeDef request={0};
    request.TypeErase=FLASH_TYPEERASE_PAGES; request.Page=14+page; request.NbPages=1;
    uint32_t failed=0;
    HAL_StatusTypeDef result=HAL_FLASHEx_Erase(&request,&failed);
    uint32_t error=HAL_FLASH_GetError(),sr=FLASH->SR,cr=FLASH->CR;
    HAL_StatusTypeDef locked=HAL_FLASH_Lock();
    serial_line("LB FLASH op=erase page=%u hal=%u lock_hal=%u error=0x%08lx failed_page=%lu sr=0x%08lx cr_before=0x%08lx cr_after=0x%08lx before=0x%08lx after=0x%08lx\r\n",
        page,result,locked,(unsigned long)error,(unsigned long)failed,(unsigned long)sr,
        (unsigned long)cr_before,(unsigned long)cr,(unsigned long)before,
        (unsigned long)*(volatile const uint32_t *)address);
    bool ok=result==HAL_OK && locked==HAL_OK;
    if (ok) {
        for (unsigned i=0; i<2048; i+=4)
            if (*(volatile const uint32_t *)(ARCHIVE_BASE+page*2048+i)!=0xffffffffu) {
                serial_line("LB FLASH op=erase result=VERIFY_FAIL page=%u offset=%u word=0x%08lx sr=0x%08lx cr=0x%08lx\r\n",
                    page,i,(unsigned long)*(volatile const uint32_t *)(ARCHIVE_BASE+page*2048+i),
                    (unsigned long)FLASH->SR,(unsigned long)FLASH->CR);
                return false;
            }
    }
    return ok;
}
static bool program_media(void *context, uint32_t offset, const uint8_t data[8])
{
    (void)context;
    if ((offset&7) || offset>4088) return false;
    HAL_StatusTypeDef unlocked=HAL_FLASH_Unlock();
    if (unlocked!=HAL_OK) {
        serial_line("LB FLASH op=program_unlock hal=%u sr=0x%08lx cr=0x%08lx\r\n",
            unlocked,(unsigned long)FLASH->SR,(unsigned long)FLASH->CR);
        return false;
    }
    uint64_t value; memcpy(&value,data,8);
    HAL_StatusTypeDef result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,ARCHIVE_BASE+offset,value);
    uint32_t error=HAL_FLASH_GetError(),sr=FLASH->SR,cr=FLASH->CR;
    HAL_StatusTypeDef locked=HAL_FLASH_Lock();
    bool matches=memcmp((const void *)(ARCHIVE_BASE+offset),data,8)==0;
    if (result!=HAL_OK || locked!=HAL_OK || !matches)
        serial_line("LB FLASH op=program offset=%lu hal=%u lock_hal=%u match=%u error=0x%08lx sr=0x%08lx cr=0x%08lx want=%08lx,%08lx got=%08lx,%08lx\r\n",
            (unsigned long)offset,result,locked,matches,(unsigned long)error,
            (unsigned long)sr,(unsigned long)cr,(unsigned long)(uint32_t)value,
            (unsigned long)(uint32_t)(value>>32),
            (unsigned long)*(volatile const uint32_t *)(ARCHIVE_BASE+offset),
            (unsigned long)*(volatile const uint32_t *)(ARCHIVE_BASE+offset+4));
    return result==HAL_OK && locked==HAL_OK && matches;
}
archive_media_t board_archive_media(void)
{
    return (archive_media_t){NULL,read_media,erase_media,program_media};
}
