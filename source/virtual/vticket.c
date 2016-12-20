#include "vticket.h"
#include "image.h"
#include "ticket.h"
#include "ui.h" // for wait message

#define VFLAG_DECDB         (1<<26)
#define VFLAG_ENCDB         (1<<27)
#define VFLAG_FAKE          (1<<28)
#define VFLAG_UNKNOWN       (1<<29)
#define VFLAG_ESHOP         (1<<30)
#define VFLAG_SYSTEM        (1<<31)
#define VFLAG_TICKDIR       (VFLAG_SYSTEM|VFLAG_ESHOP|VFLAG_UNKNOWN|VFLAG_FAKE)
#define VFLAG_MEMORY        (VFLAG_DECDB|VFLAG_ENCDB)

#define NAME_TIK            "%02lX.%016llX.%08lX" // index / title id / console id

// only for the main directory
static const VirtualFile vTicketFileTemplates[] = {
    { "system"           , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_SYSTEM },
    { "eshop"            , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_ESHOP },
    { "unknown"          , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_UNKNOWN },
    { "fake"             , 0x00000000, 0x00000000, 0xFF, VFLAG_DIR | VFLAG_FAKE },
    { "EncTitleKeys.bin" , 0x00000000, 0x00000000, 0xFF, VFLAG_ENCDB },
    { "DecTitleKeys.bin" , 0x00000000, 0x00000000, 0xFF, VFLAG_DECDB }
};

static TicketInfo* tick_info     = (TicketInfo*) VGAME_BUFFER; // first 512kb reserved (enough for ~10000 entries)
static TitleKeysInfo* tik_e_info = (TitleKeysInfo*) (VGAME_BUFFER + 0x80000); // 256kb reserved
static TitleKeysInfo* tik_d_info = (TitleKeysInfo*) (VGAME_BUFFER + 0xC0000); // 256kb reserved

u32 InitVTicketDrive(void) { // prerequisite: ticket.db mounted as image
    const u32 area_offsets[] = { TICKDB_AREA_OFFSETS };
    if (!(GetMountState() & SYS_TICKDB)) return 1;
    
    // reset internal dbs
    memset(tick_info, 0, 16);
    memset(tik_e_info, 0, 16);
    memset(tik_d_info, 0, 16);
    
    // parse file, sector by sector
    u32 tick_count = 0;
    for (u32 p = 0; p < sizeof(area_offsets); p++) {
        u32 offset_area = area_offsets[p];
        for (u32 i = 0; i < TICKDB_AREA_SIZE; i += 0x200) {
            u8 data[0x400];
            Ticket* ticket = (Ticket*) (data + 0x18);
            if (ReadImageBytes(data, offset_area + i, 0x400) != 0) {
                tick_info->n_entries = 0;
                return 1;
            }
            if ((getle32(data + 0x10) == 0) || (getle32(data + 0x14) != sizeof(Ticket))) continue;
            if (ValidateTicket(ticket) != 0) continue;
            if (++tick_count % 100 == 0)
                ShowString("Tickets found: %lu", tick_count);
            AddTicketInfo(tick_info, ticket, offset_area + i + 0x18);
        }
    }
    
    // init titlekey databases
    BuildTitleKeyInfo(tik_e_info, tick_info, false);
    BuildTitleKeyInfo(tik_d_info, tick_info, true);
    
    return SYS_TICKDB;
}

u32 CheckVTicketDrive(void) {
    if ((GetMountState() & SYS_TICKDB) && tick_info->n_entries) // very basic sanity check
        return SYS_TICKDB;
    tick_info->n_entries = 0;
    return 0;
}

bool ReadVTicketDir(VirtualFile* vfile, VirtualDir* vdir) {
    if (vdir->flags & VFLAG_TICKDIR) { // ticket dir
        while (++vdir->index < (int) tick_info->n_entries) {
            TicketEntry* tick_entry = tick_info->entries + vdir->index;
            
            u64 ticket_id = getbe64(tick_entry->ticket_id);
            u32 ck_idx = tick_entry->commonkey_idx;
            if (!(((vdir->flags & VFLAG_SYSTEM) && ticket_id && (ck_idx == 1)) ||
                ((vdir->flags & VFLAG_ESHOP) && ticket_id && (ck_idx == 0)) ||
                ((vdir->flags & VFLAG_UNKNOWN) && ticket_id && (ck_idx > 1)) ||
                ((vdir->flags & VFLAG_FAKE) && !ticket_id)))
                continue;
            
            memset(vfile, 0, sizeof(VirtualFile));
            snprintf(vfile->name, 32, NAME_TIK, tick_entry->commonkey_idx, getbe64(tick_entry->title_id), getbe32(tick_entry->console_id));
            vfile->offset = tick_entry->offset;
            vfile->size = sizeof(Ticket);
            vfile->keyslot = 0xFF;
            vfile->flags = vdir->flags & ~VFLAG_DIR;
            
            return true; // found
        }
    } else { // root dir
        int n_templates = sizeof(vTicketFileTemplates) / sizeof(VirtualFile);
        const VirtualFile* templates = vTicketFileTemplates;
        while (++vdir->index < n_templates) {
            // copy current template to vfile
            memcpy(vfile, templates + vdir->index, sizeof(VirtualFile));
            // process flags
            if (vfile->flags & VFLAG_ENCDB) {
                vfile->size = 16 + (tik_e_info->n_entries * sizeof(TitleKeyEntry));
                vfile->offset = (u32) tik_e_info;
            } else if (vfile->flags & VFLAG_DECDB) {
                vfile->size = 16 + (tik_d_info->n_entries * sizeof(TitleKeyEntry));
                vfile->offset = (u32) tik_d_info;
            }
            
            return true; // found
        }
    }
    
    return false;
}

int ReadVTicketFile(const VirtualFile* vfile, u8* buffer, u32 offset, u32 count) {
    u32 foffset = vfile->offset + offset;
    if (vfile->flags & VFLAG_MEMORY) memcpy(buffer, (u8*) foffset, count);
    else return ReadImageBytes(buffer, foffset, count);
    return 0;
}
