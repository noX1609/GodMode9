#include "filetype.h"
#include "fsutil.h"
#include "game.h"

u32 IdentifyFileType(const char* path) {
    const u8 romfs_magic[] = { ROMFS_MAGIC };
    const u8 tickdb_magic[] = { TICKDB_MAGIC };
    u8 header[0x200] __attribute__((aligned(32))); // minimum required size
    size_t fsize = FileGetSize(path);
    if (FileGetData(path, header, 0x200, 0) != 0x200) return 0;
    
    if ((getbe32(header + 0x100) == 0x4E435344) && (getbe64(header + 0x110) == (u64) 0x0104030301000000) &&
        (getbe64(header + 0x108) == (u64) 0) && (fsize >= 0x8FC8000)) {
        return IMG_NAND; // NAND image
    } else if (getbe16(header + 0x1FE) == 0x55AA) { // migt be FAT or MBR
        if ((strncmp((char*) header + 0x36, "FAT12   ", 8) == 0) || (strncmp((char*) header + 0x36, "FAT16   ", 8) == 0) ||
            (strncmp((char*) header + 0x36, "FAT     ", 8) == 0) || (strncmp((char*) header + 0x52, "FAT32   ", 8) == 0) ||
            ((getle64(header + 0x36) == 0) && (getle16(header + 0x0B) == 0x200))) { // last one is a special case for public.sav
            return IMG_FAT; // this is an actual FAT header
        } else if (((getle32(header + 0x1BE + 0x8) + getle32(header + 0x1BE + 0xC)) < (fsize / 0x200)) && // check file size
            (getle32(header + 0x1BE + 0x8) > 0) && (getle32(header + 0x1BE + 0xC) >= 0x800) && // check first partition sanity
            ((header[0x1BE + 0x4] == 0x1) || (header[0x1BE + 0x4] == 0x4) || (header[0x1BE + 0x4] == 0x6) || // filesystem type
             (header[0x1BE + 0x4] == 0xB) || (header[0x1BE + 0x4] == 0xC) || (header[0x1BE + 0x4] == 0xE))) {
            return IMG_FAT; // this might be an MBR -> give it the benefit of doubt
        }
    } else if (ValidateCiaHeader((CiaHeader*) (void*) header) == 0) {
        // this only works because these functions ignore CIA content index
        CiaInfo info;
        GetCiaInfo(&info, (CiaHeader*) header);
        if (fsize >= info.size_cia)
            return GAME_CIA; // CIA file
    } else if (ValidateNcsdHeader((NcsdHeader*) (void*) header) == 0) {
        NcsdHeader* ncsd = (NcsdHeader*) (void*) header;
        if (fsize >= GetNcsdTrimmedSize(ncsd))
            return GAME_NCSD; // NCSD (".3DS") file
    } else if (ValidateNcchHeader((NcchHeader*) (void*) header) == 0) {
        NcchHeader* ncch = (NcchHeader*) (void*) header;
        if (fsize >= (ncch->size * NCCH_MEDIA_UNIT))
            return GAME_NCCH; // NCSD (".3DS") file
    } else if (ValidateExeFsHeader((ExeFsHeader*) (void*) header, fsize) == 0) {
        return GAME_EXEFS; // ExeFS file
    } else if (memcmp(header, romfs_magic, sizeof(romfs_magic)) == 0) {
        return GAME_ROMFS; // RomFS file (check could be better)
    } else if (strncmp(TMD_ISSUER, (char*) (header + 0x140), 0x40) == 0) {
        if (fsize >= TMD_SIZE_N(getbe16(header + 0x1DE)))
            return GAME_TMD; // TMD file
    } else if (memcmp(header + 0x100, tickdb_magic, sizeof(tickdb_magic)) == 0) {
        return SYS_TICKDB; // ticket.db
    }
    
    return 0;
}
