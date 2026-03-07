#pragma once

#include <string>

/* Mapper selection for LoadDriver(). 0=kdmapper, 1=Aether.Mapper, 2=rtcore vulnerability mapper, 3=LegitMemory (untested). */
extern int g_mapper_type;
/* Custom folder for mapper exe; when non-empty, loader uses this directory + mapper exe name first. Set via "Pick directory" in Misc. */
extern std::string g_mapper_directory;
