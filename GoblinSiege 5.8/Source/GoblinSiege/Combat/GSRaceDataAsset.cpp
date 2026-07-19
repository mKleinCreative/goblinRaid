#include "Combat/GSRaceDataAsset.h"

// Pure data (UPrimaryDataAsset) - see header. In-editor, create one instance per race:
//   DA_Race_Goblin  (all multipliers 1.0x - the baseline)
//   DA_Race_Human   (Torch/Fire 1.3x, Greatclub 1.1x, rest 1.0x)
//   DA_Race_Elf     (Dagger 1.3x, Bow 1.1x, Greatclub 0.8x, Shadow 0.7x, Blood 1.3x, Fire 1.4x)
//   DA_Race_Dwarf   (Dagger 0.7x, Bow 0.8x, Greatclub 1.3x, Shadow 1.3x, Blood 0.8x, Fire 0.6x)
// under Content/Data/Races/, matching the tables in each race-design-*.md doc exactly.
