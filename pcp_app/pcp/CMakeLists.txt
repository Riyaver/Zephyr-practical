cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pcp_app)

zephyr_linker_sources(DATA_SECTIONS data-sections-ram.ld)
zephyr_linker_sources(SECTIONS sections-rom.ld)

target_sources(app PRIVATE src/main.c src/tasks.c src/critical_section.c src/display.c)
