#include "godmode.h"
#include "ui.h"
#include "hid.h"
#include "fsinit.h"
#include "fsdrive.h"
#include "fsutil.h"
#include "fsperm.h"
#include "gameutil.h"
#include "filetype.h"
#include "platform.h"
#include "nand.h"
#include "virtual.h"
#include "image.h"

#define N_PANES 2

#define COLOR_TOP_BAR   ((GetWritePermissions() & (PERM_A9LH&~PERM_SYSNAND)) ? COLOR_DARKRED : (GetWritePermissions() & PERM_SYSNAND) ? COLOR_RED : (GetWritePermissions() & PERM_MEMORY) ? COLOR_BRIGHTBLUE : (GetWritePermissions() & (PERM_EMUNAND|PERM_IMAGE)) ? COLOR_BRIGHTYELLOW : GetWritePermissions() ? COLOR_BRIGHTGREEN : COLOR_WHITE)   
#define COLOR_SIDE_BAR  COLOR_DARKGREY
#define COLOR_MARKED    COLOR_TINTEDYELLOW
#define COLOR_FILE      COLOR_TINTEDGREEN
#define COLOR_DIR       COLOR_TINTEDBLUE
#define COLOR_ROOT      COLOR_GREY
#define COLOR_ENTRY(e)  (((e)->marked) ? COLOR_MARKED : ((e)->type == T_DIR) ? COLOR_DIR : ((e)->type == T_FILE) ? COLOR_FILE : ((e)->type == T_ROOT) ?  COLOR_ROOT : COLOR_GREY)

#define COLOR_HVOFFS    RGB(0x40, 0x60, 0x50)
#define COLOR_HVOFFSI   COLOR_DARKESTGREY
#define COLOR_HVASCII   RGB(0x40, 0x80, 0x50)
#define COLOR_HVHEX(i)  ((i % 2) ? RGB(0x30, 0x90, 0x30) : RGB(0x30, 0x80, 0x30))

typedef struct {
    char path[256];
    u32 cursor;
    u32 scroll;
} PaneData;

void DrawUserInterface(const char* curr_path, DirEntry* curr_entry, DirStruct* clipboard, u32 curr_pane) {
    const u32 n_cb_show = 8;
    const u32 bartxt_start = (FONT_HEIGHT_EXT == 10) ? 1 : 2;
    const u32 bartxt_x = 2;
    const u32 bartxt_rx = SCREEN_WIDTH_TOP - (19*FONT_WIDTH_EXT) - bartxt_x;
    const u32 info_start = 18;
    const u32 instr_x = (SCREEN_WIDTH_TOP - (36*FONT_WIDTH_EXT)) / 2;
    char tempstr[64];
    
    static u32 state_prev = 0xFFFFFFFF;
    u32 state_curr =
        ((*curr_path) ? (1<<0) : 0) |
        ((clipboard->n_entries) ? (1<<1) : 0) |
        ((CheckSDMountState()) ? (1<<2) : 0) |
        ((GetMountState()) ? (1<<3) : 0) |
        (curr_pane<<4);
    
    if (state_prev != state_curr) {
        ClearScreenF(true, false, COLOR_STD_BG);
        state_prev = state_curr;
    }
    
    // top bar - current path & free/total storage
    DrawRectangle(TOP_SCREEN, 0, 0, SCREEN_WIDTH_TOP, 12, COLOR_TOP_BAR);
    if (strncmp(curr_path, "", 256) != 0) {
        char bytestr0[32];
        char bytestr1[32];
        TruncateString(tempstr, curr_path, 240 / FONT_WIDTH_EXT, 8);
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, tempstr);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "LOADING...");
        FormatBytes(bytestr0, GetFreeSpace(curr_path));
        FormatBytes(bytestr1, GetTotalSpace(curr_path));
        snprintf(tempstr, 64, "%s/%s", bytestr0, bytestr1);
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", tempstr);
    } else {
        DrawStringF(TOP_SCREEN, bartxt_x, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "[root]");
        DrawStringF(TOP_SCREEN, bartxt_rx, bartxt_start, COLOR_STD_BG, COLOR_TOP_BAR, "%19.19s", "GodMode9");
    }
    
    // left top - current file info
    if (curr_pane) snprintf(tempstr, 63, "PANE #%lu", curr_pane);
    else snprintf(tempstr, 63, "CURRENT");
    DrawStringF(TOP_SCREEN, 2, info_start, COLOR_STD_FONT, COLOR_STD_BG, "[%s]", tempstr);
    // file / entry name
    ResizeString(tempstr, curr_entry->name, 160 / FONT_WIDTH_EXT, 8, false);
    u32 color_current = COLOR_ENTRY(curr_entry);
    DrawStringF(TOP_SCREEN, 4, info_start + 12, color_current, COLOR_STD_BG, "%s", tempstr);
    // size (in Byte) or type desc
    if (curr_entry->type == T_DIR) {
        ResizeString(tempstr, "(dir)", 160 / FONT_WIDTH_EXT, 8, false);
    } else if (curr_entry->type == T_DOTDOT) {
        snprintf(tempstr, 21, "%20s", "");
    } else if (curr_entry->type == T_ROOT) {
        int drvtype = DriveType(curr_entry->path);
        char drvstr[32];
        snprintf(drvstr, 31, "(%s%s)", 
            ((drvtype & DRV_SDCARD) ? "SD" : (drvtype & DRV_RAMDRIVE) ? "RAMdrive" : (drvtype & DRV_GAME) ? "Game" :
            (drvtype & DRV_SYSNAND) ? "SysNAND" : (drvtype & DRV_EMUNAND) ? "EmuNAND" : (drvtype & DRV_IMAGE) ? "Image" :
            (drvtype & DRV_XORPAD) ? "XORpad" : (drvtype & DRV_MEMORY) ? "Memory" : (drvtype & DRV_ALIAS) ? "Alias" :
            (drvtype & DRV_SEARCH) ? "Search" : ""),
            ((drvtype & DRV_FAT) ? " FAT" : (drvtype & DRV_VIRTUAL) ? " Virtual" : ""));
        ResizeString(tempstr, drvstr, 160 / FONT_WIDTH_EXT, 8, false);
    }else {
        char numstr[32];
        char bytestr[32];
        FormatNumber(numstr, curr_entry->size);
        snprintf(bytestr, 31, "%s Byte", numstr);
        ResizeString(tempstr, bytestr, 160 / FONT_WIDTH_EXT, 8, false);
    }
    DrawStringF(TOP_SCREEN, 4, info_start + 12 + 10, color_current, COLOR_STD_BG, tempstr);
    // path of file (if in search results)
    if ((DriveType(curr_path) & DRV_SEARCH) && strrchr(curr_entry->path, '/')) {
        char dirstr[256];
        strncpy(dirstr, curr_entry->path, 256);
        *(strrchr(dirstr, '/')+1) = '\0';
        ResizeString(tempstr, dirstr, 160 / FONT_WIDTH_EXT, 8, false);
        DrawStringF(TOP_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, tempstr);
    } else {
        ResizeString(tempstr, "", 160 / FONT_WIDTH_EXT, 8, false);
        DrawStringF(TOP_SCREEN, 4, info_start + 12 + 10 + 10, color_current, COLOR_STD_BG, tempstr);
    }
    
    // right top - clipboard
    DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 160, info_start, COLOR_STD_FONT, COLOR_STD_BG, "%*s", 160 / FONT_WIDTH_EXT,
        (clipboard->n_entries) ? "[CLIPBOARD]" : "");
    for (u32 c = 0; c < n_cb_show; c++) {
        u32 color_cb = COLOR_ENTRY(&(clipboard->entry[c]));
        ResizeString(tempstr, (clipboard->n_entries > c) ? clipboard->entry[c].name : "", 160 / FONT_WIDTH_EXT, 8, true);
        DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 160 - 4, info_start + 12 + (c*10), color_cb, COLOR_STD_BG, tempstr);
    }
    *tempstr = '\0';
    if (clipboard->n_entries > n_cb_show) snprintf(tempstr, 60, "+ %lu more", clipboard->n_entries - n_cb_show);
    DrawStringF(TOP_SCREEN, SCREEN_WIDTH_TOP - 160 - 4, info_start + 12 + (n_cb_show*10), COLOR_DARKGREY, COLOR_STD_BG, "%*s",
        160 / FONT_WIDTH_EXT, tempstr);
    
    // bottom: inctruction block
    char instr[512];
    snprintf(instr, 512, "%s%s\n%s%s%s%s%s%s%s%s",
        #ifndef SAFEMODE
        "GodMode9 Explorer v", VERSION, // generic start part
        #else
        "SafeMode9 Explorer v", VERSION, // generic start part
        #endif
        (*curr_path) ? ((clipboard->n_entries == 0) ? "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - COPY file(s) / [+R] CREATE dir\n" :
        "L - MARK files (use with \x18\x19\x1A\x1B)\nX - DELETE / [+R] RENAME file(s)\nY - PASTE file(s) / [+R] CREATE dir\n") :
        ((GetWritePermissions() > PERM_BASE) ? "R+Y - Relock write permissions\n" : "R+Y - Unlock write permissions\n"),
        (*curr_path) ? "" : (CheckSDMountState()) ? "R+B - Unmount SD card\n" : "R+B - Remount SD card\n",
        (*curr_path) ? "" : (GetMountState()) ? "R+X - Unmount image\n" : "",
        (*curr_path) ? "R+A - Search directory\n" : "R+A - Search drive\n", 
        "R+L - Make a Screenshot\n",
        "R+\x1B\x1A - Switch to prev/next pane\n",
        (clipboard->n_entries) ? "SELECT - Clear Clipboard\n" : "SELECT - Restore Clipboard\n", // only if clipboard is full
        "START - Reboot / [+R] Poweroff"); // generic end part
    DrawStringF(TOP_SCREEN, instr_x, SCREEN_HEIGHT - 4 - GetDrawStringHeight(instr), COLOR_STD_FONT, COLOR_STD_BG, instr);
}

void DrawDirContents(DirStruct* contents, u32 cursor, u32* scroll) {
    const int str_width = (SCREEN_WIDTH_BOT-3) / FONT_WIDTH_EXT;
    const u32 bar_height_min = 32;
    const u32 bar_width = 2;
    const u32 start_y = 2;
    const u32 stp_y = 12;
    const u32 pos_x = 0;
    const u32 lines = (SCREEN_HEIGHT-start_y+stp_y-1) / stp_y;
    u32 pos_y = start_y;
     
    if (*scroll > cursor) *scroll = cursor;
    else if (*scroll + lines <= cursor) *scroll = cursor - lines + 1;
    if (*scroll + lines > contents->n_entries)
        *scroll = (contents->n_entries > lines) ? contents->n_entries - lines : 0;
    
    for (u32 i = 0; pos_y < SCREEN_HEIGHT; i++) {
        char tempstr[str_width + 1];
        u32 offset_i = *scroll + i;
        u32 color_font = COLOR_WHITE;
        if (offset_i < contents->n_entries) {
            DirEntry* curr_entry = &(contents->entry[offset_i]);
            char namestr[str_width - 10 + 1];
            char bytestr[10 + 1];
            color_font = (cursor != offset_i) ? COLOR_ENTRY(curr_entry) : COLOR_STD_FONT;
            FormatBytes(bytestr, curr_entry->size);
            ResizeString(namestr, curr_entry->name, str_width - 10, str_width - 20, false);
            snprintf(tempstr, str_width + 1, "%s%10.10s", namestr,
                (curr_entry->type == T_DIR) ? "(dir)" : (curr_entry->type == T_DOTDOT) ? "(..)" : bytestr);
        } else snprintf(tempstr, str_width + 1, "%-*.*s", str_width, str_width, "");
        DrawStringF(BOT_SCREEN, pos_x, pos_y, color_font, COLOR_STD_BG, tempstr);
        pos_y += stp_y;
    }
    
    if (contents->n_entries > lines) { // draw position bar at the right      
        u32 bar_height = (lines * SCREEN_HEIGHT) / contents->n_entries;
        if (bar_height < bar_height_min) bar_height = bar_height_min;
        u32 bar_pos = ((u64) *scroll * (SCREEN_HEIGHT - bar_height)) / (contents->n_entries - lines);
        
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, bar_pos, COLOR_STD_BG);
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, bar_pos + bar_height, bar_width, SCREEN_WIDTH_BOT - (bar_pos + bar_height), COLOR_STD_BG);
        DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, bar_pos, bar_width, bar_height, COLOR_SIDE_BAR);
    } else DrawRectangle(BOT_SCREEN, SCREEN_WIDTH_BOT - bar_width, 0, bar_width, SCREEN_HEIGHT, COLOR_STD_BG);
}

u32 SdFormatMenu(void) {
    const u32 emunand_size_table[6] = { 0x0, 0x0, 0x3AF, 0x4D8, 0x3FF, 0x7FF };
    const u32 cluster_size_table[5] = { 0x0, 0x0, 0x4000, 0x8000, 0x10000 };
    const char* option_emunand_size[6] = { "No EmuNAND", "O3DS NAND size", "N3DS NAND size", "1GB (legacy size)", "2GB (legacy size)", "User input..." };
    const char* option_cluster_size[4] = { "Auto", "16KB Clusters", "32KB Clusters", "64KB Clusters" }; 
    u32 cluster_size = 0;
    u64 sdcard_size_mb = 0;
    u64 emunand_size_mb = (u64) -1;
    u32 user_select;
    
    // check actual SD card size
    sdcard_size_mb = GetSDCardSize() / 0x100000;
    if (!sdcard_size_mb) {
        ShowPrompt(false, "Error: SD card not detected.");
        return 1;
    }
    
    user_select = ShowSelectPrompt(6, option_emunand_size, "Format SD card (%lluMB)?\nChoose EmuNAND size:", sdcard_size_mb);
    if (user_select && (user_select < 6)) {
        emunand_size_mb = emunand_size_table[user_select];
    } else if (user_select == 6) do {
        emunand_size_mb = ShowNumberPrompt(0, "SD card size is %lluMB.\nEnter EmuNAND size (MB) below:", sdcard_size_mb);
        if (emunand_size_mb == (u64) -1) break;
    } while (emunand_size_mb > sdcard_size_mb);
    if (emunand_size_mb == (u64) -1) return 1;
    
    user_select = ShowSelectPrompt(4, option_cluster_size, "Format SD card (%lluMB)?\nChoose cluster size:", sdcard_size_mb);
    if (!user_select) return 1;
    else cluster_size = cluster_size_table[user_select];
    
    if (!FormatSDCard(emunand_size_mb, cluster_size)) {
        ShowPrompt(false, "Format SD: failed!");
        return 1;
    }
    
    if (CheckA9lh()) {
        InitSDCardFS(); // on A9LH: copy the payload from mem to SD root
        FileSetData("0:/arm9loaderhax.bin", (u8*) 0x23F00000, 0x40000, 0, true);
        DeinitSDCardFS();
    }
    
    VirtualFile nand;
    if (!GetVirtualFile(&nand, "S:/nand_minsize.bin"))
        return 0;
    if ((nand.size / (1024*1024) <= emunand_size_mb) && ShowPrompt(true, "Clone SysNAND to RedNAND now?")) {
        if (!PathCopy("E:", "S:/nand_minsize.bin", NULL))
            ShowPrompt(false, "Cloning SysNAND to EmuNAND: failed!");
    }
    
    return 0;
}

u32 HexViewer(const char* path) {
    static const u32 max_data = (SCREEN_HEIGHT / 8) * 16;
    static u32 mode = 0;
    u8* data = TEMP_BUFFER;
    u8* bottom_cpy = TEMP_BUFFER + 0xC0000; // a copy of the bottom screen framebuffer
    u32 fsize = FileGetSize(path);
    
    bool dual_screen;
    int x_off, x_hex, x_ascii;
    u32 vpad, hlpad, hrpad;
    u32 rows, cols;
    u32 total_shown;
    u32 total_data;
    
    u32 last_mode = 0xFF;
    u32 last_offset = (u32) -1;
    u32 offset = 0;
    
    u32 found_offset = (u32) -1;
    u32 found_size = 0;
    
    static const u32 edit_bsize = 0x4000; // should be multiple of 0x200 * 2
    bool edit_mode = false;
    u8* edit_buffer = TEMP_BUFFER;
    u8* edit_buffer_cpy = TEMP_BUFFER + edit_bsize;
    u32 edit_start;
    int cursor = 0;
    
    static bool show_instr = true;
    if (show_instr) { // show one time instructions
        ShowPrompt(false, "Hexeditor Controls:\n \n\x18\x19\x1A\x1B(+R) - Scroll\nR+Y - Switch view\nX - Search / goto...\nA - Enter edit mode\nA+\x18\x19\x1A\x1B - Edit value\nB - Exit\n");
        show_instr = false;
    }
    
    memcpy(bottom_cpy, BOT_SCREEN, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
    
    while (true) {
        if (mode != last_mode) {
            switch (mode) { // display mode
                #ifdef FONT_6X10
                case 1:
                    vpad = 0;
                    hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (FONT_WIDTH_EXT * cols);
                    x_hex = x_off + (8*FONT_WIDTH_EXT) + 16;
                    dual_screen = false;
                    break;
                default:
                    mode = 0; 
                    vpad = 0;
                    hlpad = hrpad = 3;
                    cols = 8;
                    x_off = 30 + (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (FONT_WIDTH_EXT * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + (2*FONT_WIDTH_EXT) + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
                #else
                case 1:
                    vpad = hlpad = hrpad = 1;
                    cols = 12;
                    x_off = 0;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = x_off + (8*8) + 12;
                    dual_screen = false;
                    break;
                case 2:
                    vpad = 1;
                    hlpad = 0;
                    hrpad = 1;
                    cols = 16;
                    x_off = -1;
                    x_ascii = SCREEN_WIDTH_TOP - (8 * cols);
                    x_hex = 0;
                    dual_screen = false;
                    break;
                case 3:
                    vpad = hlpad = hrpad = 1;
                    cols = 16;
                    x_off = 20;
                    x_ascii = -1;
                    x_hex = x_off + (8*8) + 12;
                    dual_screen = false;
                    break;
                default:
                    mode = 0; 
                    vpad = hlpad = hrpad = 2;
                    cols = 8;
                    x_off = (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) / 2;
                    x_ascii = SCREEN_WIDTH_TOP - x_off - (8 * cols);
                    x_hex = (SCREEN_WIDTH_TOP - ((hlpad + 16 + hrpad) * cols)) / 2;
                    dual_screen = true;
                    break;
                #endif
            }
            rows = (dual_screen ? 2 : 1) * SCREEN_HEIGHT / (FONT_HEIGHT_EXT + (2*vpad));
            total_shown = rows * cols;
            last_mode = mode;
            ClearScreenF(true, dual_screen, COLOR_STD_BG);
            if (!dual_screen) memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
        }
        // fix offset (if required)
        if (offset % cols) offset -= (offset % cols); // fix offset (align to cols)
        if (offset + total_shown > fsize + cols) // if offset too big
            offset = (total_shown > fsize) ? 0 : (fsize + cols - total_shown - (fsize % cols));
        // get data, using max data size (if new offset)
        if (offset != last_offset) {
            if (!edit_mode) {
                total_data = FileGetData(path, data, max_data, offset);
            } else { // edit mode - read from memory
                if ((offset < edit_start) || (offset + max_data > edit_start + edit_bsize))
                    offset = last_offset; // we don't expect this to happen
                total_data = (fsize - offset >= max_data) ? max_data : fsize - offset;
                data = edit_buffer + (offset - edit_start);
            }
            last_offset = offset;
        }
        
        // display data on screen
        for (u32 row = 0; row < rows; row++) {
            char ascii[16 + 1] = { 0 };
            u32 y = row * (FONT_HEIGHT_EXT + (2*vpad)) + vpad;
            u32 curr_pos = row * cols;
            u32 cutoff = (curr_pos >= total_data) ? 0 : (total_data >= curr_pos + cols) ? cols : total_data - curr_pos;
            u32 marked0 = (found_size && (offset <= found_offset)) ? found_offset - offset : 0;
            u32 marked1 = marked0 + found_size;
            u8* screen = TOP_SCREEN;
            u32 x0 = 0;
            
            // fix marked0 / marked1 offsets for current row
            marked0 = (marked0 < curr_pos) ? 0 : (marked0 >= curr_pos + cols) ? cols : marked0 - curr_pos;
            marked1 = (marked1 < curr_pos) ? 0 : (marked1 >= curr_pos + cols) ? cols : marked1 - curr_pos;
            
            if (y >= SCREEN_HEIGHT) { // switch to bottom screen
                y -= SCREEN_HEIGHT;
                screen = BOT_SCREEN;
                x0 = 40;
            }
            
            memcpy(ascii, data + curr_pos, cols);
            for (u32 col = 0; col < cols; col++)
                if ((col >= cutoff) || (ascii[col] == 0x00)) ascii[col] = ' ';
            
            // draw offset / ASCII representation
            if (x_off >= 0) DrawStringF(screen, x_off - x0, y, cutoff ? COLOR_HVOFFS : COLOR_HVOFFSI,
                COLOR_STD_BG, "%08X", (unsigned int) offset + curr_pos);
            if (x_ascii >= 0) {
                DrawString(screen, ascii, x_ascii - x0, y, COLOR_HVASCII, COLOR_STD_BG);
                for (u32 i = marked0; i < marked1; i++)
                    DrawCharacter(screen, ascii[i % cols], x_ascii - x0 + (FONT_WIDTH_EXT * i), y, COLOR_MARKED, COLOR_STD_BG);
                if (edit_mode && ((u32) cursor / cols == row)) DrawCharacter(screen, ascii[cursor % cols],
                    x_ascii - x0 + FONT_WIDTH_EXT * (cursor % cols), y, COLOR_RED, COLOR_STD_BG);
            }
            
            // draw HEX values
            for (u32 col = 0; (col < cols) && (x_hex >= 0); col++) {
                u32 x = (x_hex + hlpad) + (((2*FONT_WIDTH_EXT) + hrpad + hlpad) * col) - x0;
                u32 hex_color = (edit_mode && ((u32) cursor == curr_pos + col)) ? COLOR_RED :
                    ((col >= marked0) && (col < marked1)) ? COLOR_MARKED : COLOR_HVHEX(col);
                if (col < cutoff)
                    DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "%02X", (unsigned int) data[curr_pos + col]);
                else DrawStringF(screen, x, y, hex_color, COLOR_STD_BG, "  ");
            }
        }
        
        // handle user input
        u32 pad_state = InputWait();
        if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_L1)) CreateScreenshot();
        else if (!edit_mode) { // standard viewer mode
            u32 step_ud = (pad_state & BUTTON_R1) ? (0x1000  - (0x1000  % cols)) : cols;
            u32 step_lr = (pad_state & BUTTON_R1) ? (0x10000 - (0x10000 % cols)) : total_shown;
            if (pad_state & BUTTON_DOWN) offset += step_ud;
            else if (pad_state & BUTTON_RIGHT) offset += step_lr;
            else if (pad_state & BUTTON_UP) offset = (offset > step_ud) ? offset - step_ud : 0;
            else if (pad_state & BUTTON_LEFT) offset = (offset > step_lr) ? offset - step_lr : 0;
            else if ((pad_state & BUTTON_R1) && (pad_state & BUTTON_Y)) mode++;
            else if (pad_state & BUTTON_A) edit_mode = true;
            else if (pad_state & (BUTTON_B|BUTTON_START)) break;
            else if (found_size && (pad_state & BUTTON_R1) && (pad_state & BUTTON_X)) {
                u8 data[64] = { 0 };
                FileGetData(path, data, found_size, found_offset);
                found_offset = FileFindData(path, data, found_size, found_offset + 1);
                ClearScreenF(true, false, COLOR_STD_BG);
                if (found_offset == (u32) -1) {
                    ShowPrompt(false, "Not found!");
                    found_size = 0;
                } else offset = found_offset;
            } else if (pad_state & BUTTON_X) {
                const char* optionstr[3] = { "Go to offset", "Search for string", "Search for data" };
                u32 user_select = ShowSelectPrompt(3, optionstr, "Current offset: %08X\nSelect action:", 
                    (unsigned int) offset);
                if (user_select == 1) { // -> goto offset
                    u64 new_offset = ShowHexPrompt(offset, 8, "Current offset: %08X\nEnter new offset below.",
                        (unsigned int) offset);
                    if (new_offset != (u64) -1) offset = new_offset;
                } else if (user_select == 2) {
                    char string[64 + 1] = { 0 };
                    if (found_size) FileGetData(path, (u8*) string, (found_size <= 64) ? found_size : 64, found_offset);
                    if (ShowStringPrompt(string, 64 + 1, "Enter search string below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = strnlen(string, 64);
                        found_offset = FileFindData(path, (u8*) string, found_size, offset);
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                } else if (user_select == 3) {
                    u8 data[64] = { 0 };
                    u32 size = 0;
                    if (found_size) size = FileGetData(path, data, (found_size <= 64) ? found_size : 64, found_offset);
                    if (ShowDataPrompt(data, &size, "Enter search data below.\n(R+X to repeat search)", (unsigned int) offset)) {
                        found_size = size;
                        found_offset = FileFindData(path, data, size, offset);
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (found_offset == (u32) -1) {
                            ShowPrompt(false, "Not found!");
                            found_size = 0;
                        } else offset = found_offset;
                    }
                }
            }
            if (edit_mode && CheckWritePermissions(path)) { // setup edit mode
                found_size = 0;
                found_offset = (u32) -1;
                cursor = 0;
                edit_start = ((offset - (offset % 0x200) <= (edit_bsize / 2)) || (fsize < edit_bsize)) ? 0 : 
                    offset - (offset % 0x200) - (edit_bsize / 2);
                FileGetData(path, edit_buffer, edit_bsize, edit_start);
                memcpy(edit_buffer_cpy, edit_buffer, edit_bsize);
                data = edit_buffer + (offset - edit_start);
            } else edit_mode = false;
        } else { // editor mode
            if (pad_state & (BUTTON_B|BUTTON_START)) {
                edit_mode = false;
                // check for user edits
                u32 diffs = 0;
                for (u32 i = 0; i < edit_bsize; i++) if (edit_buffer[i] != edit_buffer_cpy[i]) diffs++;
                if (diffs && ShowPrompt(true, "You made edits in %i place(s).\nWrite changes to file?", diffs))
                    if (!FileSetData(path, edit_buffer, min(edit_bsize, (fsize - edit_start)), edit_start, false))
                        ShowPrompt(false, "Failed writing to file!");
                data = TEMP_BUFFER;
                last_offset = (u32) -1; // force reload from file
            } else if (pad_state & BUTTON_A) {
                if (pad_state & BUTTON_DOWN) data[cursor]--;
                else if (pad_state & BUTTON_UP) data[cursor]++;
                else if (pad_state & BUTTON_RIGHT) data[cursor] += 0x10;
                else if (pad_state & BUTTON_LEFT) data[cursor] -= 0x10;
            } else {
                if (pad_state & BUTTON_DOWN) cursor += cols;
                else if (pad_state & BUTTON_UP) cursor -= cols;
                else if (pad_state & BUTTON_RIGHT) cursor++;
                else if (pad_state & BUTTON_LEFT) cursor--;
                // fix cursor position
                if (cursor < 0) {
                    if (offset >= cols) {
                        offset -= cols;
                        cursor += cols;
                    } else cursor = 0;
                } else if (((u32) cursor >= total_data) && (total_data < total_shown)) {
                    cursor = total_data - 1;
                } else if ((u32) cursor >= total_shown) {
                    if (offset + total_shown == fsize) {
                        cursor = total_shown - 1;
                    } else {
                        offset += cols;
                        cursor = (offset + cursor >= fsize) ? fsize - offset - 1 : cursor - cols;
                    }
                }
            }
        }
    }
    
    ClearScreenF(true, false, COLOR_STD_BG);
    memcpy(BOT_SCREEN, bottom_cpy, (SCREEN_HEIGHT * SCREEN_WIDTH_BOT * 3));
    return 0;
}

u32 Sha256Calculator(const char* path) {
    u32 drvtype = DriveType(path);
    char pathstr[32 + 1];
    u8 sha256[32];
    TruncateString(pathstr, path, 32, 8);
    if (!FileGetSha256(path, sha256)) {
        ShowPrompt(false, "Calculating SHA-256: failed!");
        return 1;
    } else {
        static char pathstr_prev[32 + 1] = { 0 };
        static u8 sha256_prev[32] = { 0 };
        char sha_path[256];
        u8 sha256_file[32];
        
        snprintf(sha_path, 256, "%s.sha", path);
        bool have_sha = (FileGetData(sha_path, sha256_file, 32, 0) == 32);
        bool write_sha = !have_sha && (drvtype & DRV_SDCARD); // writing only on SD
        if (ShowPrompt(write_sha, "%s\n%016llX%016llX\n%016llX%016llX%s%s%s%s%s",
            pathstr, getbe64(sha256 + 0), getbe64(sha256 + 8), getbe64(sha256 + 16), getbe64(sha256 + 24),
            (have_sha) ? "\nSHA verification: " : "",
            (have_sha) ? ((memcmp(sha256, sha256_file, 32) == 0) ? "passed!" : "failed!") : "",
            (memcmp(sha256, sha256_prev, 32) == 0) ? "\n \nIdentical with previous file:\n" : "",
            (memcmp(sha256, sha256_prev, 32) == 0) ? pathstr_prev : "",
            (write_sha) ? "\n \nWrite .SHA file?" : "") && !have_sha && write_sha) {
            FileSetData(sha_path, sha256, 32, 0, true);
        }
        
        strncpy(pathstr_prev, pathstr, 32 + 1);
        memcpy(sha256_prev, sha256, 32);
    }
    
    return 0;
}

u32 FileHandlerMenu(char* current_path, u32* cursor, u32* scroll, DirStruct* current_dir, DirStruct* clipboard) {
    DirEntry* curr_entry = &(current_dir->entry[*cursor]);
    const char* optionstr[8];
    
    // check for file lock
    if (!FileUnlock(curr_entry->path)) return 1;
    
    u32 filetype = IdentifyFileType(curr_entry->path);
    u32 drvtype = DriveType(curr_entry->path);
    
    // special stuff, only available on FAT drives (see int special below)
    bool mountable = ((filetype & FTYPE_MOUNTABLE) && !(drvtype & DRV_IMAGE));
    bool verificable = (filetype & FYTPE_VERIFICABLE);
    bool decryptable = (filetype & FYTPE_DECRYPTABLE);
    bool decryptable_inplace = (decryptable && (drvtype & (DRV_SDCARD|DRV_RAMDRIVE)));
    bool buildable = (filetype & FTYPE_BUILDABLE);
    bool buildable_legit = (filetype & FTYPE_BUILDABLE_L);
    
    char pathstr[32 + 1];
    TruncateString(pathstr, curr_entry->path, 32, 8);
    
    // main menu processing
    int n_opt = 0;
    int special = (filetype && (drvtype & DRV_FAT)) ? ++n_opt : -1;
    int hexviewer = ++n_opt;
    int calcsha = ++n_opt;
    int inject = ((clipboard->n_entries == 1) &&
        (clipboard->entry[0].type == T_FILE) &&
        (drvtype & DRV_FAT) &&
        (strncmp(clipboard->entry[0].path, curr_entry->path, 256) != 0)) ?
        (int) ++n_opt : -1;
    int searchdrv = (DriveType(current_path) & DRV_SEARCH) ? ++n_opt : -1;
    if (special > 0) optionstr[special-1] =
        (filetype == IMG_NAND  ) ? "Mount as NAND image"   :
        (filetype == IMG_FAT   ) ? "Mount as FAT image"    :
        (filetype == GAME_CIA  ) ? "CIA image options..."  :
        (filetype == GAME_NCSD ) ? "NCSD image options..." :
        (filetype == GAME_NCCH ) ? "NCCH image options..." :
        (filetype == GAME_EXEFS) ? "Mount as EXEFS image"  :
        (filetype == GAME_ROMFS) ? "Mount as ROMFS image"  :
        (filetype == GAME_TMD  ) ? "TMD file options..."   :
        (filetype == SYS_TICKDB) ? "Mount as ticket.db" : "???";
    optionstr[hexviewer-1] = "Show in Hexeditor";
    optionstr[calcsha-1] = "Calculate SHA-256";
    if (inject > 0) optionstr[inject-1] = "Inject data @offset";
    if (searchdrv > 0) optionstr[searchdrv-1] = "Open containing folder";
    
    int user_select = ShowSelectPrompt(n_opt, optionstr, pathstr);
    if (user_select == hexviewer) { // -> show in hex viewer
        HexViewer(curr_entry->path);
        return 0;
    } else if (user_select == calcsha) { // -> calculate SHA-256
        Sha256Calculator(curr_entry->path);
        GetDirContents(current_dir, current_path);
        return 0;
    } else if (user_select == inject) { // -> inject data from clipboard
        char origstr[18 + 1];
        TruncateString(origstr, clipboard->entry[0].name, 18, 10);
        u64 offset = ShowHexPrompt(0, 8, "Inject data from %s?\nSpecifiy offset below.", origstr);
        if (offset != (u64) -1) {
            if (!FileInjectFile(curr_entry->path, clipboard->entry[0].path, (u32) offset))
                ShowPrompt(false, "Failed injecting %s", origstr);
            clipboard->n_entries = 0;
        }
        return 0;
    } else if (user_select == searchdrv) { // -> search drive, open containing path
        char* last_slash = strrchr(curr_entry->path, '/');
        if (last_slash) {
            snprintf(current_path, last_slash - curr_entry->path + 1, "%s", curr_entry->path);
            GetDirContents(current_dir, current_path);
            *cursor = 1;
            *scroll = 0;
        }
        return 0;
    } else if (user_select != special) {
        return 1;
    }
    
    // stuff for special menu starts here
    n_opt = 0;
    int mount = (mountable) ? ++n_opt : -1;
    int decrypt = (decryptable) ? ++n_opt : -1;
    int decrypt_inplace = (decryptable_inplace) ? ++n_opt : -1;
    int build = (buildable) ? ++n_opt : -1;
    int build_legit = (buildable_legit) ? ++n_opt : -1;
    int verify = (verificable) ? ++n_opt : -1;
    if (mount > 0) optionstr[mount-1] = "Mount image to drive";
    if (decrypt > 0) optionstr[decrypt-1] = "Decrypt file (SD output)";
    if (decrypt_inplace > 0) optionstr[decrypt_inplace-1] = "Decrypt file (inplace)";
    if (build > 0) optionstr[build-1] = (build_legit < 0) ? "Build CIA from file" : "Build CIA (standard)";
    if (build_legit > 0) optionstr[build_legit-1] = "Build CIA (legit)";
    if (verify > 0) optionstr[verify-1] = "Verify file";
    
    u32 n_marked = 0;
    if (curr_entry->marked) {
        for (u32 i = 0; i < current_dir->n_entries; i++) 
            if (current_dir->entry[i].marked) n_marked++;
    }
    
    // auto select when there is only one option
    user_select = (n_opt > 1) ? (int) ShowSelectPrompt(n_opt, optionstr, pathstr) : n_opt;
    if (user_select == mount) { // -> mount file as image
        if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
            clipboard->n_entries = 0; // remove last mounted image clipboard entries
        InitImgFS(curr_entry->path);
        if (!(DriveType("7:")||DriveType("G:")||DriveType("T:"))) {
            ShowPrompt(false, "Mounting image: failed");
            InitImgFS(NULL);
        } else {
            *cursor = 0;
            *current_path = '\0';
            GetDirContents(current_dir, current_path);
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                if (strspn(current_dir->entry[i].path, "7GI") == 0)
                    continue;
                strncpy(current_path, current_dir->entry[i].path, 256);
                GetDirContents(current_dir, current_path);
                *cursor = 1;
                *scroll = 0;
                break;
            }
        }
        return 0;
    } else if ((user_select == decrypt) || (user_select == decrypt_inplace)) { // -> decrypt game file
        bool inplace = (user_select == decrypt_inplace);
        if ((n_marked > 1) && ShowPrompt(true, "Try to decrypt all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_unencrypted = 0;
            u32 n_other = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (IdentifyFileType(path) != filetype) {
                    n_other++;
                    continue;
                }
                if (CheckEncryptedGameFile(path) != 0) {
                    n_unencrypted++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (DecryptGameFile(path, inplace) == 0) n_success++;
                else { // on failure: set cursor on failed title, break;
                    *cursor = i;
                    break;
                }
            }
            if (n_other || n_unencrypted) {
                ShowPrompt(false, "%lu/%lu files decrypted ok\n%lu/%lu not encrypted\n%lu/%lu not of same type",
                n_success, n_marked, n_unencrypted, n_marked, n_other, n_marked);
            } else ShowPrompt(false, "%lu/%lu files decrypted ok", n_success, n_marked);
            if (!inplace && n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            if (CheckEncryptedGameFile(curr_entry->path) != 0) {
                ShowPrompt(false, "%s\nFile is not encrypted", pathstr);
            } else {
                u32 ret = DecryptGameFile(curr_entry->path, inplace);
                if (inplace || (ret != 0)) ShowPrompt(false, "%s\nDecryption %s", pathstr, (ret == 0) ? "success" : "failed");
                else ShowPrompt(false, "%s\nDecrypted to %s", pathstr, OUTPUT_PATH);
            }
        }
        return 0;
    } else if ((user_select == build) || (user_select == build_legit)) { // -> build CIA
        bool force_legit = (user_select == build_legit);
        if ((n_marked > 1) && ShowPrompt(true, "Try to process all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0; 
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (IdentifyFileType(path) != filetype) {
                    n_other++;
                    continue;
                }
                current_dir->entry[i].marked = false;
                if (BuildCiaFromGameFile(path, force_legit) == 0) n_success++;
                else { // on failure: set *cursor on failed title, break;
                    *cursor = i;
                    break;
                }
            }
            if (n_other) ShowPrompt(false, "%lu/%lu CIAs built ok\n%lu/%lu not of same type",
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu CIAs built ok", n_success, n_marked);
            if (n_success) ShowPrompt(false, "%lu files written to %s", n_success, OUTPUT_PATH);
        } else {
            if (BuildCiaFromGameFile(curr_entry->path, force_legit) == 0)
                ShowPrompt(false, "%s\nCIA built to %s", pathstr, OUTPUT_PATH);
            else ShowPrompt(false, "%s\nCIA build failed", pathstr);
        }
        return 0;
    } else if (user_select == verify) { // -> verify game file
        if ((n_marked > 1) && ShowPrompt(true, "Try to verify all %lu selected files?", n_marked)) {
            u32 n_success = 0;
            u32 n_other = 0; 
            u32 n_processed = 0;
            for (u32 i = 0; i < current_dir->n_entries; i++) {
                const char* path = current_dir->entry[i].path;
                if (!current_dir->entry[i].marked) 
                    continue;
                if (IdentifyFileType(path) != filetype) {
                    n_other++;
                    continue;
                }
                if (!(filetype & (GAME_CIA|GAME_TMD)) &&
                    !ShowProgress(n_processed++, n_marked, path)) break;
                current_dir->entry[i].marked = false;
                if (VerifyGameFile(path) == 0) n_success++;
                else { // on failure: set *cursor on failed title, break;
                    *cursor = i;
                    break;
                }
            }
            if (n_other) ShowPrompt(false, "%lu/%lu files verified ok\n%lu/%lu not of same type",
                n_success, n_marked, n_other, n_marked);
            else ShowPrompt(false, "%lu/%lu files verified ok", n_success, n_marked); 
        } else {
            ShowString("%s\nVerifying file, please wait...", pathstr);
            ShowPrompt(false, "%s\nVerification %s", pathstr,
                (VerifyGameFile(curr_entry->path) == 0) ? "success" : "failed");
        }
        return 0;
    }
    
    return 1;
}

u32 GodMode() {
    static const u32 quick_stp = 20;
    u32 exit_mode = GODMODE_EXIT_REBOOT;
    
    // reserve 480kB for DirStruct, 64kB for PaneData, just to be safe
    static DirStruct* current_dir = (DirStruct*) (DIR_BUFFER + 0x00000);
    static DirStruct* clipboard   = (DirStruct*) (DIR_BUFFER + 0x78000);
    static PaneData* panedata     = (PaneData*)  (DIR_BUFFER + 0xF0000);
    PaneData* pane = panedata;
    char current_path[256] = { 0x00 };
    
    int mark_next = -1;
    u32 last_clipboard_size = 0;
    u32 cursor = 0;
    u32 scroll = 0;
    
    ClearScreenF(true, true, COLOR_STD_BG);
    if ((sizeof(DirStruct) > 0x78000) || (N_PANES * sizeof(PaneData) > 0x10000)) {
        ShowPrompt(false, "Out of memory!"); // just to be safe
        return exit_mode;
    }
    while (!InitSDCardFS()) {
        const char* optionstr[] = { "Retry initialising", "Poweroff system", "Reboot system", "No SD mode (exp.)", "SD format menu" };
        u32 user_select = ShowSelectPrompt(5, optionstr, "Initialising SD card failed!\nSelect action:" );
        if (user_select == 2) return GODMODE_EXIT_POWEROFF;
        else if (user_select == 3) return GODMODE_EXIT_REBOOT;
        else if (user_select == 4) break;
        else if (user_select == 5) SdFormatMenu();
        ClearScreenF(true, true, COLOR_STD_BG);
    }
    InitEmuNandBase();
    InitNandCrypto();
    InitExtFS();
    
    // could also check for a9lh via this: ((*(vu32*) 0x101401C0) == 0) 
    if ((GetUnitPlatform() == PLATFORM_N3DS) && !CheckSlot0x05Crypto()) {
        if (!ShowPrompt(true, "Warning: slot0x05 crypto fail!\nCould not set up slot0x05keyY.\nContinue?")) {
            DeinitExtFS();
            DeinitSDCardFS();
            return exit_mode;
        }
    }
    
    GetDirContents(current_dir, "");
    clipboard->n_entries = 0;
    memset(panedata, 0x00, 0x10000);
    while (true) { // this is the main loop
        int curr_drvtype = DriveType(current_path);
        
        // basic sanity checking
        if (!current_dir->n_entries) { // current dir is empty -> revert to root
            *current_path = '\0';
            DeinitExtFS(); // deinit and...
            InitExtFS(); // reinitialize extended file system
            GetDirContents(current_dir, current_path);
            cursor = 0;
            if (!current_dir->n_entries) { // should not happen, if it does fail gracefully
                ShowPrompt(false, "Invalid directory object");
                return exit_mode;
            }
        }
        if (cursor >= current_dir->n_entries) // cursor beyond allowed range
            cursor = current_dir->n_entries - 1;
        DirEntry* curr_entry = &(current_dir->entry[cursor]);
        if ((mark_next >= 0) && (curr_entry->type != T_DOTDOT)) {
            curr_entry->marked = mark_next;
            mark_next = -2;
        }
        DrawUserInterface(current_path, curr_entry, clipboard, N_PANES ? pane - panedata + 1 : 0);
        DrawDirContents(current_dir, cursor, &scroll);
        u32 pad_state = InputWait();
        bool switched = (pad_state & BUTTON_R1);
        
        // basic navigation commands
        if ((pad_state & BUTTON_A) && (curr_entry->type != T_FILE) && (curr_entry->type != T_DOTDOT)) { // for dirs
            if (switched && !(DriveType(curr_entry->path) & DRV_SEARCH)) { // search directory
                char searchstr[256];
                char namestr[20+1];
                snprintf(searchstr, 256, "*");
                TruncateString(namestr, curr_entry->name, 20, 8);
                if (ShowStringPrompt(searchstr, 256, "Search %s?\nEnter search below.", namestr)) {
                    SetFSSearch(searchstr, curr_entry->path);
                    snprintf(current_path, 256, "Z:");
                    GetDirContents(current_dir, current_path);
                    if (current_dir->n_entries) ShowPrompt(false, "Found %lu results.", current_dir->n_entries - 1);
                    cursor = 1;
                    scroll = 0;
                }
            } else { // one level up
                u32 user_select = 1;
                if (curr_drvtype & DRV_SEARCH) { // special menu for search drive
                    const char* optionstr[2] = { "Open this folder", "Open containing folder" };
                    char pathstr[32 + 1];
                    TruncateString(pathstr, curr_entry->path, 32, 8);
                    user_select = ShowSelectPrompt(2, optionstr, pathstr);
                }
                if (user_select) {
                    strncpy(current_path, curr_entry->path, 256);
                    if (user_select == 2) {
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash) *last_slash = '\0'; 
                    } 
                    GetDirContents(current_dir, current_path);
                    if (*current_path && (current_dir->n_entries > 1)) {
                        cursor = 1;
                        scroll = 0;
                    } else cursor = 0;
                }
            }
        } else if ((pad_state & BUTTON_A) && (curr_entry->type == T_FILE)) { // process a file
            FileHandlerMenu(current_path, &cursor, &scroll, current_dir, clipboard); // processed externally
        } else if (*current_path && ((pad_state & BUTTON_B) || // one level down
            ((pad_state & BUTTON_A) && (curr_entry->type == T_DOTDOT)))) {
            if (switched) { // use R+B to return to root fast
                *current_path = '\0';
                GetDirContents(current_dir, current_path);
                cursor = scroll = 0;
            } else {
                char old_path[256];
                char* last_slash = strrchr(current_path, '/');
                strncpy(old_path, current_path, 256);
                if (last_slash) *last_slash = '\0'; 
                else *current_path = '\0';
                GetDirContents(current_dir, current_path);
                if (*old_path && current_dir->n_entries) {
                    for (cursor = current_dir->n_entries - 1;
                        (cursor > 0) && (strncmp(current_dir->entry[cursor].path, old_path, 256) != 0); cursor--);
                    if (*current_path && !cursor && (current_dir->n_entries > 1)) cursor = 1; // don't set it on the dotdot
                    scroll = 0;
                }
            }
        } else if (switched && (pad_state & BUTTON_B)) { // unmount SD card
            DeinitExtFS();
            if (!CheckSDMountState()) {
                while (!InitSDCardFS() &&
                    ShowPrompt(true, "Reinitialising SD card failed! Retry?"));
            } else {
                DeinitSDCardFS();
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) &
                    (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
                    clipboard->n_entries = 0; // remove SD clipboard entries
            }
            ClearScreenF(true, true, COLOR_STD_BG);
            InitEmuNandBase();
            InitExtFS();
            GetDirContents(current_dir, current_path);
            if (cursor >= current_dir->n_entries) cursor = 0;
        } else if ((pad_state & BUTTON_DOWN) && (cursor + 1 < current_dir->n_entries))  { // cursor down
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor++;
        } else if ((pad_state & BUTTON_UP) && cursor) { // cursor up
            if (pad_state & BUTTON_L1) mark_next = curr_entry->marked;
            cursor--;
        } else if (switched && (pad_state & (BUTTON_RIGHT|BUTTON_LEFT))) { // switch pane
            memcpy(pane->path, current_path, 256);  // store state in current pane
            pane->cursor = cursor;
            pane->scroll = scroll;
            (pad_state & BUTTON_LEFT) ? pane-- : pane++; // switch to next
            if (pane < panedata) pane += N_PANES;
            else if (pane >= panedata + N_PANES) pane -= N_PANES;
            memcpy(current_path, pane->path, 256);  // get state from next pane
            cursor = pane->cursor;
            scroll = pane->scroll;
            GetDirContents(current_dir, current_path);
        } else if ((pad_state & BUTTON_RIGHT) && !(pad_state & BUTTON_L1)) { // cursor down (quick)
            cursor += quick_stp;
        } else if ((pad_state & BUTTON_LEFT) && !(pad_state & BUTTON_L1)) { // cursor up (quick)
            cursor = (cursor >= quick_stp) ? cursor - quick_stp : 0;
        } else if (pad_state & BUTTON_RIGHT) { // mark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 1;
            mark_next = 1;
        } else if (pad_state & BUTTON_LEFT) { // unmark all entries
            for (u32 c = 1; c < current_dir->n_entries; c++) current_dir->entry[c].marked = 0;
            mark_next = 0;
        } else if (switched && (pad_state & BUTTON_L1)) { // switched L -> screenshot
            CreateScreenshot();
            ClearScreenF(true, true, COLOR_STD_BG);
        } else if (*current_path && (pad_state & BUTTON_L1) && (curr_entry->type != T_DOTDOT)) {
            // unswitched L - mark/unmark single entry
            if (mark_next < -1) mark_next = -1;
            else curr_entry->marked ^= 0x1;
        } else if (pad_state & BUTTON_SELECT) { // clear/restore clipboard
            clipboard->n_entries = (clipboard->n_entries > 0) ? 0 : last_clipboard_size;
        }

        // highly specific commands
        if (!*current_path) { // in the root folder...
            if (switched && (pad_state & BUTTON_X)) { // unmount image
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & DRV_IMAGE))
                    clipboard->n_entries = 0; // remove last mounted image clipboard entries
                InitImgFS(NULL);
                ClearScreenF(false, true, COLOR_STD_BG);
                GetDirContents(current_dir, current_path);
            } else if (switched && (pad_state & BUTTON_Y)) {
                SetWritePermissions((GetWritePermissions() > PERM_BASE) ? PERM_BASE : PERM_ALL, false);
            }
        } else if (!switched) { // standard unswitched command set
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & BUTTON_X)) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if (pad_state & BUTTON_X) { // delete a file 
                u32 n_marked = 0;
                for (u32 c = 0; c < current_dir->n_entries; c++)
                    if (current_dir->entry[c].marked) n_marked++;
                if (n_marked) {
                    if (ShowPrompt(true, "Delete %u path(s)?", n_marked)) {
                        u32 n_errors = 0;
                        ShowString("Deleting files, please wait...");
                        for (u32 c = 0; c < current_dir->n_entries; c++)
                            if (current_dir->entry[c].marked && !PathDelete(current_dir->entry[c].path))
                                n_errors++;
                        ClearScreenF(true, false, COLOR_STD_BG);
                        if (n_errors) ShowPrompt(false, "Failed deleting %u/%u path(s)", n_errors, n_marked);
                    }
                } else if (curr_entry->type != T_DOTDOT) {
                    char namestr[36+1];
                    TruncateString(namestr, curr_entry->name, 28, 12);
                    if (ShowPrompt(true, "Delete \"%s\"?", namestr)) {
                        ShowString("Deleting %s\nPlease wait...", namestr);
                        if (!PathDelete(curr_entry->path))
                            ShowPrompt(false, "Failed deleting:\n%s", namestr);
                        ClearScreenF(true, false, COLOR_STD_BG);
                    }
                }
                GetDirContents(current_dir, current_path);
            } else if ((pad_state & BUTTON_Y) && (clipboard->n_entries == 0)) { // fill clipboard
                for (u32 c = 0; c < current_dir->n_entries; c++) {
                    if (current_dir->entry[c].marked) {
                        current_dir->entry[c].marked = 0;
                        DirEntryCpy(&(clipboard->entry[clipboard->n_entries]), &(current_dir->entry[c]));
                        clipboard->n_entries++;
                    }
                }
                if ((clipboard->n_entries == 0) && (curr_entry->type != T_DOTDOT)) {
                    DirEntryCpy(&(clipboard->entry[0]), curr_entry);
                    clipboard->n_entries = 1;
                }
                if (clipboard->n_entries)
                    last_clipboard_size = clipboard->n_entries;
            } else if ((curr_drvtype & DRV_SEARCH) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in search drive");
            } else if ((curr_drvtype & DRV_GAME) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in virtual game path");
            } else if ((curr_drvtype & DRV_XORPAD) && (pad_state & BUTTON_Y)) {
                ShowPrompt(false, "Not allowed in XORpad drive");
            } else if (pad_state & BUTTON_Y) { // paste files
                const char* optionstr[2] = { "Copy path(s)", "Move path(s)" };
                char promptstr[64];
                u32 flags = 0;
                u32 user_select;
                if (clipboard->n_entries == 1) {
                    char namestr[20+1];
                    TruncateString(namestr, clipboard->entry[0].name, 20, 12);
                    snprintf(promptstr, 64, "Paste \"%s\" here?", namestr);
                } else snprintf(promptstr, 64, "Paste %lu paths here?", clipboard->n_entries);
                user_select = ((DriveType(clipboard->entry[0].path) & curr_drvtype & DRV_STDFAT)) ?
                    ShowSelectPrompt(2, optionstr, promptstr) : (ShowPrompt(true, promptstr) ? 1 : 0);
                if (user_select) {
                    for (u32 c = 0; c < clipboard->n_entries; c++) {
                        char namestr[36+1];
                        TruncateString(namestr, clipboard->entry[c].name, 36, 12);
                        flags &= ~ASK_ALL;
                        if (c < clipboard->n_entries - 1) flags |= ASK_ALL;
                        if ((user_select == 1) && !PathCopy(current_path, clipboard->entry[c].path, &flags)) {    
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed copying path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed copying path:\n%s", namestr);
                        } else if ((user_select == 2) && !PathMove(current_path, clipboard->entry[c].path, &flags)) {    
                            if (c + 1 < clipboard->n_entries) {
                                if (!ShowPrompt(true, "Failed moving path:\n%s\nProcess remaining?", namestr)) break;
                            } else ShowPrompt(false, "Failed moving path:\n%s", namestr);
                        }
                    }                        
                    clipboard->n_entries = 0;
                    GetDirContents(current_dir, current_path);
                }
                ClearScreenF(true, false, COLOR_STD_BG);
            }
        } else { // switched command set
            if ((curr_drvtype & DRV_VIRTUAL) && (pad_state & (BUTTON_X|BUTTON_Y))) {
                ShowPrompt(false, "Not allowed in virtual path");
            } else if ((curr_drvtype & DRV_ALIAS) && (pad_state & (BUTTON_X))) {
                ShowPrompt(false, "Not allowed in alias path");
            } else if ((pad_state & BUTTON_X) && (curr_entry->type != T_DOTDOT)) { // rename a file
                char newname[256];
                char namestr[20+1];
                TruncateString(namestr, curr_entry->name, 20, 12);
                snprintf(newname, 255, curr_entry->name);
                if (ShowStringPrompt(newname, 256, "Rename %s?\nEnter new name below.", namestr)) {
                    if (!PathRename(curr_entry->path, newname))
                        ShowPrompt(false, "Failed renaming path:\n%s", namestr);
                    else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, newname, 256) != 0); cursor--);
                    }
                }
            } else if (pad_state & BUTTON_Y) { // create a folder
                char dirname[256];
                snprintf(dirname, 255, "newdir");
                if (ShowStringPrompt(dirname, 256, "Create a new folder here?\nEnter name below.")) {
                    if (!DirCreate(current_path, dirname)) {
                        char namestr[36+1];
                        TruncateString(namestr, dirname, 36, 12);
                        ShowPrompt(false, "Failed creating folder:\n%s", namestr);
                    } else {
                        GetDirContents(current_dir, current_path);
                        for (cursor = (current_dir->n_entries) ? current_dir->n_entries - 1 : 0;
                            (cursor > 1) && (strncmp(current_dir->entry[cursor].name, dirname, 256) != 0); cursor--);
                    }
                }
            }
        }
        
        if (pad_state & BUTTON_START) {
            exit_mode = (switched || (pad_state & BUTTON_LEFT)) ? GODMODE_EXIT_POWEROFF : GODMODE_EXIT_REBOOT;
            break;
        } else if (pad_state & BUTTON_POWER) {
            exit_mode = GODMODE_EXIT_POWEROFF;
            break;
        } else if (pad_state & BUTTON_HOME) { // Home menu
            const char* optionstr[] = { "Poweroff system", "Reboot system", "SD format menu" };
            u32 user_select = ShowSelectPrompt(3, optionstr, "HOME button pressed.\nSelect action:" );
            if (user_select == 1) { 
                exit_mode = GODMODE_EXIT_POWEROFF;
                break;
            } else if (user_select == 2) { 
                exit_mode = GODMODE_EXIT_REBOOT;
                break;
            } else if (user_select == 3) { // format SD card
                bool sd_state = CheckSDMountState();
                if (clipboard->n_entries && (DriveType(clipboard->entry[0].path) & (DRV_SDCARD|DRV_ALIAS|DRV_EMUNAND|DRV_IMAGE)))
                    clipboard->n_entries = 0; // remove SD clipboard entries
                DeinitExtFS();
                DeinitSDCardFS();
                if ((SdFormatMenu() == 0) || sd_state) {;
                    while (!InitSDCardFS() &&
                        ShowPrompt(true, "Reinitialising SD card failed! Retry?"));
                }
                ClearScreenF(true, true, COLOR_STD_BG);
                InitEmuNandBase();
                InitExtFS();
                GetDirContents(current_dir, current_path);
                if (cursor >= current_dir->n_entries)
                    cursor = current_dir->n_entries - 1;
            }
        }
    }
    
    DeinitExtFS();
    DeinitSDCardFS();
    
    return exit_mode;
}
