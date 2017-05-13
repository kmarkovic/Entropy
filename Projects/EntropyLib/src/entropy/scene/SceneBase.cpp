#include "Base.h"

#include "entropy/Helpers.h"
#include "entropy/util/App.h"

namespace entropy
{
	namespace scene
	{
		//--------------------------------------------------------------
		Base::Base()
			: initialized(false)
			, ready(false)
			, cuesTrack(nullptr)
			, messagesTrack(nullptr)
		{}

		//--------------------------------------------------------------
		Base::~Base()
		{
			this->clear_();
		}

		//--------------------------------------------------------------
		void Base::init_()
		{
			if (this->initialized)
			{
				ofLogError(__FUNCTION__) << "Scene is already initialized!";
				return;
			}

			// Set data path root for scene.
			const auto prevDataPathRoot = ofToDataPath("");
			ofSetDataPathRoot(this->getDataPath());

			auto & parameters = this->getParameters();

			// Create timeline.
			static string timelineDataPath;
			if (timelineDataPath.empty())
			{
				timelineDataPath = ofFilePath::addTrailingSlash(GetSharedDataPath());
				timelineDataPath.append(ofFilePath::addTrailingSlash("ofxTimeline"));
			}
			this->timeline = std::make_shared<ofxTimeline>();
			this->timeline->setName("timeline");
			auto timelineSettings = ofxTimeline::Settings();
			timelineSettings.dataPath = timelineDataPath;
			this->timeline->setup(timelineSettings);
			this->timeline->setSpacebarTogglePlay(false);
			this->timeline->setLoopType(OF_LOOP_NONE);
			this->timeline->setFrameRate(30.0f);
			this->timeline->setDurationInSeconds(16 * 60);
			this->timeline->setAutosave(false);
			this->timeline->setPageName(parameters.getName());

			// Add the cues and messages tracks and listeners.
			this->cuesTrack = this->timeline->addFlags("Cues");
			this->messagesTrack = this->timeline->addFlags("Messages");
			ofAddListener(this->timeline->events().bangFired, this, &Base::timelineBangFired_);
			ofAddListener(GetMessenger()->messageReceivedEvent, this, &Base::messageReceived_);

			// Build the Back and Front cameras.
			this->cameras.emplace(render::Layout::Back, std::make_shared<world::Camera>());
			this->cameras[render::Layout::Back]->setup(render::Layout::Back, this->timeline);

			this->cameras.emplace(render::Layout::Front, std::make_shared<world::Camera>());
			this->cameras[render::Layout::Front]->setup(render::Layout::Front, this->timeline);
			this->cameras[render::Layout::Front]->setParent(this->cameras[render::Layout::Back]);

			// Initialize child class.
			this->init();

			// Force resize.
			for (auto & it : this->cameras)
			{
				auto resizeArgs = ofResizeEventArgs(GetCanvasWidth(it.first), GetCanvasHeight(it.first));
				this->resize_(it.first, resizeArgs);
			}

			// Configure and register parameters.
			this->populateMappings(parameters);

			this->populateMappings(this->cameras[render::Layout::Back]->parameters, world::CameraTimelinePageName);
			this->populateMappings(this->cameras[render::Layout::Front]->parameters, world::CameraTimelinePageName);

			this->boxes[render::Layout::Back].parameters.setName("Box Back");
			this->boxes[render::Layout::Front].parameters.setName("Box Front");
			this->populateMappings(this->boxes[render::Layout::Back].parameters);
			this->populateMappings(this->boxes[render::Layout::Front].parameters);

			this->postEffects[render::Layout::Back].setName("Post Effects Back");
			this->postEffects[render::Layout::Front].setName("Post Effects Front");
			this->populateMappings(this->postEffects[render::Layout::Back], render::PostEffectsTimelinePageName);
			this->populateMappings(this->postEffects[render::Layout::Front], render::PostEffectsTimelinePageName);

			// List presets.
			this->populatePresets();
			this->currPreset.clear();

			// Restore default data path.
			ofSetDataPathRoot(prevDataPathRoot);

			this->initialized = true;
		}

		//--------------------------------------------------------------
		void Base::clear_()
		{
			if (!this->initialized)
			{
				ofLogError(__FUNCTION__) << "Scene is not initialized!";
				return;
			}
			
			this->clear();

			// Clear parameter listeners.
			this->parameterListeners.clear();

			// Clear mappings.
			this->clearMappings();
			this->mappings.clear();

			// Clear cameras.
			for (auto & it : this->cameras)
			{
				it.second->clear();
			}
			this->cameras.clear();

			// Clear any remaining timeline stuff.
			ofRemoveListener(this->timeline->events().bangFired, this, &Base::timelineBangFired_);
			ofRemoveListener(GetMessenger()->messageReceivedEvent, this, &Base::messageReceived_);
			this->timeline->clear();
			this->timeline.reset();
		}

		//--------------------------------------------------------------
		void Base::setup_()
		{
			if (this->ready)
			{
				//ofLogNotice(__FUNCTION__) << "Scene is already set up!";
				return;
			}

			// Reset the timeline.
			this->timeline->setCurrentFrame(0);

			// Reset cameras (but keep transform intact).
			for (auto & it : this->cameras)
			{
				it.second->reset(false);
			}

			// Inherit the camera settings if necessary.
			for (auto & it : this->cameras)
			{
				if (it.second->inheritsSettings)
				{
					const auto & cameraSettings = GetSavedCameraSettings(it.first);
					if (cameraSettings.fov != 0.0f)
					{
						it.second->applySettings(cameraSettings);
					}
				}
			}

			// Setup child Scene.
			this->setup();

			this->ready = true;
		}

		//--------------------------------------------------------------
		void Base::exit_()
		{
			if (!this->ready)
			{
				//ofLogNotice(__FUNCTION__) << "Scene is not set up!";
				return;
			}

			// Stop the timeline.
			this->timeline->stop();

			// Exit child Scene.
			this->exit();

			// Save default preset.
			this->savePreset(kPresetDefaultName);

			// Clear pop-ups.
			while (!this->popUps.empty())
			{
				this->removePopUp();
			}

			this->ready = false;
		}

		//--------------------------------------------------------------
		void Base::resize_(render::Layout layout, ofResizeEventArgs & args)
		{
			this->cameras[layout]->resize(args);

			if (layout == render::Layout::Back)
			{
				this->resizeBack(args);
			}
			else
			{
				this->resizeFront(args);
			}

			for (auto popUp : this->popUps)
			{
				popUp->resize_(args);
			}
		}

		//--------------------------------------------------------------
		void Base::update_(double dt)
		{
			for (auto & it : this->cameras)
			{
				it.second->update(GetApp()->isMouseOverGui());
			}

			for (auto & it : this->mappings)
			{
				for (auto mapping : it.second)
				{
					mapping->update();
				}
			}

			for (auto popUp : this->popUps)
			{
				popUp->update_(dt);
			}

			this->update(dt);
		}

		//--------------------------------------------------------------
		void Base::drawBase_(render::Layout layout)
		{
			ofClear(0, 255);

			if (layout == render::Layout::Back)
			{
				this->drawBackBase();
			}
			else
			{
				this->drawFrontBase();
			}

			for (auto popUp : this->popUps)
			{
				if (popUp->getLayout() == layout && popUp->getSurface() == popup::Surface::Base)
				{
					popUp->draw_();
				}
			}
		}

		//--------------------------------------------------------------
		void Base::drawWorld_(render::Layout layout)
		{
			auto & parameters = this->getParameters();
			
			this->cameras[layout]->begin();
			ofEnableDepthTest();
			{
				if (this->boxes[layout].autoDraw)
				{
					this->boxes[layout].draw();
				}

				if (layout == render::Layout::Back)
				{
					this->drawBackWorld();
				}
				else
				{
					this->drawFrontWorld();
				}
			}
			ofDisableDepthTest();
			this->cameras[layout]->end();
		}

		//--------------------------------------------------------------
		void Base::drawOverlay_(render::Layout layout)
		{
			if (layout == render::Layout::Back)
			{
				this->drawBackOverlay();
			}
			else
			{
				this->drawFrontOverlay();
			}

			for (auto popUp : this->popUps)
			{
				if (popUp->getLayout() == layout && popUp->getSurface() == popup::Surface::Overlay)
				{
					popUp->draw_();
				}
			}
		}

		//--------------------------------------------------------------
		void Base::gui_(ofxImGui::Settings & settings)
		{
			auto & parameters = this->getParameters();

			// Add gui window for Presets.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Presets", settings))
			{
				if (ImGui::Button("Save"))
				{
					this->savePreset(this->currPreset);
				}
				ImGui::SameLine();
				if (ImGui::Button("Save As..."))
				{
					auto name = ofSystemTextBoxDialog("Enter a name for the preset", "");
					if (!name.empty())
					{
						this->savePreset(name);
					}
				}

				ImGui::ListBoxHeader("Load", 3);
				for (auto & name : this->presets)
				{
					if (ImGui::Selectable(name.c_str(), (name == this->currPreset)))
					{
						// Don't load right away, notify Playlist which will take action when ready.
						this->presetCuedEvent.notify(name);
					}
				}
				ImGui::ListBoxFooter();
			}
			ofxImGui::EndWindow(settings);

			// Add gui window for Pop-ups management.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Pop-ups", settings))
			{
				ImGui::ListBoxHeader("List", 3);
				for (auto i = 0; i < this->popUps.size(); ++i)
				{
					auto name = "Pop-up " + ofToString(i);
					ImGui::Checkbox(name.c_str(), &this->popUps[i]->editing);
				}
				ImGui::ListBoxFooter();

				if (ImGui::Button("Add Pop-up..."))
				{
					ImGui::OpenPopup("Pop-ups");
					ImGui::SameLine();
				}
				if (ImGui::BeginPopup("Pop-ups"))
				{
					static vector<string> popUpNames{ "Image", "Video", "Sound" };
					for (auto i = 0; i < popUpNames.size(); ++i)
					{
						if (ImGui::Selectable(popUpNames[i].c_str()))
						{
							if (i == 0)
							{
								this->addPopUp(popup::Type::Image);
							}
							else if (i == 1)
							{
								this->addPopUp(popup::Type::Video);
							}
							else // if (i == 2)
							{
								this->addPopUp(popup::Type::Sound);
							}
						}
					}
					ImGui::EndPopup();
				}

				if (!this->popUps.empty())
				{
					ImGui::SameLine();
					if (ImGui::Button("Remove Pop-up"))
					{
						this->removePopUp();
					}
				}
			}
			ofxImGui::EndWindow(settings);

			// Add individual gui windows for each Pop-up.
			{
				auto popUpSettings = ofxImGui::Settings();
				//popUpSettings.windowPos.x = (settings.totalBounds.getMaxX() + kImGuiMargin);
				popUpSettings.windowPos.x = (800.0f + kImGuiMargin);
				popUpSettings.windowPos.y = 0.0f;
				for (auto i = 0; i < this->popUps.size(); ++i)
				{
					this->popUps[i]->gui_(popUpSettings);
				}
				settings.mouseOverGui |= popUpSettings.mouseOverGui;
			}

			// Add gui window for Mappings.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Mappings", settings))
			{
				for (auto & it : this->mappings)
				{
					if (ofxImGui::BeginTree(it.first, settings))
					{
						for (auto mapping : it.second)
						{
							if (ofxImGui::AddParameter(mapping->animated))
							{
								if (mapping->animated)
								{
									mapping->addTrack(this->timeline);
								}
								else
								{
									mapping->removeTrack(this->timeline);
								}
							}
						}

						ofxImGui::EndTree(settings);
					}
				}
			}
			ofxImGui::EndWindow(settings);

			// Add gui window for Cameras.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Cameras", settings))
			{
				for (auto & it : this->cameras)
				{
					it.second->gui(settings);
				}
			}
			ofxImGui::EndWindow(settings);

			// Add gui window for Boxes.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Boxes", settings))
			{
				for (auto & it : this->boxes)
				{
					if (ofxImGui::BeginTree(it.second.parameters, settings))
					{
						ofxImGui::AddParameter(it.second.enabled);
						if (it.second.enabled)
						{
							ImGui::SameLine();
							ofxImGui::AddParameter(it.second.autoDraw);
						}
						static const vector<string> blendLabels{ "Disabled", "Alpha", "Add", "Subtract", "Multiply", "Screen" };
						ofxImGui::AddRadio(it.second.blendMode, blendLabels, 3);
						ofxImGui::AddParameter(it.second.depthTest);
						static const vector<string> cullLabels{ "None", "Back", "Front" };
						ofxImGui::AddRadio(it.second.cullFace, cullLabels, 3);
						ofxImGui::AddParameter(it.second.color);
						ofxImGui::AddParameter(it.second.alpha);
						ofxImGui::AddParameter(it.second.size);
						ofxImGui::AddParameter(it.second.edgeRatio);
						ofxImGui::AddParameter(it.second.subdivisions); 
						
						ofxImGui::EndTree(settings);
					}
				}
			}
			ofxImGui::EndWindow(settings);

			// Add gui window for Post Effects.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Post Effects", settings))
			{
				for (auto & it : this->postEffects)
				{
					auto & postParameters = it.second;
					if (ofxImGui::BeginTree(postParameters, settings))
					{
						ofxImGui::AddGroup(postParameters.bloom, settings);

						if (ofxImGui::BeginTree(postParameters.color, settings))
						{
							ofxImGui::AddParameter(postParameters.color.exposure);
							ofxImGui::AddParameter(postParameters.color.gamma);
							static vector<string> labels =
							{
								"None",
								"Gamma Only",
								"Reinhard",
								"Reinhard Lum",
								"Filmic",
								"ACES",
								"Uncharted 2"
							};
							ofxImGui::AddRadio(postParameters.color.tonemapping, labels, 3);
							ofxImGui::AddParameter(postParameters.color.brightness);
							ofxImGui::AddParameter(postParameters.color.contrast);
						
							ofxImGui::EndTree(settings);
						}

						ofxImGui::AddGroup(postParameters.vignette, settings);

						ofxImGui::EndTree(settings);
					}
				}
			}
			ofxImGui::EndWindow(settings);

			// Let the child class handle its child parameters.
			this->gui(settings);
		}

		//--------------------------------------------------------------
		void Base::serialize_(nlohmann::json & json)
		{
			ofxPreset::Serializer::Serialize(json, this->getParameters());
			
			// Save Camera settings.
			auto & jsonCameras = json["Cameras"];
			for (auto & it : this->cameras)
			{
				it.second->serialize(jsonCameras);
			}

			// Save Box settings.
			auto & jsonBoxes = json["Boxes"];
			for (auto & it : this->boxes)
			{
				ofxPreset::Serializer::Serialize(jsonBoxes, it.second.parameters);
			}

			// Save PostEffects settings.
			auto & jsonPostEffects = json["Post Effects"];
			for (auto & it : this->postEffects)
			{
				ofxPreset::Serializer::Serialize(jsonPostEffects, it.second);
			}

			// Save Mappings.
			auto & jsonMappings = json["Mappings"];
			for (auto & it : this->mappings)
			{
				for (auto mapping : it.second)
				{
					ofxPreset::Serializer::Serialize(jsonMappings, mapping->animated);
				}
			}

			// Save Pop-ups.
			auto & jsonPopUps = json["Pop-ups"];
			for (auto popUp : this->popUps)
			{
				nlohmann::json jsonPopUp;
				popUp->serialize_(jsonPopUp);
				jsonPopUps.push_back(jsonPopUp);
			}

			// Save child scene settings.
			this->serialize(json);
		}

		//--------------------------------------------------------------
		void Base::deserialize_(const nlohmann::json & json)
		{
			ofxPreset::Serializer::Deserialize(json, this->getParameters());

			// Restore Camera settings.
			if (json.count("Cameras"))
			{
				auto & jsonCameras = json["Cameras"];
				for (auto & it : this->cameras)
				{
					it.second->deserialize(jsonCameras);
				}
			}

			// Restore Box settings.
			if (json.count("Boxes"))
			{
				auto & jsonBoxes = json["Boxes"];
				for (auto & it : this->boxes)
				{
					ofxPreset::Serializer::Deserialize(jsonBoxes, it.second.parameters);
				}
			}

			// Restore PostEffects settings.
			if (json.count("Post Effects"))
			{
				auto & jsonPostEffects = json["Post Effects"];
				for (auto & it : this->postEffects)
				{
					ofxPreset::Serializer::Deserialize(jsonPostEffects, it.second);
				}
			}

			// Clear previous Pop-ups.
			while (!this->popUps.empty())
			{
				this->removePopUp();
			}

			// Add new Pop-ups.
			if (json.count("Pop-ups"))
			{
				for (auto & jsonPopUp : json["Pop-ups"])
				{
					int typeAsInt = jsonPopUp["type"];
					popup::Type type = static_cast<popup::Type>(typeAsInt);

					auto popUp = this->addPopUp(type);
					if (popUp)
					{
						popUp->deserialize_(jsonPopUp);
					}
				}
			}

			this->deserialize(json);

			// Clear previous mappings.
			this->clearMappings();

			// Load new mappings.
			if (json.count("Mappings"))
			{
				auto & jsonGroup = json["Mappings"];
				for (auto & it : this->mappings)
				{
					for (auto mapping : it.second)
					{
						ofxPreset::Serializer::Deserialize(jsonGroup, mapping->animated);
					}
				}
			}
			this->refreshMappings();
		}

		//--------------------------------------------------------------
		bool Base::isInitialized() const
		{
			return this->initialized;
		}

		//--------------------------------------------------------------
		bool Base::isReady() const
		{
			return this->ready;
		}

		//--------------------------------------------------------------
		void Base::setShowtime()
		{
			this->timeline->setCurrentTimeToInPoint();
			this->timeline->setCurrentPage(0);
			this->setCameraLocked(true);
			//this->timeline->play();
		}

		//--------------------------------------------------------------
		void Base::drawTimeline(ofxImGui::Settings & settings)
		{
			// Disable mouse events if it's already been captured.
			if (settings.mouseOverGui)
			{
				this->timeline->disableEvents();
			}
			else
			{
				this->timeline->enableEvents();
			}

			this->timeline->setWidth(settings.screenBounds.getWidth());
			this->timeline->setOffset(glm::vec2(settings.screenBounds.getMinY(), settings.screenBounds.getMaxY() - this->timeline->getHeight()));
			this->timeline->draw();
			settings.mouseOverGui |= this->timeline->getDrawRect().inside(ofGetMouseX(), ofGetMouseY());
		}

		//--------------------------------------------------------------
		int Base::getCurrentTimelineFrame()
		{
			return this->timeline->getCurrentFrame();
		}

		//--------------------------------------------------------------
		bool Base::goToNextTimelineFlag()
		{
			if (this->cuesTrack)
			{
				for (auto keyframe : this->cuesTrack->getKeyframes())
				{
					if (keyframe->time > this->timeline->getCurrentTimeMillis())
					{
						this->timeline->setCurrentTimeMillis(keyframe->time);
						return true;
					}
				}
			}

			// No keyframe found, toggle playback.
			this->timeline->togglePlay();

			return false;
		}

		//--------------------------------------------------------------
		void Base::timelineBangFired_(ofxTLBangEventArgs & args)
		{
			if (args.track == this->messagesTrack)
			{
				GetMessenger()->sendMessage(args.flag);
			}
			else
			{
				static const string kStopFlag = "stop";
				static const string kPlayFlag = "play";
				if (args.flag.compare(0, kStopFlag.size(), kStopFlag) == 0)
				{
					this->timeline->stop();
				}
				else if (args.flag.compare(0, kPlayFlag.size(), kPlayFlag) == 0)
				{
					this->timeline->play();
				}
				else
				{
					// Cascade to child scene.
					this->timelineBangFired(args);
				}
			}
		}

		//--------------------------------------------------------------
		void Base::messageReceived_(ofxOscMessage & message)
		{
			ostringstream oss;
			oss << message.getAddress() << " ";
			for (int i = 0; i < message.getNumArgs(); ++i)
			{
				oss << message.getArgTypeName(i) << ":";
				if (message.getArgType(i) == OFXOSC_TYPE_INT32)
				{
					oss << message.getArgAsInt32(i);
				}
				else if (message.getArgType(i) == OFXOSC_TYPE_FLOAT)
				{
					oss << message.getArgAsFloat(i);
				}
				else if (message.getArgType(i) == OFXOSC_TYPE_STRING)
				{
					oss << message.getArgAsString(i);
				}
				else
				{
					oss << "unknown";
				}
				oss << " ";
			}

			ofLogNotice(__FUNCTION__) << "Received OSC message " << oss.str();

			// Cascade to child scene.
			this->messageReceived(message);
		}

		//--------------------------------------------------------------
		std::filesystem::path Base::getAssetsPath(const string & file)
		{
			if (this->assetsPath.empty())
			{
				auto tokens = ofSplitString(this->getName(), "::", true, true);
				auto assetsPath = GetSharedAssetsPath();
				for (auto & component : tokens)
				{
					assetsPath = assetsPath / component;
				}
				this->assetsPath = assetsPath;
			}
			if (file.empty())
			{
				return this->assetsPath;
			}

			auto filePath = this->assetsPath / file;
			return filePath;
		}

		//--------------------------------------------------------------
		std::filesystem::path Base::getDataPath(const string & file)
		{
			if (this->dataPath.empty())
			{
				auto tokens = ofSplitString(this->getName(), "::", true, true);
				auto dataPath = GetSharedDataPath();
				for (auto & component : tokens)
				{
					dataPath = dataPath / component;
				}
				this->dataPath = dataPath;
			}
			if (file.empty()) 
			{
				return this->dataPath;
			}

			auto filePath = this->dataPath / file;
			return filePath;
		}

		//--------------------------------------------------------------
		std::filesystem::path Base::getPresetPath(const string & preset)
		{
			auto presetPath = this->getDataPath("presets");
			if (!preset.empty())
			{
				presetPath = presetPath / preset;
			}
			return presetPath;
		}

		//--------------------------------------------------------------
		std::filesystem::path Base::getCurrentPresetPath(const string & file)
		{
			auto currentPresetPath = this->getPresetPath(this->currPreset);
			if (file.empty())
			{
				return currentPresetPath;
			}

			return (currentPresetPath / file);
		}

		//--------------------------------------------------------------
		const vector<string> & Base::getPresets() const
		{
			return this->presets;
		}

		//--------------------------------------------------------------
		const string & Base::getCurrentPresetName() const
		{
			return this->currPreset;
		}

		//--------------------------------------------------------------
		bool Base::loadPreset(const string & presetName)
		{
			if (!this->initialized)
			{
				ofLogError(__FUNCTION__) << "Scene not initialized, call init_() first!";
				return false;
			}

			// Set data path root for scene.
			ofSetDataPathRoot(this->getDataPath());

			// Clean up scene.
			this->exit_();

			// Make sure file exists.
			const auto presetPath = this->getPresetPath(presetName);
			auto presetFile = ofFile(presetPath);
			if (presetFile.exists())
			{
				// Load parameters from the preset.
				auto paramsPath = presetPath / "parameters.json";
				auto paramsFile = ofFile(paramsPath);
				if (paramsFile.exists())
				{
					nlohmann::json json;
					paramsFile >> json;

					this->deserialize_(json);
				}

				this->timeline->loadTracksFromFolder(presetPath.string());

				this->currPreset = presetName;
			}
			else
			{
				ofLogWarning(__FUNCTION__) << "File not found at path " << presetPath;
				this->currPreset.clear();
			}

			// Setup scene with the new parameters.
			this->setup_();

			if (this->currPreset.empty())
			{
				return false;
			}
			
			// Notify listeners.
			this->presetLoadedEvent.notify(this->currPreset);

			return true;
		}

		//--------------------------------------------------------------
		bool Base::savePreset(const string & presetName)
		{
			const auto presetPath = this->getPresetPath(presetName);

			auto paramsPath = presetPath / "parameters.json";
			auto paramsFile = ofFile(paramsPath, ofFile::WriteOnly);
			nlohmann::json json;
			this->serialize_(json);
			paramsFile << json.dump(4);

			this->timeline->saveTracksToFolder(presetPath.string());

			// Notify listeners.
			// TODO: This parameter should be presetName but it's volatile
			// and I'm not using it so whatever...
			this->presetSavedEvent.notify(this->currPreset);

			this->populatePresets();

			return true;
		}

		//--------------------------------------------------------------
		void Base::populatePresets()
		{
			auto presetsPath = this->getPresetPath();
			auto presetsDir = ofDirectory(presetsPath);
			presetsDir.listDir();
			presetsDir.sort();

			this->presets.clear();
			for (auto i = 0; i < presetsDir.size(); ++i)
			{
				if (presetsDir.getFile(i).isDirectory())
				{
					this->presets.push_back(presetsDir.getName(i));
				}
			}
		}

		//--------------------------------------------------------------
		void Base::loadTextureImage(const std::string & filePath, ofTexture & texture)
		{
			ofPixels pixels;
			ofLoadImage(pixels, filePath);
			if (!pixels.isAllocated())
			{
				ofLogError(__FUNCTION__) << "Could not load file at path " << filePath;
			}

			bool wasUsingArbTex = ofGetUsingArbTex();
			ofDisableArbTex();
			{
				texture.enableMipmap();
				texture.loadData(pixels);
			}
			if (wasUsingArbTex) ofEnableArbTex();
		}

		//--------------------------------------------------------------
		void Base::populateMappings(const ofParameterGroup & group, const std::string & timelinePageName)
		{
			for (const auto & parameter : group)
			{
				// Group.
				auto parameterGroup = dynamic_pointer_cast<ofParameterGroup>(parameter);
				if (parameterGroup)
				{
					// Recurse through contents.
					this->populateMappings(*parameterGroup);
					continue;
				}

				// Parameter.
				{
					auto parameterFloat = dynamic_pointer_cast<ofParameter<float>>(parameter);
					if (parameterFloat)
					{
						auto mapping = make_shared<util::Mapping<float, ofxTLCurves>>();
						mapping->setup(parameterFloat, timelinePageName);
						this->mappings[mapping->getGroupName()].push_back(mapping);
						continue;
					}

					auto parameterInt = dynamic_pointer_cast<ofParameter<int>>(parameter);
					if (parameterInt)
					{
						auto mapping = make_shared<util::Mapping<int, ofxTLCurves>>();
						mapping->setup(parameterInt, timelinePageName);
						this->mappings[mapping->getGroupName()].push_back(mapping);
						continue;
					}

					auto parameterBool = dynamic_pointer_cast<ofParameter<bool>>(parameter);
					if (parameterBool)
					{
						auto mapping = make_shared<util::Mapping<bool, ofxTLSwitches>>();
						mapping->setup(parameterBool, timelinePageName);
						this->mappings[mapping->getGroupName()].push_back(mapping);
						continue;
					}

					auto parameterColor = dynamic_pointer_cast<ofParameter<ofFloatColor>>(parameter);
					if (parameterColor)
					{
						auto mapping = make_shared<util::Mapping<ofFloatColor, ofxTLColorTrack>>();
						mapping->setup(parameterColor, timelinePageName);
						this->mappings[mapping->getGroupName()].push_back(mapping);
						continue;
					}
				}
			}
		}

		//--------------------------------------------------------------
		void Base::refreshMappings()
		{
			for (auto & it : this->mappings)
			{
				for (auto mapping : it.second)
				{
					if (mapping->animated)
					{
						mapping->addTrack(this->timeline);
					}
					else
					{
						mapping->removeTrack(this->timeline);
					}
				}
			}
		}

		//--------------------------------------------------------------
		void Base::clearMappings()
		{
			for (auto & it : this->mappings)
			{
				for (auto mapping : it.second)
				{
					if (mapping->animated)
					{
						mapping->removeTrack(this->timeline);
						mapping->animated = false;
					}
				}
			}
			//this->mappings.clear();
		}

		//--------------------------------------------------------------
		shared_ptr<popup::Base> Base::addPopUp(popup::Type type)
		{
			shared_ptr<popup::Base> popUp;
			if (type == popup::Type::Image)
			{
				popUp = make_shared<popup::Image>();
			}
			else if (type == popup::Type::Video)
			{
				popUp = make_shared<popup::Video>();
			}
			else if (type == popup::Type::Sound)
			{
				popUp = make_shared<popup::Sound>();
			}
			else
			{
				ofLogError(__FUNCTION__) << "Unrecognized pop-up type " << static_cast<int>(type);
				return nullptr;
			}

			auto idx = this->popUps.size();
			popUp->init_(idx, this->timeline);
			this->popUps.push_back(popUp);

			return popUp;
		}

		//--------------------------------------------------------------
		void Base::removePopUp()
		{
			auto popUp = this->popUps.back();
			popUp->clear_();
			this->popUps.pop_back();
		}

		//--------------------------------------------------------------
		std::shared_ptr<world::Camera> Base::getCamera(render::Layout layout)
		{
			return this->cameras[layout];
		}

		//--------------------------------------------------------------
		void Base::setCameraControlArea(render::Layout layout, const ofRectangle & controlArea)
		{
			this->cameras[layout]->setControlArea(controlArea);
		}

		//--------------------------------------------------------------
		void Base::setCameraLocked(bool cameraLocked)
		{
			for (auto & it : this->cameras)
			{
				it.second->setLockedToTrack(cameraLocked);
			}
		}

		//--------------------------------------------------------------
		void Base::toggleCameraLocked()
		{
			// Back camera takes the lead.
			const auto backLocked = this->isCameraLocked(render::Layout::Back);
			this->setCameraLocked(!backLocked);
		}

		//--------------------------------------------------------------
		bool Base::isCameraLocked(render::Layout layout) const
		{
			return this->cameras.at(layout)->isLockedToTrack();
		}

		//--------------------------------------------------------------
		void Base::addCameraKeyframe(render::Layout layout)
		{
			this->cameras[layout]->addKeyframe();
		}

		//--------------------------------------------------------------
		render::PostParameters & Base::getPostParameters(render::Layout layout)
		{
			return this->postEffects[layout];
		}

		//--------------------------------------------------------------
		void Base::beginExport()
		{
			this->timeline->setFrameBased(true);
			this->setCameraLocked(true);
			this->timeline->setCurrentTimeToInPoint();
			this->timeline->play();
		}

		//--------------------------------------------------------------
		void Base::endExport()
		{
			this->timeline->stop();
			this->timeline->setFrameBased(false);
			this->setCameraLocked(false);
		}
	}
}
