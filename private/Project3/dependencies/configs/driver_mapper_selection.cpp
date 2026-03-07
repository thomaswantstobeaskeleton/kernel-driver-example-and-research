#include "driver_mapper_selection.h"

/* Mapper selection for LoadDriver(). 0=kdmapper, 1=Aether.Mapper, 2=rtcore vulnerability mapper, 3=LegitMemory. */
int g_mapper_type = 0;
std::string g_mapper_directory;
