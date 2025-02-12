#include "SequenceRamses.h"
#include "Constants.h"

namespace ent
{
    //--------------------------------------------------------------
    SequenceRamses::SequenceRamses()
		: m_frameRate(30.0f)
	    , m_currFrame(-1)
    {
		clear();
    }

    //--------------------------------------------------------------
    SequenceRamses::~SequenceRamses()
    {
		clear();
    }

	//--------------------------------------------------------------
	void SequenceRamses::setupRemote(const std::string& urlFolder, const std::string& folder, int startIndex, int endIndex){
		clear();
		m_folder = folder;
		m_urlFolder = urlFolder;
		setup(startIndex, endIndex);
	}

	//--------------------------------------------------------------
	void SequenceRamses::setup(const std::string& folder, int startIndex, int endIndex)
	{
		clear();
		m_folder = folder;
		setup(startIndex, endIndex);
	}


	//--------------------------------------------------------------
	void SequenceRamses::setup(int startIndex, int endIndex){
		int numFiles = endIndex - startIndex + 1;
		if (numFiles <= 0) {
			ofLogError("SequenceRamses::loadSequence") << "Invalid range [" << startIndex << ", " << endIndex << "]";
			return;
		}

		m_startIndex = startIndex;
		m_endIndex = endIndex;

		// Load the shaders.
		m_renderShader.setupShaderFromFile(GL_VERTEX_SHADER, "shaders/render.vert");
		m_renderShader.setupShaderFromFile(GL_FRAGMENT_SHADER, "shaders/render.frag");
		m_renderShader.bindAttribute(POSITION_ATTRIBUTE, "position");
		m_renderShader.bindAttribute(SIZE_ATTRIBUTE, "size");
		m_renderShader.bindAttribute(DENSITY_ATTRIBUTE, "density");
		m_renderShader.linkProgram();
		m_renderShader.begin();
		m_renderShader.setUniform1f("uDensityMin", m_densityMin * m_densityRange.getSpan());
		m_renderShader.setUniform1f("uDensityMax", m_densityMax * m_densityRange.getSpan());
		m_renderShader.end();

#if USE_VOXELS_COMPUTE_SHADER
		frameSettings.voxels2texture.setupShaderFromFile(GL_COMPUTE_SHADER, "shaders/voxels2texture3d.glsl");
		frameSettings.voxels2texture.linkProgram();
		nextFrameSettings.voxels2texture = frameSettings.voxels2texture;
		int maxBufferTextureSize;
		glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufferTextureSize);
		auto maxNumVoxelsBytes = 128*1024*1024; //MB
		if(maxNumVoxelsBytes>maxBufferTextureSize){
			cout << "voxels buffer size " << maxNumVoxelsBytes/1024/1024 << "MB > max buffer texture size " << maxBufferTextureSize/1024./1024. << "MB" << endl;
			std::exit(0);
		}
		frameSettings.voxelsBuffer.allocate(maxNumVoxelsBytes, GL_STATIC_DRAW);
		frameSettings.voxelsTexture.allocateAsBufferTexture(frameSettings.voxelsBuffer, GL_RG32I);
		nextFrameSettings.voxelsBuffer.allocate(maxNumVoxelsBytes, GL_STATIC_DRAW);
		nextFrameSettings.voxelsTexture.allocateAsBufferTexture(nextFrameSettings.voxelsBuffer, GL_RG32I);
#endif

#if USE_PARTICLES_COMPUTE_SHADER
		frameSettings.particles2texture.setupShaderFromFile(GL_COMPUTE_SHADER, "shaders/particles2texture3d.glsl");
		frameSettings.particles2texture.linkProgram();
		nextFrameSettings.particles2texture = frameSettings.particles2texture;
		int maxBufferTextureSize;
		glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufferTextureSize);
#if USE_HALF_PARTICLE
		auto maxNumParticlesBytes = MAX_NUM_PARTICLES*sizeof(HalfParticle);
#else
		auto maxNumParticlesBytes = MAX_NUM_PARTICLES*sizeof(Particle);
#endif
		if(maxNumParticlesBytes>maxBufferTextureSize){
			cout << "voxels buffer size " << maxNumParticlesBytes/1024/1024 << "MB > max buffer texture size " << maxBufferTextureSize/1024./1024. << "MB" << endl;
			std::exit(0);
		}
		frameSettings.particlesBuffer.allocate(maxNumParticlesBytes, GL_STATIC_DRAW);
		nextFrameSettings.particlesBuffer.allocate(maxNumParticlesBytes, GL_STATIC_DRAW);
#if USE_HALF_PARTICLE
		frameSettings.particlesTexture.allocateAsBufferTexture(frameSettings.particlesBuffer, GL_RG16F);
		nextFrameSettings.particlesTexture.allocateAsBufferTexture(nextFrameSettings.particlesBuffer, GL_RG16F);
#else
		frameSettings.particlesTexture.allocateAsBufferTexture(frameSettings.particlesBuffer, GL_RG32F);
		nextFrameSettings.particlesTexture.allocateAsBufferTexture(nextFrameSettings.particlesBuffer, GL_RG32F);
#endif
#endif

		namespace fs = std::filesystem;
		m_lastVertTime = fs::last_write_time(ofFile("shaders/volumetrics_vertex.glsl", ofFile::Reference));
		m_lastFragTime = fs::last_write_time(ofFile("shaders/volumetrics_frag.glsl", ofFile::Reference));
		m_lastIncludesTime = fs::last_write_time(ofFile("shaders/defines.glsl", ofFile::Reference));

		m_volumetricsShader.load("shaders/volumetrics_vertex.glsl", "shaders/volumetrics_frag.glsl");
		m_volumetricsShader.begin();
		m_volumetricsShader.setUniform1f("minDensity", m_densityMin * m_densityRange.getSpan());
		m_volumetricsShader.setUniform1f("maxDensity", m_densityMax * m_densityRange.getSpan());
		m_volumetricsShader.end();

		frameSettings.worldsize = 512;
		m_clearData.assign(frameSettings.worldsize*frameSettings.worldsize*frameSettings.worldsize,0);
		frameSettings.volumeTexture.allocate(frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, GL_R16F);
		frameSettings.volumeTexture.loadData(
			m_clearData.data(),
			frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, 0,0,0, GL_RED
		);

		nextFrameSettings.worldsize = 512;
		nextFrameSettings.volumeTexture.allocate(frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, GL_R16F);
		nextFrameSettings.volumeTexture.loadData(
			m_clearData.data(),
			frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, 0,0,0, GL_RED
		);

		composite.allocate(frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, GL_R16F);
		composite.loadData(
			m_clearData.data(),
			frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, 0,0,0, GL_RED
		);

		volumetrics.setup(&composite, ofVec3f(1,1,1), m_volumetricsShader);
		volumetrics.setRenderSettings(1, m_volumeQuality, m_volumeDensity, 0);
		glBindTexture(composite.texData.textureTarget,composite.texData.textureID);
		glTexParameteri(composite.texData.textureTarget, GL_TEXTURE_SWIZZLE_R, GL_RED);
		glTexParameteri(composite.texData.textureTarget, GL_TEXTURE_SWIZZLE_G, GL_RED);
		glTexParameteri(composite.texData.textureTarget, GL_TEXTURE_SWIZZLE_B, GL_RED);
		glTexParameteri(composite.texData.textureTarget, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
		glBindTexture(composite.texData.textureTarget,0);

		mix3d.setupShaderFromFile(GL_COMPUTE_SHADER, "shaders/mix3D.comp");
		mix3d.linkProgram();


		listeners.push_back(m_densityMin.newListener([this](float & density){
			if(density != prevMinDensity){
				loadFrame(m_currentIndex, m_currentIndex, m_currentIndex + 1);
			}
			prevMinDensity = density;
		}));
		listeners.push_back(m_densityMax.newListener([this](float & density){
			if(density != prevMaxDensity){
				cout << density << " " << prevMaxDensity << endl;
				loadFrame(m_currentIndex, m_currentIndex, m_currentIndex + 1);
			}
			prevMaxDensity = density;
		}));

		m_bReady = true;
	}

	//--------------------------------------------------------------
	void SequenceRamses::clear()
	{
		m_folder = "";
		m_urlFolder = "";
		m_startIndex = 0;
		m_endIndex = 0;

		m_coordRange.clear();
		m_sizeRange.clear();
		m_densityRange.clear();

		m_currFrame = 0;

		m_bReady = false;
	}

    //--------------------------------------------------------------
    void SequenceRamses::update()
    {
		namespace fs = std::filesystem;
		auto vertTime = fs::last_write_time(ofFile("shaders/volumetrics_vertex.glsl", ofFile::Reference));
		auto fragTime = fs::last_write_time(ofFile("shaders/volumetrics_frag.glsl", ofFile::Reference));
		auto includesTime = fs::last_write_time(ofFile("shaders/defines.glsl", ofFile::Reference));

		if(vertTime > m_lastVertTime || fragTime > m_lastFragTime || includesTime > m_lastIncludesTime){
			cout << "reloading" << endl;
			ofShader newShader;
			// Load the shaders.
			auto loaded = newShader.load("shaders/volumetrics_vertex.glsl", "shaders/volumetrics_frag.glsl");
			if(loaded){
				m_volumetricsShader = newShader;
				volumetrics.setup(&composite, ofVec3f(1,1,1), m_volumetricsShader);
				volumetrics.setRenderSettings(1, m_volumeQuality, m_volumeDensity, 0);
				cout << "reloaded" << endl;
			}

			m_lastVertTime = vertTime;
			m_lastFragTime = fragTime;
			m_lastIncludesTime = includesTime;
		}
		m_renderShader.begin();
		m_renderShader.setUniform1f("uDensityMin", m_densityMin * m_densityRange.getSpan());
		m_renderShader.setUniform1f("uDensityMax", m_densityMax * m_densityRange.getSpan());
		m_renderShader.end();
		m_volumetricsShader.begin();
		m_volumetricsShader.setUniform1f("minDensity", m_densityMin * m_densityRange.getSpan());
		m_volumetricsShader.setUniform1f("maxDensity", m_densityMax * m_densityRange.getSpan());
		m_volumetricsShader.end();
    }

    //--------------------------------------------------------------
    void SequenceRamses::draw(float scale)
    {
        if (m_bRender) 
		{
            ofSetColor(ofColor::white);
			//glPointSize(10.0);

            ofPushMatrix();
            ofScale(scale / m_normalizeFactor, scale / m_normalizeFactor, scale / m_normalizeFactor);
            ofTranslate(m_originShift.x, m_originShift.y, m_originShift.z);
            {
				m_renderShader.begin();
				{
					getSnapshot().update(m_renderShader);
					getSnapshot().draw();
				}
				m_renderShader.end();
            }
            ofPopMatrix();
        }
    }

	//--------------------------------------------------------------
	void SequenceRamses::drawOctree(float scale)
	{
		if (m_bRender)
		{
			ofSetColor(ofColor::white);

			ofPushMatrix();
			ofScale(scale / m_normalizeFactor, scale / m_normalizeFactor, scale / m_normalizeFactor);
			ofTranslate(m_originShift.x, m_originShift.y, m_originShift.z);
			getSnapshot().drawOctree(m_densityMin * m_densityRange.getSpan(), m_densityMax * m_densityRange.getSpan());
			ofPopMatrix();
		}
	}

	//--------------------------------------------------------------
	void SequenceRamses::drawOctreeDensities(const ofTrueTypeFont & ttf, const ofCamera & camera, float scale)
	{
		if (m_bRender)
		{
			ofSetColor(ofColor::white);
			glm::mat4 model = glm::scale(glm::vec3(scale/m_normalizeFactor));
			model = glm::translate(model, m_originShift);
			getSnapshot().drawOctreeDensities(ttf, camera, model, m_densityMin * m_densityRange.getSpan(), m_densityMax * m_densityRange.getSpan());
		}
	}

	//--------------------------------------------------------------
	void SequenceRamses::drawTexture(float scale){
		auto original_scale = std::max(getSnapshot().getCoordRange().getSpan().x, std::max(getSnapshot().getCoordRange().getSpan().y, getSnapshot().getCoordRange().getSpan().z));
		auto box_scale = std::max(getSnapshot().m_boxRange.getSpan().x, std::max(getSnapshot().m_boxRange.getSpan().y, getSnapshot().m_boxRange.getSpan().z));
		auto ratio = box_scale/original_scale;
		volumetrics.setRenderSettings(1, m_volumeQuality, m_volumeDensity/(ratio*ratio), 0);
		volumetrics.drawVolume(0, 0, 0, scale * ratio, 0);

		/*ofSetColor(ofColor::white);
		ofNoFill();
		ofPushMatrix();
		ofScale(scale / m_normalizeFactor, scale / m_normalizeFactor, scale / m_normalizeFactor);
		ofTranslate(m_originShift.x, m_originShift.y, m_originShift.z);
		ofDrawBox(getSnapshot().m_boxRange.center, getSnapshot().m_boxRange.size.x, getSnapshot().m_boxRange.size.y, getSnapshot().m_boxRange.size.z);
		ofPopMatrix();
		ofFill();*/
	}

	//--------------------------------------------------------------
	ofMesh SequenceRamses::getOctreeMesh(float scale, int minLevel) const{
		auto mesh = getSnapshot().getOctreeMesh(m_densityMin * m_densityRange.getSpan(), m_densityMax * m_densityRange.getSpan(), minLevel);
		glm::mat4 model = glm::scale(glm::vec3(scale/m_normalizeFactor));
		model = glm::translate(model, m_originShift);
		for(auto & v: mesh.getVertices()){
			v = (model * glm::vec4(v,1.0)).xyz();
		}
		return mesh;
	}

	//--------------------------------------------------------------
	void SequenceRamses::preloadAllFrames()
	{

	}

	//--------------------------------------------------------------
	void SequenceRamses::loadFrame(float index, int from, int to)
	{
//		if (0 > index || index >= getTotalFrames())
//		{
//			ofLogError("SequenceRamses::loadFrame") << "Index " << index << " out of range [0, " << getTotalFrames() << "]";
//			return;
//		}

		cout << "load frame " << index << endl;
		auto range = (m_endIndex - m_startIndex);
		if(frameSettings.frameIndex != m_startIndex + (int(from) % range)){
			if(nextFrameSettings.frameIndex == m_startIndex + (int(from) % range)){
				std::swap(frameSettings, nextFrameSettings);
			}else{
				frameSettings.folder = m_folder;
				frameSettings.urlFolder = m_urlFolder;
				frameSettings.frameIndex = m_startIndex + (int(from) % range);
				frameSettings.minDensity = m_densityMin;
				frameSettings.maxDensity = m_densityMax;
				frameSettings.volumeTexture.loadData(
					m_clearData.data(),
					frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, 0,0,0, GL_RED
				);
				m_snapshot.setup(frameSettings);
			}

			m_coordRange.clear();
			m_sizeRange.clear();
			m_densityRange.clear();

			// Adjust the ranges.
			m_coordRange.include(m_snapshot.getCoordRange());
			m_sizeRange.include(m_snapshot.getSizeRange());
			m_densityRange.include(m_snapshot.getDensityRange());

			// Set normalization values to remap to [-0.5, 0.5]
			glm::vec3 coordSpan = m_coordRange.getSpan();
			m_originShift = -0.5f * coordSpan - m_coordRange.getMin();

			m_normalizeFactor = MAX(MAX(coordSpan.x, coordSpan.y), coordSpan.z);
			m_renderShader.begin();
			m_renderShader.setUniform1f("uDensityMin", m_densityMin * m_densityRange.getSpan());
			m_renderShader.setUniform1f("uDensityMax", m_densityMax * m_densityRange.getSpan());
			m_renderShader.end();
			m_volumetricsShader.begin();
			m_volumetricsShader.setUniform1f("minDensity", m_densityMin * m_densityRange.getSpan());
			m_volumetricsShader.setUniform1f("maxDensity", m_densityMax * m_densityRange.getSpan());
			m_volumetricsShader.end();
			cout << "min density " << m_densityMin * m_densityRange.getSpan() <<
					" max density " << m_densityMax * m_densityRange.getSpan() << endl;
			cout << "min coord " << m_coordRange.getMin() <<
					" max coord " << m_coordRange.getMax() << endl;
		}

		if(nextFrameSettings.frameIndex != m_startIndex + (int(to) % range)){
			nextFrameSettings.folder = m_folder;
			nextFrameSettings.urlFolder = m_urlFolder;
			nextFrameSettings.frameIndex = m_startIndex + (int(to) % range);
			nextFrameSettings.minDensity = m_densityMin;
			nextFrameSettings.maxDensity = m_densityMax;
			nextFrameSettings.volumeTexture.loadData(
				m_clearData.data(),
				frameSettings.worldsize, frameSettings.worldsize, frameSettings.worldsize, 0,0,0, GL_RED
			);
			m_snapshot.setup(nextFrameSettings);
		}

		if(m_currentIndex!=index){
			mix3d.begin();
			composite.bindAsImage(0, GL_WRITE_ONLY, 0, false, 0);
			frameSettings.volumeTexture.bindAsImage(1, GL_READ_ONLY, 0, false, 0);
			nextFrameSettings.volumeTexture.bindAsImage(2, GL_READ_ONLY, 0, false, 0);
			mix3d.setUniform1f("uPct", index - from);
			auto size = composite.texData.width;
			mix3d.dispatchCompute(size / 8, size / 8, size / 8);
			cout << "computing " << index << " between " << frameSettings.frameIndex << " and " << nextFrameSettings.frameIndex << endl;
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			mix3d.end();
			glBindImageTexture(0, 0, 0, 0, 0, GL_READ_WRITE, GL_R16F);
			glBindImageTexture(1, 0, 0, 0, 0, GL_READ_WRITE, GL_R16F);
			glBindImageTexture(2, 0, 0, 0, 0, GL_READ_WRITE, GL_R16F);
		}
		m_currentIndex = index;
	}

	//--------------------------------------------------------------
	void SequenceRamses::setFrameRate(float frameRate)
	{
		m_frameRate = frameRate;
	}

	//--------------------------------------------------------------
	float SequenceRamses::getFrameRate() const
	{
		return m_frameRate;
	}

	//--------------------------------------------------------------
	void SequenceRamses::setFrame(float index)
	{
		if (!m_bReady) 
		{
			ofLogError("SequenceRamses::setFrame") << "Not ready, call setup() first!";
			return;
		}

		if (index < 0) 
		{
			ofLogError("SequenceRamses::setFrame") << "Index must be a positive number!";
			return;
		}

//		if(m_currFrame == index){
//			return;
//		}

		//index = ofClamp(index, 0, getTotalFrames());

		loadFrame(index, index, index + 1);
		m_currFrame = index;
	}
	
	//--------------------------------------------------------------
	void SequenceRamses::setFrameForTime(float time)
	{
		return setFrameAtPercent(time / getTotalTime());
	}
	
	//--------------------------------------------------------------
	void SequenceRamses::setFrameAtPercent(float percent)
	{
		setFrame(getFrameIndexAtPercent(percent));
	}

	//--------------------------------------------------------------
	int SequenceRamses::getFrameIndexAtPercent(float percent)
	{
		if (0.0f > percent || percent > 1.0f)
		{
			percent -= floor(percent);
		}

		return MIN((int)(percent * getTotalFrames()), getTotalFrames() - 1);
	}

	//--------------------------------------------------------------
	float SequenceRamses::getPercentAtFrameIndex(int index)
	{
		return ofMap(index, 0, getTotalFrames() - 1, 0.0f, 1.0f, true);
	}

	//--------------------------------------------------------------
	int SequenceRamses::getCurrentFrame() const
	{
		return m_currFrame;
	}

	//--------------------------------------------------------------
	int SequenceRamses::getTotalFrames() const
	{
		return m_endIndex - m_startIndex + 1;
	}

	//--------------------------------------------------------------
	float SequenceRamses::getTotalTime() const
	{
		return getTotalFrames() / m_frameRate;
	}

	//--------------------------------------------------------------
	SnapshotRamses& SequenceRamses::getSnapshot()
	{
		return m_snapshot;
	}

	//--------------------------------------------------------------
	SnapshotRamses& SequenceRamses::getSnapshotForFrame(int index)
	{
		setFrame(index);
		return getSnapshot();
	}

	//--------------------------------------------------------------
	SnapshotRamses& SequenceRamses::getSnapshotForTime(float time)
	{
		setFrameForTime(time);
		return getSnapshot();
	}

	//--------------------------------------------------------------
	SnapshotRamses& SequenceRamses::getSnapshotForPercent(float percent)
	{
		setFrameAtPercent(percent);
		return getSnapshot();
	}

	//--------------------------------------------------------------
	const SnapshotRamses& SequenceRamses::getSnapshot() const
	{
		return m_snapshot;
	}

	//--------------------------------------------------------------
	bool SequenceRamses::isReady() const
	{
		return m_bReady;
	}
}
