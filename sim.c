#include "sim.h"

u64 get_file_size(char* file_name) {
    FILE* file = fopen(file_name, "rb");
    if (file == NULL) {
        return 0;
    } else {
        fseek(file, 0, SEEK_END);
        u64 size = (u64)ftell(file);
        fclose(file);
        return size;
    }
}

void sim_load_file(SIM* sim, char* file_name) {
    char code_name[36], data_name[36];
    sprintf(code_name, "./bin/%s.code", file_name);
    sprintf(data_name, "./bin/%s.data", file_name);
    u64 file_size;
    // load instr
    file_size = get_file_size(code_name) >> 2;
    if (file_size) {
        sim->core->mmu->allocate_instr(sim->core->mmu, (DEFAULT_PC >> 2) + file_size);
        FILE* file = fopen(code_name, "rb");
        u32 word = 0;
        ADDR addr = DEFAULT_PC >> 2;
        for (int i = 0; i < file_size; i++) {
            fread(&word, 1, 4, file);
            format2big(word);
            sim->core->mmu->write_instr(sim->core->mmu, addr++, word);
        }
        fclose(file);
    } else {
        printf("invalid file name: %s.\n", file_name);
        exit(-1);
    }
    // load data
    file_size = get_file_size(data_name);
    if (file_size) {
        FILE *file = fopen(data_name, "rb");
        u8 byte = 0;
        ADDR addr = 0;
        for (int i = 0; i < file_size; i++) {
            fread(&byte, 1, 1, file);
            sim->core->mmu->data_mem->write_byte(sim->core->mmu->data_mem, addr++, byte);
        }
        fclose(file);
    }
}

void sim_run(SIM* sim) {
    #if defined(TIME_TEST_MODE)
    // timer
    clock_t t1, t2;
    BROADCAST(STAT_STEP | ((u64)STAT_INFO_MAX << STAT_SHIFT_AMOUNT));
    t1 = clock();
    #else
    // init GUI
    static GUI gui;
    init_gui(&gui);
    sim->gui = &gui;
    #endif
    // main loop of simulator
    for (;;) {
        switch (BROADCAST.decoder.type) {
        case STAT_STEP:
            if (BROADCAST.decoder.info > 0) {
                // step forward
                sim->core->step(sim->core);
                BROADCAST.decoder.info--;
            } else if (BROADCAST.decoder.info < 0) {
                // roll back
                s64 remains = (s64)sim->core->instr_counter + BROADCAST.decoder.info;
                sim->core->reset(sim->core);
                s64 steps = max(0, remains);
                BROADCAST(STAT_STEP | ((u64)steps << STAT_SHIFT_AMOUNT));
            } else {
                BROADCAST(STAT_HALT);
            }
            break;
        case STAT_HALT:
            BROADCAST(sim->gui->update(sim->gui, sim->core));
            break;
        case STAT_EXIT:
            #if defined(TIME_TEST_MODE)
            t2 = clock();
            u64 num = sim->core->instr_counter;
            printf("%llu instructions in %ld clk, %lf per sec\n", num, t2 - t1, (f64)num * CLOCKS_PER_SEC / (f64)(t2 - t1));
            sim->core->deinit(sim->core);
            return;
            #endif
        case STAT_MEM_EXCEPTION:
        case STAT_INSTR_EXCEPTION:
            BROADCAST(sim->gui->update(sim->gui, sim->core));
            if (BROADCAST.decoder.type != STAT_QUIT)
                BROADCAST(STAT_EXIT);
            break;
        case STAT_DUMPING:
            if (BROADCAST.decoder.info > 0) {
                // step forward and record (almost same with STAT_STEP)
                sim->core->step(sim->core);
                sim->core->dump(sim->core, --BROADCAST.decoder.info);
            } else {
                // ignore negative step number
                BROADCAST(STAT_HALT);
            }
            break;
        case STAT_QUIT:
            sim->core->deinit(sim->core);
            sim->gui->deinit(sim->gui);
            return;
        default:
            break;
        }
    }
}

void init_sim(SIM* sim) {
    // init core
    static CORE core;
    init_core(&core);
    sim->core = &core;
    // assign interfaces
    sim->load = sim_load_file;
    sim->run = sim_run;
    // broadcast state
    BROADCAST(STAT_HALT);
}
