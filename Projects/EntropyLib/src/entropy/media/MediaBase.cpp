#include "Base.h"

#include "entropy/Helpers.h"
#include "entropy/util/App.h"

#include "ofxEasing.h"
#include "ofxTimeline.h"

namespace entropy
{
	namespace media
	{
		//--------------------------------------------------------------
		Base::Base(Type type)
			: type(type)
			, editing(false)
			, boundsDirty(false)
			, borderDirty(false)
			, transitionPct(0.0f)
			, switchMillis(-1.0f)
			, switchesTrack(nullptr)
		{}

		//--------------------------------------------------------------
		Base::~Base()
		{
			this->clear_();
		}

		//--------------------------------------------------------------
		Type Base::getType() const
		{
			return this->type;
		}

		//--------------------------------------------------------------
		std::string Base::getTypeName() const
		{
			switch (this->type)
			{
			case Type::Image:
				return "Image";
			case Type::Movie:
				return "Movie";
			case Type::HPV:
				return "HPV";
			case Type::Sequence:
				return "Sequence";
			case Type::Sound:
				return "Sound";
			default:
				return "Unknown";
			}
		}

		//--------------------------------------------------------------
		render::Layout Base::getLayout()
		{
			return static_cast<render::Layout>(this->getParameters().base.layout.get());
		}

		//--------------------------------------------------------------
		Surface Base::getSurface()
		{
			return static_cast<Surface>(this->getParameters().base.surface.get());
		}

		//--------------------------------------------------------------
		HorzAlign Base::getHorzAlign()
		{
			return static_cast<HorzAlign>(this->getParameters().base.alignHorz.get());
		}

		//--------------------------------------------------------------
		VertAlign Base::getVertAlign()
		{
			return static_cast<VertAlign>(this->getParameters().base.alignVert.get());
		}

		//--------------------------------------------------------------
		void Base::init_(int index, std::shared_ptr<ofxTimeline> timeline)
		{
			this->index = index;
			this->timeline = timeline;

			this->addTimelineTrack();

			auto & parameters = this->getParameters();
			this->parameterListeners.push_back(parameters.base.layout.newListener([this](int &)
			{
				this->boundsDirty = true;
			}));
			this->parameterListeners.push_back(parameters.base.size.newListener([this](float &)
			{
				this->boundsDirty = true;
			}));
			this->parameterListeners.push_back(parameters.base.anchor.newListener([this](glm::vec2 &)
			{
				this->boundsDirty = true;
			}));
			this->parameterListeners.push_back(parameters.base.alignHorz.newListener([this](int &)
			{
				this->boundsDirty = true;
			}));
			this->parameterListeners.push_back(parameters.base.alignVert.newListener([this](int &)
			{
				this->boundsDirty = true;
			}));
			this->parameterListeners.push_back(parameters.border.width.newListener([this](float &)
			{
				this->borderDirty = true;
			}));

			this->init();
		}

		//--------------------------------------------------------------
		void Base::clear_()
		{
			this->clear();

			this->parameterListeners.clear();

			this->removeTimelineTrack();
			this->timeline.reset();
		}

		//--------------------------------------------------------------
		void Base::setup_()
		{
			this->setup();

			this->boundsDirty = true;
		}

		//--------------------------------------------------------------
		void Base::exit_()
		{
			this->exit();
		}

		//--------------------------------------------------------------
		void Base::resize_(ofResizeEventArgs & args)
		{
			// Update right away so that event listeners can use the new bounds.
			this->updateBounds();
			
			this->resize(args);
		}

		//--------------------------------------------------------------
		void Base::update_(double dt)
		{
			if (this->boundsDirty)
			{
				this->updateBounds();
			}

			this->switchMillis = -1.0f;
			this->enabled = this->switchesTrack->isOn();
			if (this->enabled)
			{
				// Update the transition if any switch is currently active.
				auto trackTime = this->switchesTrack->currentTrackTime();
				auto activeSwitch = this->switchesTrack->getActiveSwitchAtMillis(trackTime);
				if (activeSwitch)
				{
					this->switchMillis = trackTime - activeSwitch->timeRange.min;

					auto kEasingFunction = ofxeasing::quad::easeIn;
					long transitionDuration = this->getParameters().transition.duration * 1000; 
					if (trackTime - activeSwitch->timeRange.min < transitionDuration)
					{
						// Transitioning in.
						this->transitionPct = ofxeasing::map_clamp(trackTime, activeSwitch->timeRange.min, activeSwitch->timeRange.min + transitionDuration, 0.0f, 1.0f ,kEasingFunction);
						this->borderDirty = true;
					}
					else if (activeSwitch->timeRange.max - trackTime < transitionDuration)
					{
						// Transitioning out.
						this->transitionPct = ofxeasing::map_clamp(trackTime, activeSwitch->timeRange.max - transitionDuration, activeSwitch->timeRange.max, 1.0f, 0.0f, kEasingFunction);
						this->borderDirty = true;
					}
					else if (this->transitionPct != 1.0f)
					{
						this->transitionPct = 1.0f;
						this->borderDirty = true;
					}
				}
			}

			if (this->enabled || this->editing)
			{
				auto & parameters = this->getParameters();
				auto transition = static_cast<Transition>(parameters.transition.type.get());
				
				// Set front color value.
				if (this->enabled)
				{
					if (transition == Transition::Mix)
					{
						this->frontAlpha = this->transitionPct;
					}
					else if (transition == Transition::Strobe)
					{
						this->frontAlpha = (ofRandomuf() < this->transitionPct) ? parameters.base.fade : 0.0f;
					}
					else
					{
						this->frontAlpha = parameters.base.fade;
					}
				}
				else
				{
					this->frontAlpha = 0.5f;
				}

				// Set subsection bounds.
				this->dstBounds = this->viewport;
				this->srcBounds = this->roi;
				if (this->enabled && transition == Transition::Wipe && this->transitionPct < 1.0f)
				{
					this->dstBounds.height = this->transitionPct * this->viewport.height;
					this->dstBounds.y = this->viewport.y + (1.0f - this->transitionPct) * this->viewport.height * 0.5f;

					this->srcBounds.height = this->transitionPct * this->roi.height;
					this->srcBounds.y = this->roi.y + (1.0f - this->transitionPct) * this->roi.height * 0.5f;
				}

				if (this->borderDirty)
				{
					this->updateBorder();
				}
			}

			this->update(dt);
		}

		//--------------------------------------------------------------
		void Base::draw_()
		{
			auto & parameters = this->getParameters();

			ofPushStyle();
			{
				if (this->isLoaded() && (this->enabled || this->editing))
				{
					// Draw the background.
					if (parameters.base.background->a > 0)
					{
						ofSetColor(parameters.base.background.get());
						ofDrawRectangle(this->dstBounds);
					}

					// Draw the border.
					if (parameters.border.width > 0.0f)
					{
						ofSetColor(parameters.border.color.get(), this->frontAlpha * 255);
						this->borderMesh.draw();
					}

					// Draw the content.
					ofEnableBlendMode(OF_BLENDMODE_ADD);
					//ofSetColor(255, this->frontAlpha * 255);
					ofSetColor(255 * this->frontAlpha);
					this->renderContent();
					ofEnableBlendMode(OF_BLENDMODE_ALPHA);
				}
			}
			ofPopStyle();

			this->draw();
		}

		//--------------------------------------------------------------
		void Base::gui_(ofxImGui::Settings & settings)
		{
			if (!this->editing) return;

			auto & parameters = this->getParameters();

			// Add a GUI window for the parameters.
			ofxImGui::SetNextWindow(settings);
			if (ofxImGui::BeginWindow("Media " + ofToString(this->index) + ": " + parameters.getName(), settings, false, &this->editing))
			{
				// Add sections for the base parameters.
				if (ofxImGui::BeginTree(parameters.base, settings))
				{
					ofxImGui::AddParameter(parameters.base.background);
					ofxImGui::AddParameter(parameters.base.fade);
					static std::vector<std::string> layoutLabels{ "Back", "Front" };
					ofxImGui::AddRadio(parameters.base.layout, layoutLabels, 2);
					static std::vector<std::string> surfaceLabels{ "Base", "Overlay" };
					ofxImGui::AddRadio(parameters.base.surface, surfaceLabels, 2);
					ofxImGui::AddParameter(parameters.base.size);
					ofxImGui::AddParameter(parameters.base.anchor);
					static std::vector<std::string> horzLabels{ "Left", "Center", "Right" };
					ofxImGui::AddRadio(parameters.base.alignHorz, horzLabels, 3);
					static std::vector<std::string> vertLabels{ "Top", "Middle", "Bottom" };
					ofxImGui::AddRadio(parameters.base.alignVert, vertLabels, 3);

					ofxImGui::EndTree(settings);
				}

				if (ofxImGui::BeginTree(parameters.border, settings))
				{
					ofxImGui::AddParameter(parameters.border.width);
					ofxImGui::AddParameter(parameters.border.color);

					ofxImGui::EndTree(settings);
				}

				if (ofxImGui::BeginTree(parameters.transition, settings))
				{
					static vector<string> labels{ "Cut", "Mix", "Wipe", "Strobe" };
					ofxImGui::AddRadio(parameters.transition.type, labels, 2);

					if (static_cast<Transition>(parameters.transition.type.get()) != Transition::Cut)
					{
						ofxImGui::AddParameter(parameters.transition.duration);
					}

					ofxImGui::EndTree(settings);
				}

				// Let the child class handle its child parameters.
				this->gui(settings);
			}
			ofxImGui::EndWindow(settings);
		}

		//--------------------------------------------------------------
		void Base::serialize_(nlohmann::json & json)
		{
			json["type"] = static_cast<int>(this->type);

			ofxPreset::Serializer::Serialize(json, this->getParameters());

			this->serialize(json);
		}

		//--------------------------------------------------------------
		void Base::deserialize_(const nlohmann::json & json)
		{
			ofxPreset::Serializer::Deserialize(json, this->getParameters());

			this->deserialize(json);

			this->boundsDirty = true;
		}

		//--------------------------------------------------------------
		void Base::addTimelineTrack()
		{
			if (!this->timeline)
			{
				ofLogError(__FUNCTION__) << "No timeline set, call init_() first!";
				return;
			}

			if (this->switchesTrack)
			{
				ofLogWarning(__FUNCTION__) << "Switches track already exists.";
				return;
			}
			
			// Add Page if it doesn't already exist.
			if (!this->timeline->hasPage(MediaTimelinePageName))
			{
				this->timeline->addPage(MediaTimelinePageName);
			}
			auto page = this->timeline->getPage(MediaTimelinePageName);

			const auto trackName = "Media_" + ofToString(this->index) + "_" + this->getTypeName();
			if (page->getTrack(trackName))
			{
				//ofLogWarning(__FUNCTION__) << "Track for Pop-up " << this->index << " already exists!";
				return;
			}

			this->timeline->setCurrentPage(MediaTimelinePageName);

			// Add Track.
			this->switchesTrack = this->timeline->addSwitches(trackName);
		}

		//--------------------------------------------------------------
		void Base::removeTimelineTrack()
		{
			if (!this->timeline)
			{
				//ofLogWarning(__FUNCTION__) << "No timeline set.";
				return;
			}

			if (!this->switchesTrack)
			{
				//ofLogWarning(__FUNCTION__) << "Switches track for Pop-up " << this->index << " does not exist.";
				return;
			}

			this->timeline->removeTrack(this->switchesTrack);
			this->switchesTrack = nullptr;

			// TODO: Figure out why this is not working!
			//auto page = this->timeline->getPage(kTimelinePageName);
			//if (page && page->getTracks().empty())
			//{
			//	cout << "Removing page " << page->getName() << endl;
			//	this->timeline->removePage(page);
			//}
		}

		//--------------------------------------------------------------
		void Base::updateBounds()
		{
			auto & parameters = this->getParameters();

			const auto layout = this->getLayout();
			const auto canvasSize = glm::vec2(GetCanvasWidth(layout), GetCanvasHeight(layout));
			const auto viewportAnchor = canvasSize * parameters.base.anchor.get();
			const auto viewportHeight = canvasSize.y * parameters.base.size;
			if (this->isLoaded())
			{
				const auto contentRatio = this->getContentWidth() / this->getContentHeight();
				const auto viewportWidth = viewportHeight * contentRatio;
				this->viewport.setFromCenter(viewportAnchor, viewportWidth, viewportHeight);

				// Calculate the source subsection for Aspect Fill.
				const auto viewportRatio = this->viewport.getAspectRatio();
				if (this->viewport.getAspectRatio() > contentRatio)
				{
					this->roi.width = this->getContentWidth();
					this->roi.height = this->roi.width / viewportRatio;
					this->roi.x = 0.0f;
					this->roi.y = (this->getContentHeight() - this->roi.height) * 0.5f;
				}
				else
				{
					this->roi.height = this->getContentHeight();
					this->roi.width = this->roi.height * viewportRatio;
					this->roi.y = 0.0f;
					this->roi.x = (this->getContentWidth() - this->roi.width) * 0.5f;
				}
			}
			else
			{
				this->viewport.setFromCenter(viewportAnchor, viewportHeight, viewportHeight);
			}

			// Adjust anchor alignment.
			if (this->getHorzAlign() == HorzAlign::Left)
			{
				this->viewport.x += this->viewport.width * 0.5f;
			}
			else if (this->getHorzAlign() == HorzAlign::Right)
			{
				this->viewport.x -= this->viewport.width * 0.5f;
			}
			if (this->getVertAlign() == VertAlign::Top)
			{
				this->viewport.y += this->viewport.height * 0.5f;
			}
			else if (this->getVertAlign() == VertAlign::Bottom)
			{
				this->viewport.y -= this->viewport.height * 0.5f;
			}

			this->borderDirty = true;

			this->boundsDirty = false;
		}

		//--------------------------------------------------------------
		void Base::updateBorder()
		{
			this->borderMesh.clear();
			
			float borderWidth = this->getParameters().border.width;
			if (borderWidth > 0.0f)
			{
				// Rebuild the border mesh.
				this->borderMesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

				const auto borderBounds = ofRectangle(dstBounds.x - borderWidth, dstBounds.y - borderWidth, dstBounds.width + 2.0f * borderWidth, dstBounds.height + 2.0f * borderWidth);
				this->borderMesh.addVertex(glm::vec3(borderBounds.getMinX(), borderBounds.getMinY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(this->dstBounds.getMinX(), this->dstBounds.getMinY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(borderBounds.getMaxX(), borderBounds.getMinY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(this->dstBounds.getMaxX(), this->dstBounds.getMinY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(borderBounds.getMaxX(), borderBounds.getMaxY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(this->dstBounds.getMaxX(), this->dstBounds.getMaxY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(borderBounds.getMinX(), borderBounds.getMaxY(), 0.0f));
				this->borderMesh.addVertex(glm::vec3(this->dstBounds.getMinX(), this->dstBounds.getMaxY(), 0.0f));

				this->borderMesh.addIndex(0);
				this->borderMesh.addIndex(1);
				this->borderMesh.addIndex(2);
				this->borderMesh.addIndex(3);
				this->borderMesh.addIndex(4);
				this->borderMesh.addIndex(5);
				this->borderMesh.addIndex(6);
				this->borderMesh.addIndex(7);
				this->borderMesh.addIndex(0);
				this->borderMesh.addIndex(1);
			}

			this->borderDirty = false;
		}
	}
}
