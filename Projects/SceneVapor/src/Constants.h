#ifndef CONSTANTS_H
#define CONSTANTS_H
#include "ofConstants.h"

#define FAST_READ 0
#define READ_TO_BUFFER 0

#define HDF5_DIRECT 0
#define USE_RAW 0
#define USE_VOXELS_COMPUTE_SHADER 0
#define USE_PARTICLES_COMPUTE_SHADER 1
#define USE_PARTICLES_HISTOGRAM 0
#define USE_VOXELS_DCT_COMPRESSION 0
#define USE_VBO 0
#define USE_HALF_PARTICLE 0

#define MAX_PARTICLE_SIZE 10
#define USE_TEXTURE_3D_MIPMAPS 1
constexpr size_t MAX_NUM_PARTICLES = 5000000;

#endif // CONSTANTS_H
