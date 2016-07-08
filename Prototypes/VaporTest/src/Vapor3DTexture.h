#ifndef VAPOR_3D_TEXTURE_H
#define VAPOR_3D_TEXTURE_H

#include "ofConstants.h"
#include "ofVectorMath.h"
#include "ofPixels.h"
#include "ofxRange.h"
#include "Particle.h"

class Vapor3DTexture
{
	public:
		void setup(const std::vector<Particle> & particles, size_t size, float minDensity, float maxDensity, ofxRange3f coordsRange);
		size_t size() const;
		const std::vector<float> & data() const;
		std::pair<float,float> minmax() const;
	private:
		inline void add(size_t x, size_t y, size_t z, float value);
		std::vector<float> m_data;
		size_t m_size, m_cubesize, m_quadsize;
};

#endif // OCTREE_H
