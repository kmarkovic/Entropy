#include "Camera.h"

#include "entropy/Helpers.h"
#include "entropy/util/App.h"
#include "ofxEasing.h"

namespace entropy
{
	namespace world
	{
		//--------------------------------------------------------------
		Camera::Settings::Settings()
			: fov(0.0f)
			, nearClip(0.0f)
			, farClip(0.0f)
		{}

		//--------------------------------------------------------------
		Camera::Camera()
			: cameraTrack(nullptr)
		{
			// Set parameter listeners.
			this->parameterListeners.push_back(fov.newListener([this](float & value)
			{
				this->easyCam.setFov(value);
			}));
			this->parameterListeners.push_back(nearClip.newListener([this](float & value)
			{
				this->easyCam.setNearClip(value);
			}));
			this->parameterListeners.push_back(farClip.newListener([this](float & value)
			{
				this->easyCam.setFarClip(value);
			})); 
			
			this->parameterListeners.push_back(attachToParent.newListener([this](bool & enabled)
			{
				this->setAttachedToParent(enabled);
			}));
			this->parameterListeners.push_back(mouseControl.newListener([this](bool & enabled)
			{
				if (enabled)
				{
					this->attachToParent = false;
				}
				this->setMouseInputEnabled(enabled);
			}));
			this->parameterListeners.push_back(relativeYAxis.newListener([this](bool & enabled)
			{
				this->easyCam.setRelativeYAxis(enabled);
			}));
			this->parameterListeners.push_back(tumbleOverride.newListener([this](bool & enabled)
			{
				if (enabled)
				{
					this->beginTumbleOverride();
				}
			}));
		}
		
		//--------------------------------------------------------------
		Camera::~Camera()
		{
			this->clear();
		}

		//--------------------------------------------------------------
		void Camera::setup(render::Layout layout, std::shared_ptr<ofxTimeline> timeline)
		{
			this->clear();

			this->layout = layout;
			const auto name = (this->layout == render::Layout::Back ? "Camera Back" : "Camera Front");
			this->parameters.setName(name);

			this->timeline = timeline;
			this->addTimelineTrack();

			// Make sure the ofEasyCam is up to date.
			this->easyCam.begin();
			this->easyCam.end();
		}
		
		//--------------------------------------------------------------
		void Camera::clear()
		{
			this->parent.reset();

			this->removeTimelineTrack();
			this->timeline.reset();
		}

		//--------------------------------------------------------------
		void Camera::reset(bool transform)
		{
			this->easyCam.setAspectRatio(GetCanvasWidth(this->layout) / GetCanvasHeight(this->layout));
			if (transform)
			{
				this->easyCam.reset();
			}
			this->tumbleOffset = glm::vec3(0.0f);
			this->dollyOffset = 0.0f;
		}

		//--------------------------------------------------------------
		void Camera::beginTumbleOverride()
		{
			//this->tumbleOffset.x = ofWrapDegrees(this->tumbleOffset.x, 0, 360);
			//this->tumbleOffset.y = ofWrapDegrees(this->tumbleOffset.y, 0, 360);
			//this->tumbleOffset.z = ofWrapDegrees(this->tumbleOffset.z, 0, 360);
			this->overrideParams.tumble = this->tumbleOffset;
			this->overrideParams.origin = this->tumbleOffset;
			this->overrideParams.target = this->tumbleOffset;
			/*this->overrideParams.target.x = this->tumbleOffset.x + ofAngleDifferenceDegrees(360, this->tumbleOffset.x);
			this->overrideParams.target.y = this->tumbleOffset.y + ofAngleDifferenceDegrees(360, this->tumbleOffset.y);
			this->overrideParams.target.z = this->tumbleOffset.z + ofAngleDifferenceDegrees(360, this->tumbleOffset.z);*/

			//cout << this->tumbleOffset << " to " << this->overrideParams.target << endl;
			auto iTumbleOrigin = glm::ivec3(static_cast<int>(this->tumbleOffset.x) / 360,
				static_cast<int>(this->tumbleOffset.y) / 360,
				static_cast<int>(this->tumbleOffset.z) / 360);
			//if (this->tumbleOffset.x != 0.0f) this->overrideParams.target.x = 360 * (iTumbleOrigin.x + 1);
			if (this->tumbleOffset.y != 0.0f) this->overrideParams.target.y = 360 * (iTumbleOrigin.y + 1);
			//if (this->tumbleOffset.z != 0.0f) this->overrideParams.target.z = 360 * (iTumbleOrigin.z + 1);
			//this->overrideParams.target = glm::vec3(iTumbleTarget);
			this->overrideParams.startTime = ofGetElapsedTimef();
			this->overrideParams.totalDuration = 20.0f;


			//cout << "Tumble num turns " << iTumbleOrigin.x << " " << iTumbleOrigin.y << " " << iTumbleOrigin.z << endl;
			//cout << "Tumble override from " << this->overrideParams.origin << " to " << this->overrideParams.target << endl;
		}

		//--------------------------------------------------------------
		void Camera::update(bool mouseOverGui)
		{
			// TODO: Figure out a better way to do this, with event consumption or something.
			if (mouseOverGui || !this->mouseControl)
			{
				this->easyCam.disableMouseInput();
			}
			else
			{
				this->easyCam.enableMouseInput();
			}

			if (!this->attachToParent)// && !this->easyCam.isMoving())
			{
				if (this->tumbleOverride)
				{
					//// Stop when we circle back to 0.
					//auto iTumbleOffset = glm::ivec3(static_cast<int>(this->tumbleOffset.x) % 360,
					//								static_cast<int>(this->tumbleOffset.y) % 360, 
					//								static_cast<int>(this->tumbleOffset.z) % 360);
					//auto iTumbleTarget = glm::ivec3(static_cast<int>(this->tumbleOffset.x + this->tiltSpeed) % 360,
					//								static_cast<int>(this->tumbleOffset.y + this->panSpeed) % 360,
					//								static_cast<int>(this->tumbleOffset.z + this->rollSpeed) % 360);
					//if (iTumbleOffset.x == 0 || iTumbleTarget.x < iTumbleOffset.x)
					//{
					//	this->tumbleOffset.x = 0.0f;
					//}
					//else
					//{
					//	this->tumbleOffset.x += this->tiltSpeed;
					//}
					//if (iTumbleOffset.y == 0 || iTumbleTarget.y < iTumbleOffset.y)
					//{
					//	this->tumbleOffset.y = 0.0f;
					//}
					//else
					//{
					//	this->tumbleOffset.y += this->panSpeed;
					//}
					//if (iTumbleOffset.z == 0 || iTumbleTarget.z < iTumbleOffset.z)
					//{
					//	this->tumbleOffset.z = 0.0f;
					//}
					//else
					//{
					//	this->tumbleOffset.z += this->rollSpeed;
					//}

					// Get the current tween pct.
					float currTime = ofGetElapsedTimef();
					float pct = (currTime - this->overrideParams.startTime) / this->overrideParams.totalDuration;
					if (pct <= 1.0f)
					{
						glm::vec3 tweenPos;
						auto easing = ofxeasing::quint::easeInOut;
						//tweenPos.x = ofxeasing::map_clamp(pct, 0.0f, 1.0f, this->overrideParams.origin.x, this->overrideParams.target.x, easing);
						tweenPos.y = ofxeasing::map_clamp(pct, 0.0f, 1.0f, this->overrideParams.origin.y, this->overrideParams.target.y, easing);
						//tweenPos.z = ofxeasing::map_clamp(pct, 0.0f, 1.0f, this->overrideParams.origin.z, this->overrideParams.target.z, easing);
					
						this->overrideParams.tumble += glm::vec3(this->tiltSpeed, this->panSpeed, this->rollSpeed);
						float otherPct = ofMap(pct, 0.0, 0.2, 0.0, 1.0);
						if (otherPct < 1.0)
						{
							this->tumbleOffset = this->overrideParams.tumble * (1.0f - otherPct) + tweenPos * otherPct;
						}
						else
						{
							this->tumbleOffset = tweenPos;
						}
					}
					else
					{
						this->tumbleOffset.y = this->overrideParams.target.y;
					}
				}
				else
				{
					this->tumbleOffset.x += this->tiltSpeed;
					this->tumbleOffset.y += this->panSpeed;
					this->tumbleOffset.z += this->rollSpeed;
				}

				this->dollyOffset += this->dollySpeed;
			}
		}

		//--------------------------------------------------------------
		void Camera::resize(ofResizeEventArgs & args)
		{
			this->easyCam.setAspectRatio(args.width / static_cast<float>(args.height));
		}

		//--------------------------------------------------------------
		void Camera::begin()
		{
			this->easyCam.begin(GetCanvasViewport(this->layout));
			
			ofPushMatrix();
			
			if (this->attachToParent)
			{
				ofMultMatrix(this->parent->getTransform());
			}
			else
			{
				ofMultMatrix(this->getTransform());
			}
		}

		//--------------------------------------------------------------
		void Camera::end()
		{
			ofPopMatrix();

			this->easyCam.end();
		}

		//--------------------------------------------------------------
		void Camera::applySettings(const Camera::Settings & settings)
		{
			this->fov = settings.fov;
			this->nearClip = settings.nearClip;
			this->farClip = settings.farClip;
			this->easyCam.setPosition(settings.position);
			this->easyCam.setOrientation(settings.orientation);

			if (this->hasTimelineTrack())
			{
				// Find the first keyframe and force its transform.
				auto & keyframes = this->cameraTrack->getKeyframes();
				if (!keyframes.empty())
				{
					auto keyframe = static_cast<ofxTLCameraFrame *>(keyframes.front());
					keyframe->position = this->easyCam.getPosition();
					keyframe->orientation = this->easyCam.getOrientationQuat();
				}
			}
		}
		
		//--------------------------------------------------------------
		Camera::Settings Camera::fetchSettings()
		{
			auto settings = Camera::Settings();
			settings.fov = this->fov;
			settings.nearClip = this->nearClip;
			settings.farClip = this->farClip;
			settings.position = this->easyCam.getPosition();
			settings.orientation = this->easyCam.getOrientationQuat();
			return settings;
		}

		//--------------------------------------------------------------
		ofEasyCam & Camera::getEasyCam()
		{
			return this->easyCam;
		}

		//--------------------------------------------------------------
		glm::mat4 Camera::getTransform() const
		{
			static const auto xAxis = glm::vec3(1.0f, 0.0f, 0.0f);
			static const auto yAxis = glm::vec3(0.0f, 1.0f, 0.0f);
			static const auto zAxis = glm::vec3(0.0f, 0.0f, 1.0f);

			glm::mat4 transform;

			// Dolly.
			transform = glm::translate(transform, this->easyCam.getZAxis() * this->dollyOffset);

			// Tumble.
			transform = glm::rotate(transform, ofDegToRad(this->tumbleOffset.x), xAxis);
			transform = glm::rotate(transform, ofDegToRad(this->tumbleOffset.y), yAxis);
			transform = glm::rotate(transform, ofDegToRad(this->tumbleOffset.z), zAxis);

			return transform;
		}

		//--------------------------------------------------------------
		void Camera::setControlArea(const ofRectangle & controlArea)
		{
			this->easyCam.setControlArea(controlArea);
		}
		
		//--------------------------------------------------------------
		void Camera::setMouseInputEnabled(bool mouseInputEnabled)
		{
			if (mouseInputEnabled)
			{
				this->easyCam.enableMouseInput();
			}
			else
			{
				this->easyCam.disableMouseInput();
			}
		}

		//--------------------------------------------------------------
		void Camera::setDistanceToTarget(float distanceToTarget)
		{
			this->easyCam.setDistance(distanceToTarget);
		}
		
		//--------------------------------------------------------------
		float Camera::getDistanceToTarget() const
		{
			return this->easyCam.getDistance();
		}

		//--------------------------------------------------------------
		void Camera::setParent(std::shared_ptr<Camera> parent)
		{
			this->parent = parent;
		}

		//--------------------------------------------------------------
		void Camera::clearParent()
		{
			this->parent.reset();
		}

		//--------------------------------------------------------------
		bool Camera::hasParent() const
		{
			return (this->parent != nullptr);
		}

		//--------------------------------------------------------------
		void Camera::setAttachedToParent(bool attachedToParent)
		{
			if (attachedToParent && this->parent)
			{
				this->mouseControl = false;
				this->removeTimelineTrack();
				this->easyCam.setParent(this->parent->getEasyCam(), true);
			}
			else
			{
				this->addTimelineTrack();
				this->easyCam.clearParent(true);
			}
		}

		//--------------------------------------------------------------
		bool Camera::isAttachedToParent() const
		{
			return (this->easyCam.getParent() != nullptr);
		}

		//--------------------------------------------------------------
		void Camera::copyTransformFromParent()
		{
			if (!this->hasParent())
			{
				ofLogError(__FUNCTION__) << "No parent node!";
				return;
			}

			const bool wasAttached = this->isAttachedToParent();
			if (wasAttached)
			{
				this->setAttachedToParent(false);
			}
			this->easyCam.setPosition(this->parent->getEasyCam().getPosition());
			this->easyCam.setOrientation(this->parent->getEasyCam().getOrientationQuat());
			this->easyCam.setScale(this->parent->getEasyCam().getScale());
			if (wasAttached)
			{
				this->setAttachedToParent(true);
			}
		}

		//--------------------------------------------------------------
		void Camera::addTimelineTrack()
		{
			if (!this->timeline)
			{
				ofLogError(__FUNCTION__) << "No timeline set, call setup() first!";
				return;
			}

			if (this->cameraTrack)
			{
				//ofLogWarning(__FUNCTION__) << "Camera track already exists.";
				return;
			}

			// Add Page if it doesn't already exist.
			if (!this->timeline->hasPage(CameraTimelinePageName))
			{
				this->timeline->addPage(CameraTimelinePageName);
			}
			this->timeline->setCurrentPage(CameraTimelinePageName);

			const auto trackName = this->parameters.getName();

			this->cameraTrack = new ofxTLCameraTrack();
			this->cameraTrack->setDampening(1.0f);
			this->cameraTrack->setCamera(this->easyCam);
			this->cameraTrack->setXMLFileName(this->timeline->nameToXMLName(trackName));
			this->timeline->addTrack(trackName, this->cameraTrack);
			this->cameraTrack->setDisplayName(trackName);
			this->cameraTrack->lockCameraToTrack = false;
		}

		//--------------------------------------------------------------
		void Camera::removeTimelineTrack()
		{
			if (!this->timeline)
			{
				//ofLogWarning(__FUNCTION__) << "No timeline set.";
				return;
			}

			if (!this->cameraTrack)
			{
				//ofLogWarning(__FUNCTION__) << "Camera track does not exist.";
				return;
			}

			this->timeline->removeTrack(this->cameraTrack);
			delete this->cameraTrack;
			this->cameraTrack = nullptr;
		}

		//--------------------------------------------------------------
		bool Camera::hasTimelineTrack() const
		{
			return (this->cameraTrack != nullptr);
		}

		//--------------------------------------------------------------
		void Camera::setLockedToTrack(bool lockedToTrack)
		{
			if (!this->hasTimelineTrack()) return;

			if (this->attachToParent)
			{
				// Don't lock when attached.
				this->cameraTrack->lockCameraToTrack = false;
			}
			else
			{
				this->cameraTrack->lockCameraToTrack = lockedToTrack;
			}
		}
		
		//--------------------------------------------------------------
		bool Camera::isLockedToTrack() const
		{
			if (!this->hasTimelineTrack()) return false;
			
			return this->cameraTrack->lockCameraToTrack;
		}

		//--------------------------------------------------------------
		void Camera::addKeyframe()
		{
			this->cameraTrack->addKeyframe();
		}

		//--------------------------------------------------------------
		bool Camera::gui(ofxImGui::Settings & settings)
		{
			if (ofxImGui::BeginTree(this->parameters, settings))
			{				
				ofxImGui::AddParameter(this->inheritsSettings);

				ofxImGui::AddParameter(this->fov);
				ofxImGui::AddRange("Clipping", this->nearClip, this->farClip);
				
				if (this->hasParent())
				{
					ofxImGui::AddParameter(this->attachToParent);
				}
				if (!this->isAttachedToParent())
				{
					ofxImGui::AddParameter(this->mouseControl);
					ofxImGui::AddParameter(this->relativeYAxis);
				}
				
				if (ImGui::Button("Reset"))
				{
					this->reset(true);
				}
				ImGui::SameLine();
				if (ImGui::Button("Set to Origin"))
				{
					this->easyCam.setPosition(glm::vec3(0.0f));
				}
				if (this->hasParent())
				{
					ImGui::SameLine();
					if (ImGui::Button("Copy from Parent"))
					{
						this->copyTransformFromParent();
					}
				}

				if (!this->isAttachedToParent())
				{
					ofxImGui::AddParameter(this->tiltSpeed);
					ofxImGui::AddParameter(this->panSpeed);
					ofxImGui::AddParameter(this->rollSpeed);
					ofxImGui::AddParameter(this->tumbleOverride);
					ofxImGui::AddParameter(this->dollySpeed);
				}

				ofxImGui::EndTree(settings);

				return true;
			}

			return false;
		}

		//--------------------------------------------------------------
		void Camera::serialize(nlohmann::json & json)
		{
			auto & jsonGroup = ofxPreset::Serializer::Serialize(json, this->parameters);
			
			ofxPreset::Serializer::Serialize(jsonGroup, this->easyCam, "ofEasyCam");
		}

		//--------------------------------------------------------------
		void Camera::deserialize(const nlohmann::json & json)
		{
			auto & jsonGroup = ofxPreset::Serializer::Deserialize(json, this->parameters);

			ofxPreset::Serializer::Deserialize(jsonGroup, this->easyCam, "ofEasyCam");
		}
	}
}
