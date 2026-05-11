#include <zephyr/linker/iterable_sections.h>

ITERABLE_SECTION_RAM(task_params, Z_LINK_ITERABLE_SUBALIGN)
ITERABLE_SECTION_RAM(resource_params, Z_LINK_ITERABLE_SUBALIGN)
