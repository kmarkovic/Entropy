/*
 *  Environment.cpp
 *
 *  Copyright (c) 2016, Neil Mendoza, http://www.neilmendoza.com
 *  All rights reserved. 
 *  
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met: 
 *  
 *  * Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 *  * Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *  * Neither the name of Neil Mendoza nor the names of its contributors may be used 
 *    to endorse or promote products derived from this software without 
 *    specific prior written permission. 
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *  POSSIBILITY OF SUCH DAMAGE. 
 *
 */
#include "Environment.h"

namespace nm
{
	Environment::Environment(const glm::vec3& min, const glm::vec3& max) 
		: min(min)
		, max(max)
		, dims(max - min)
	{
	}

	void Environment::update(){

		switch(state.get()){
			case BARYOGENESIS:
				stateStr = "BARYOGENESIS";
			break;
			case STANDARD_MODEL:
				stateStr = "STANDARD MODEL";
			break;
			case NUCLEOSYNTHESIS:
				stateStr = "NUCLEOSYNTHESIS";
			break;
			case TRANSITION_OUT:
				stateStr = "TRANSITION OUT";
			break;
		}
	}

	float Environment::getExpansionScalar() const
	{
		return 1.f + (.5f - .5f * energy);
	}

	float Environment::getForceMultiplier() const
	{
		return forceMultiplierMin + energy * (forceMultiplierMax - forceMultiplierMin);
	}

	float Environment::getAnnihilationThresh() const
	{
		return annihilationThreshMin + energy * (annihilationThreshMax - annihilationThreshMin);
	}

	float Environment::getFusionThresh() const
	{
		if(state == BARYOGENESIS){
			float exponent = fusionThresholdExponentMin + energy * (fusionThresholdExponentMax - fusionThresholdExponentMin);
			return pow(100, exponent);
		}else{
//			float exponent = fusionThresholdExponentMin + energy * (fusionThresholdExponentMax - fusionThresholdExponentMin);
//			return pow(10, exponent);

			return fusionThresholdExponentMin + energy * (fusionThresholdExponentMax - fusionThresholdExponentMin);

		}
		//return fusionThresholdMin + energy * (fusionThresholdMax - fusionThresholdMin);
	}

	float Environment::getPairProductionThresh() const
	{
		return pairProductionThresholdMin + energy * (pairProductionThresholdMax - pairProductionThresholdMin);
	}
}
