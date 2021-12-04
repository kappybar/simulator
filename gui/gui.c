#include "gui.h"
#include "splashWin.h"
#include "helpWin.h"
#include "mainWin.h"
#include "analysisWin.h"
#include "cacheWin.h"

typedef struct command {
    char type;
    int argc;
    int argv[10];
} COMMAND;

int reg2idx(char* reg) {
    // special fp
    if (!strcmp(reg, "fp"))
        return 8;
    // try regs and fregs
    for (int idx = 0; idx < 32; idx++) {
        if (!strcmp(reg, reg_name[idx]))
            return idx;
        if (!strcmp(reg, freg_name[idx]))
            return 32 + idx;
    }
    return 0;
}

COMMAND get_command() {
    // echo + blinking cursor
    echo();
    curs_set(1);
    // prepare a COMMAND struct
    COMMAND com;
    com.argc = 0;
    // get instruction
    char input[73], output[12][12];
    WINDOW* com_win = newwin(1, 77, 22, 2);
    wclear(com_win);
    mvwgetstr(com_win, 0, 0, input);
    // split with space (exactly 1 space)
    int counter = 0, argc = 0;
    strcat(input, " "); // add an end point
    for (int idx = 0; idx < strlen(input); idx++) {
        if (input[idx] == ' ') {
            output[argc][counter] = '\0';
            argc++;
            counter = 0;
        } else {
            output[argc][counter] = input[idx];
            counter++;
        }
    }
    // pack into COMMAND
    if (!strcmp(output[0], "quit")) {
        com.type = 'q';
    } else if (!strcmp(output[0], "step")) {
        com.type = 's';
        com.argc = 1;
        com.argv[0] = argc > 1 ? atoi(output[1]) : 0x7FFFFFFF;
    } else if (!strcmp(output[0], "reg")) {
        com.type = 'r';
        com.argc = 1;
        com.argv[0] = (argc > 1) ? reg2idx(output[1]) : -1;
    } else if (!strcmp(output[0], "instr")) {
        com.type = 'm';
        com.argv[0] = 'i';
        if (argc > 1) {
            com.argc = 2;
            sscanf(output[1], "0x%X", com.argv + 1);
            if (!com.argv[1]) // not recognizable
                com.argc = 1;
        } else {
            com.argc = 1;
        }
    } else if (!strcmp(output[0], "data")) {
        com.type = 'm';
        com.argv[0] = 'd';
        if (argc > 1) {
            com.argc = 2;
            sscanf(output[1], "0x%X", com.argv + 1);
            if (!com.argv[0]) // not recognizable
                com.argc = 1;
        } else {
            com.argc = 1;
        }
    } else if (!strcmp(output[0], "help")) {
        com.type = 'h';
    } else if (!strcmp(output[0], "analysis")) {
        com.type = 'a';
    } else if (!strcmp(output[0], "cache")) {
        com.type = 'c';
    } else {
        // parse as step 1
        com.type = 's';
        com.argc = 1;
        com.argv[0] = 1;
    }
    // noecho + hide cursor
    noecho();
    curs_set(0);
    return com;
}

STATE wait4command(GUI* gui, CORE* core) {
    COMMAND com = get_command();
    switch (com.type) {
    case 'q':
        return STAT_QUIT;
    case 's':
        return STAT_STEP | ((u64)com.argv[0] << 32);
    case 'r':
        gui->focused_win = REG_WIN;
        gui->reg_start = (com.argv[0] < 0) ? gui->reg_start : com.argv[0];
        show_main_win(gui, core);
        return STAT_HALT;
    case 'm':
        gui->focused_win = MEM_WIN;
        gui->mem_type = com.argv[0] == 'i' ? MEM_INSTR : MEM_DATA;
        gui->mem_start = com.argc > 1 ? (com.argv[1] >> 4) : (gui->mem_type ? DEFAULT_PC >> 4 : 0);
        show_main_win(gui, core);
        return STAT_HALT;
    case 'a':
        show_analysis_win(core);
        return STAT_HALT;
    case 'c':
        show_cache_win(core);
        return STAT_HALT;
    case 'h':
        show_help_win();
        return STAT_HALT;
    default:
        return STAT_HALT;
    }
}

STATE gui_update(GUI* gui, CORE* core) {
    show_main_win(gui, core);
    // wait for a new command
    return wait4command(gui, core);
}

void gui_deinit(GUI* gui) {
    // at the end of program ncurses mode should be exited properly
    endwin();
}

void init_gui(GUI* gui) {
    char title[] = " RISC-V simulator ";
    // resize the terminal to 80 * 24
    printf("\e[8;24;80t");
    // initialize ncurses mode, and end it in deinit()
    initscr();
    noecho();
    curs_set(0);
    // set colors if supported
    if (has_colors()) start_color();
    init_pair(TITLE_COLOR, COLOR_BLUE, COLOR_BLACK);
    init_pair(SUBTITLE_COLOR, COLOR_CYAN, COLOR_BLACK);
    init_pair(WARNING_COLOR, COLOR_RED, COLOR_BLACK);
    init_pair(HIGHLIGHT_COLOR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(STANDOUT_COLOR, COLOR_YELLOW, COLOR_BLACK);
    // regist variables
    gui->focused_win = COM_WIN;
    gui->reg_start = 0;
    memset(gui->reg_focus, 0, 64);
    gui->mem_type = MEM_INSTR;
    gui->mem_start = DEFAULT_PC / 0x10;
    // assign interfaces
    gui->update = gui_update;
    gui->deinit = gui_deinit;

    show_splash_win();
    if (getch() == 'h') show_help_win();
}
