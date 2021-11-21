#include "cacheWin.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef struct focus_info {
    WINDOW* win;
    int line; // list offset
    int offset; // data offset
} FOCUS_INFO;

void print_info(WINDOW* win, int flag, char* true_info, char* false_info) {
    if (flag) {
        wattron(win, COLOR_PAIR(HIGHLIGHT_COLOR));
        wprintw(win, true_info);
        wattroff(win, COLOR_PAIR(HIGHLIGHT_COLOR));
    } else {
        wprintw(win, false_info);
    }
}

void update_info(WINDOW* outer, WINDOW* inner, CORE* core, FOCUS_INFO* focused) {
    wattron(outer, COLOR_PAIR(TITLE_COLOR));
    box(outer, 0, 0); mvwprintw(outer, 0, 2, " Block Info ");
    wattroff(outer, COLOR_PAIR(TITLE_COLOR));

    CACHE_BLOCK cache_block = core->mmu->data_cache->blocks[focused->line];
    CACHE_ADDR_HELPER base_addr = { .d = {
        .tag = cache_block.tag,
        .set_idx = cache_block.set_idx
    } };

    mvwprintw(inner, 0, 0, "block idx = %u : ", focused->line);
    print_info(inner, cache_block.valid, "valid, ", "invalid, ");
    print_info(inner, cache_block.modified, "modified", "unmodified");
    mvwprintw(inner, 1, 0, "tag = 0x%X, set idx = %u -> ", cache_block.tag, cache_block.set_idx);
    wprintw(inner, "0x%08X", base_addr.raw);
    
    wrefresh(outer);
    wrefresh(inner);
}

void update_list(WINDOW* outer, WINDOW* inner, CORE* core, FOCUS_INFO* focused) {
    if (focused->win == outer) wattron(outer, COLOR_PAIR(STANDOUT_COLOR));
    else wattron(outer, COLOR_PAIR(TITLE_COLOR));
    box(outer, 0, 0); mvwprintw(outer, 0, 2, " Block List ");
    wattroff(outer, COLOR_PAIR(TITLE_COLOR));
    wattroff(outer, COLOR_PAIR(STANDOUT_COLOR));

    focused->line = max(0, min(focused->line, BLOCK_NUM - 1));
    mvwprintw(outer, 1, 7, (focused->line > 0) ? "^" : " ");
    mvwprintw(outer, 18, 7, (focused->line < BLOCK_NUM - 1) ?"v" : " ");

    for (int i = focused->line; i < min(focused->line + 16, BLOCK_NUM); i++) {
        char block_name[15];
        sprintf(block_name, "block %u", i);
        if (i == focused->line)
            wattron(inner, A_STANDOUT);
        wmove(inner, i - focused->line, 0);
        print_info(inner, core->mmu->data_cache->blocks[i].valid, block_name, block_name);
        wattroff(inner, A_STANDOUT);
    }

    wrefresh(outer);
    wrefresh(inner);
}

void update_detail(WINDOW* outer, WINDOW* inner, CORE* core, FOCUS_INFO* focused) {
    if (focused->win == outer) wattron(outer, COLOR_PAIR(STANDOUT_COLOR));
    else wattron(outer, COLOR_PAIR(TITLE_COLOR));
    box(outer, 0, 0); mvwprintw(outer, 0, 2, " Block Detail ");
    wattroff(outer, COLOR_PAIR(TITLE_COLOR));
    wattroff(outer, COLOR_PAIR(STANDOUT_COLOR));

    focused->offset = max(0, min(focused->offset, (BLOCK_SIZE / 0x10) - 1));
    mvwprintw(outer, 1, 31, (focused->offset > 0) ? "^" : " ");
    mvwprintw(outer, 18, 31, (focused->offset < (BLOCK_SIZE / 0x10) - 1) ? "v" : " ");

    CACHE_BLOCK cache_block = core->mmu->data_cache->blocks[focused->line];
    CACHE_ADDR_HELPER helper = { .d = {
        .tag = cache_block.tag,
        .set_idx = cache_block.set_idx
    } };
    for (int i = focused->offset; i < min(focused->offset + 16, BLOCK_SIZE / 0x10); i++) {
        wattron(inner, COLOR_PAIR(SUBTITLE_COLOR));
        helper.d.offset = i * 0x10;
        mvwprintw(inner, i - focused->offset, 0, "0x%08X   ", helper.raw);
        wattroff(inner, COLOR_PAIR(SUBTITLE_COLOR));
        for (int j = 0; j < 0x10; j++)
            wprintw(inner, " %02X", cache_block.data[i * 0x10 + j]);
    }

    wrefresh(outer);
    wrefresh(inner);
}

void show_cache_win(CORE* core) {
    clear();
    // create outer windows
    WINDOW* info_outer = newwin(4, 80, 0, 0);
    WINDOW* list_outer = newwin(20, 16, 4, 0);
    WINDOW* detail_outer = newwin(20, 64, 4, 16);
    WINDOW* info_inner = newwin(2, 78, 1, 1);
    WINDOW* list_inner = newwin(16, 14, 6, 1);
    WINDOW* detail_inner = newwin(16, 62, 6, 17);
    // refresh
    refresh();
    // main loop
    FOCUS_INFO focused = { .win = list_outer, .line = 0, .offset = 0 };
    int* offset = &focused.line;
    keypad(stdscr, 1);
    while (1) {
        update_list(list_outer, list_inner, core, &focused);
        update_info(info_outer, info_inner, core, &focused);
        update_detail(detail_outer, detail_inner, core, &focused);
        switch (getch()) {
        case KEY_UP: *offset -= 1; break;
        case KEY_DOWN: *offset += 1; break;
        case KEY_LEFT: focused.win = list_outer; offset = &focused.line; break;
        case KEY_RIGHT: focused.win = detail_outer; offset = &focused.offset; *offset = 0; break;
        default: keypad(stdscr, 0); return;
        }
    }
}