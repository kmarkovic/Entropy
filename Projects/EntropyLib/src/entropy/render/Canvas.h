#pragma once

#include "ofxPreset.h"
#include "ofxTextureRecorder.h"
#include "ofxWarp.h"

#include "Layout.h"

namespace entropy
{
	namespace render
	{
		class Canvas
		{
		public:
			Canvas(Layout layout);
			~Canvas();

			void update();

			void beginDraw();
			void endDraw();
			
			void render(const ofRectangle & bounds);

			const ofTexture & getRenderTexture() const;

			float getWidth() const;
			float getHeight() const;
			const ofRectangle & getViewport() const;

			void setWidth(float width);
			void setHeight(float height);

			bool getFillWindow() const;
			void setFillWindow(bool fillWindow);

			shared_ptr<ofxWarp::WarpBase> addWarp(ofxWarp::WarpBase::Type type);
			void removeWarp();

			bool isEditing() const;

			void drawGui(ofxImGui::Settings & settings);

			void serialize(nlohmann::json & json);
			void deserialize(const nlohmann::json & json);

			Layout getLayout() const;

			std::filesystem::path getSettingsFilePath();
			std::filesystem::path getShaderPath(const string & shaderFile = "");

			bool loadSettings();
			bool saveSettings();

			bool selectClosestControlPoint(const glm::vec2 & pos);

			bool cursorMoved(const glm::vec2 & pos);
			bool cursorDown(const glm::vec2 & pos);
			bool cursorDragged(const glm::vec2 & pos);

			bool keyPressed(ofKeyEventArgs & args);

			void screenResized(ofResizeEventArgs & args);
			
			ofEvent<ofResizeEventArgs> resizeEvent;

			static const int MAX_NUM_WARPS = 8;

		protected:
			void updateSize();
			void updateStitches();

			void resetWarpSizes();

			struct WarpParameters
				: ofParameterGroup
			{
				ofParameter<bool> editing{ "Edit Shape", false };
				ofParameter<bool> enabled{ "Enabled", true };
				ofParameter<float> brightness{ "Brightness", 1.0f, 0.0f, 1.0f };

				struct : ofParameterGroup
				{
					ofParameter<bool> adaptive{ "Adaptive", true };
					ofParameter<bool> linear{ "Linear", false };

					PARAM_DECLARE("Mesh", adaptive, linear);
				} mesh;

				struct : ofParameterGroup
				{
					ofParameter<bool> luminanceChannelLock{ "Luminance Channel Lock", false };
					ofParameter<glm::vec3> luminance{ "Luminance", glm::vec3(0.5f), glm::vec3(0.0f), glm::vec3(1.0f) };
					ofParameter<bool> gammaChannelLock{ "Gamma Channel Lock", true };
					ofParameter<glm::vec3> gamma{ "Gamma", glm::vec3(1.0f), glm::vec3(0.0f), glm::vec3(8.0f) };
					ofParameter<float> exponent{ "Exponent", 2.0f, 0.1f, 8.0f };
					ofParameter<float> edgeLeft{ "Edge Left", 0.0f, 0.0f, 1.0f };
					ofParameter<float> edgeRight{ "Edge Right", 0.0f, 0.0f, 1.0f };

					PARAM_DECLARE("Blend", luminanceChannelLock, luminance, gammaChannelLock, gamma, exponent, edgeLeft, edgeRight);
				} blend;

				PARAM_DECLARE("Warp", editing, enabled, brightness, mesh, blend);
			};

			struct : ofParameterGroup
			{
				ofParameter<bool> fillWindow{ "Fill Window", false };
				ofParameter<glm::vec2> globalOffset{ "Global Offset", glm::vec2(0.0f), glm::vec2(-100.0f), glm::vec2(100.0f) };
				ofParameter<bool> additiveBlend{ "Additive Blend", false };

				PARAM_DECLARE("Canvas", 
					fillWindow,
					globalOffset,
					additiveBlend);
			} parameters;

			Layout layout;

			ofRectangle viewport;

			ofFbo fboDraw;
			ofFbo::Settings fboSettings;

			bool exportFrames;
			ofxTextureRecorder textureRecorder;

			float screenWidth;
			float screenHeight;
			
			vector<shared_ptr<ofxWarp::WarpBase>> warps;
			size_t focusedIndex;

			vector<ofRectangle> srcAreas;
			vector<WarpParameters> warpParameters;
			bool openGuis[MAX_NUM_WARPS];  // Don't use vector<bool> because they're weird: http://en.cppreference.com/w/cpp/container/vector_bool
			
			bool dirtyStitches;
		};
	}
}
