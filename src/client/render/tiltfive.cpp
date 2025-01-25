// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 mbredif, indyteo


#include "config.h"

#if USE_TILTFIVE

#include "tiltfive.h"
#include "math.h"
#include "client/hud.h"
#include "client/camera.h"

constexpr std::chrono::milliseconds operator""_ms(unsigned long long ms) {
	return std::chrono::milliseconds(ms);
}

template <typename T> T get(const tiltfive::Result<T>& result, const std::string& error_prefix) {
	if (!result) {
		errorstream << error_prefix << result.error().message() << std::endl;
		std::exit(EXIT_FAILURE);
	}
	return *result;
}

void get(const tiltfive::Result<void>& result, const std::string& error_prefix) {
	if (!result) {
		errorstream << error_prefix << result.error().message() << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

template <typename T>
T waitForService(T5Client& client, const std::function<tiltfive::Result<T>(T5Client& client)>& func, const std::string& error_prefix) {
	for (bool waitingForService = false; ; waitingForService = true) {
		auto result = func(client);
		if (result || result.error() != tiltfive::Error::kNoService) 
			return get(result, error_prefix);

		warningstream << (waitingForService ? "." : "Waiting for service...") << std::flush;
		std::this_thread::sleep_for(100_ms);
	}
}

// TiltFiveGetPoseStep


TiltFiveGetPoseStep::~TiltFiveGetPoseStep() {
    if (camera) camera->drop();
    if (t5camera) t5camera->drop();
}

TiltFiveGetPoseStep::TiltFiveGetPoseStep(T5Glasses glasses, TextureBuffer *buffer, u8 left, u8 right) :
	glasses(glasses), isPoseValid(false), buffer(buffer), left(left), right(right)
{
    float fov = 48.0;
	int width = 1216;
	int height = 768;
    fovy  = fov * core::DEGTORAD64;
    aspectRatio = width / (float) height;
    frameInfo.vci.startY_VCI = -tan(0.5f * fovy);
    frameInfo.vci.startX_VCI = frameInfo.vci.startY_VCI * aspectRatio;
    frameInfo.vci.width_VCI = -2.0f * frameInfo.vci.startX_VCI;
    frameInfo.vci.height_VCI = -2.0f * frameInfo.vci.startY_VCI;
    frameInfo.texWidth_PIX = width;
    frameInfo.texHeight_PIX = height;
    frameInfo.isUpsideDown = false;
    frameInfo.isSrgb = false;
    frameInfo.rotToLVC_GBD = { 0., 0., 0., 0. };
    frameInfo.rotToRVC_GBD = { 0., 0., 0., 0. };
    frameInfo.posLVC_GBD = { 0., 0., 0. };
    frameInfo.posRVC_GBD = { 0., 0., 0. };

    t5camera = nullptr;
    camera = nullptr;
}

void TiltFiveGetPoseStep::reset(PipelineContext &context)
{
    scene::ISceneManager* scene = context.client->getSceneManager();
    if (!camera) {
        camera = scene->getActiveCamera();
        camera->grab();
    }
    if (!t5camera) {
        t5camera = scene->addCameraSceneNode(0, {0,0,2}, {0,0,0}, -1, true);
        t5camera->grab();
        t5camera->setFOV(fovy);
        t5camera->setAspectRatio(aspectRatio);
        t5camera->bindTargetAndRotation(true);
    }
    bool wasPoseValid = isPoseValid;
    isPoseValid = false;
	auto pose = glasses->getLatestGlassesPose(kT5_GlassesPoseUsage_GlassesPresentation);
	if (!pose) {
        if (!wasPoseValid) return;
		if (pose.error() == tiltfive::Error::kTryAgain) {
			warningstream << "Pose unavailable - Is gameboard visible?" << std::endl;
		} else {
			errorstream << "Pose unavailable - " << pose.error().message() << std::endl;
		}
        return;
	}
    isPoseValid = true;
    if (!wasPoseValid) warningstream << *pose << std::endl;
    frameInfo.rotToLVC_GBD = pose->rotToGLS_GBD;
    frameInfo.rotToRVC_GBD = pose->rotToGLS_GBD;
    frameInfo.posLVC_GBD = pose->posGLS_GBD;
    frameInfo.posRVC_GBD = pose->posGLS_GBD;
	frameInfo.leftTexHandle  = reinterpret_cast<void*>(static_cast<uintptr_t>(buffer->getTexture(left)->getOpenGLTextureName()));
	frameInfo.rightTexHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(buffer->getTexture(right)->getOpenGLTextureName()));

    float scaling = 1000;
    core::vector3df gbd(0,428,0);
    core::vector3df pos(pose->posGLS_GBD.x, pose->posGLS_GBD.z, pose->posGLS_GBD.y);
    pos = pos * scaling + gbd;

    core::quaternion quat(pose->rotToGLS_GBD.x, pose->rotToGLS_GBD.z, pose->rotToGLS_GBD.y, pose->rotToGLS_GBD.w);
    core::vector3df target(0,-1,0);
    core::vector3df up(0,0,1);
    target = pos + quat * target;
    up = quat * up;

    scene->setActiveCamera(t5camera);
    t5camera->setPosition(pos);
    t5camera->setTarget(target);
    t5camera->setUpVector(up);
}

void TiltFiveGetPoseStep::run(PipelineContext &context)
{
    scene::ISceneManager* scene = context.client->getSceneManager();
    scene->setActiveCamera(t5camera);
}

// TiltFiveSendFrameStep

TiltFiveSendFrameStep::TiltFiveSendFrameStep(TiltFiveGetPoseStep *step) :
	step(step)
{
}

void TiltFiveSendFrameStep::reset(PipelineContext &context)
{
    scene::ISceneManager* scene = context.client->getSceneManager();
    scene->setActiveCamera(step->camera);
}

void TiltFiveSendFrameStep::run(PipelineContext &context)
{
    scene::ISceneManager* scene = context.client->getSceneManager();
    scene->setActiveCamera(step->camera);
    if (!step->isPoseValid) return;
    auto result = step->glasses->sendFrame(&step->frameInfo);
    if (!result) {
        errorstream << "Error while sending frame: " << result.error().message() << std::endl;
    }
}



void populateTiltFivePipeline(RenderPipeline *pipeline, Client *client)
{
	auto client_result = tiltfive::obtainClient("luanti", "0.1.0", nullptr);
	if (!client_result)
	{
		errorstream << "Failed to create client: " << client_result.error().message() << std::endl;
        populatePlainPipeline(pipeline, client);
        return;

	}
	
    T5Client t5 = *client_result;
	warningstream << "Obtained client : " << t5 << std::endl;
    
	std::function<tiltfive::Result<std::string>(T5Client& t5)> func = [](T5Client& c) { return c->getServiceVersion(); };
	std::string serviceVersion = waitForService<std::string>(t5, func, "Error while getting service version: ");
	warningstream << "Service version : " << serviceVersion << std::endl;

	auto glassesIds_result = t5->listGlasses();
	if (!glassesIds_result)
	{
		errorstream << "Error while listing glasses: " << glassesIds_result.error().message() << std::endl;
        return;
	}

    v2f virtual_size_scale = v2f(1.0f, 1.0f);
    auto step3D = pipeline->own(create3DStage(client, virtual_size_scale));

	std::vector<std::string> glassesIds = *glassesIds_result;
	for (auto& glassesId : glassesIds)
	{
		warningstream << "Found glasses: " << glassesId << std::endl;
        auto glasses_result = tiltfive::obtainGlasses(glassesId, t5);
        if (!glasses_result)
        {
            errorstream << "Error creating glasses " << glassesId << " : " << glasses_result.error().message() << std::endl;
            continue;
        }
        T5Glasses glasses = *glasses_result;
        warningstream << "Created glasses: " << glassesId << std::endl;

	    // float ipd = BS*15**(glasses->getIpd());

        // Get the friendly name for the glasses
        std::string friendlyName = glassesId;
        // This is the name that's user set in the Tilt Five™ control panel.
        auto friendlyName_res = glasses->getFriendlyName();
        if (friendlyName_res) {
            friendlyName = *friendlyName_res;
            warningstream << "Obtained friendly name : " << *friendlyName_res << std::endl;
        } else if (friendlyName_res.error() == tiltfive::Error::kSettingUnknown) {
            errorstream << "Couldn't get friendly name : Service reports it's not set" << std::endl;
        } else {
            errorstream << "Error obtaining friendly name : " << friendlyName_res.error().message() << std::endl;
        }
        {
            // Wait for exclusive glasses connection
            auto connectionHelper = glasses->createConnectionHelper(friendlyName);
            auto connectionResult = connectionHelper->awaitConnection(std::chrono::milliseconds(10000));
            if (connectionResult) {
                warningstream << "Glasses connected for exclusive use" << std::endl;
            } else {
                errorstream << "Error connecting glasses for exclusive use : " << connectionResult.error().message() << std::endl;
                continue;
            }
        }

        T5_GraphicsContextGL settings;
        settings.textureMode = kT5_GraphicsApi_GL_TextureMode_Pair;
        auto result = glasses->initGraphicsContext(kT5_GraphicsApi_GL, &settings);
        if (!result) {
            errorstream << "Error initializing OpenGL context : " << result.error().message() << std::endl;
            continue;
        }

        static const u8 TEXTURE_LEFT = 0;
        static const u8 TEXTURE_RIGHT = 1;
        static const u8 TEXTURE_DEPTH = 2;

        auto driver = client->getSceneManager()->getVideoDriver();
        video::ECOLOR_FORMAT color_format = selectColorFormat(driver);
        video::ECOLOR_FORMAT depth_format = selectDepthFormat(driver);

        core::dimension2du size(1216, 768);

        TextureBuffer *buffer = pipeline->createOwned<TextureBuffer>();
        buffer->setTexture(TEXTURE_LEFT, size, "3d_render_left", color_format);
        buffer->setTexture(TEXTURE_RIGHT, size, "3d_render_right", color_format);
        buffer->setTexture(TEXTURE_DEPTH, size, "3d_depthmap_tiltfive", depth_format);

		auto poseStep = pipeline->addStep<TiltFiveGetPoseStep>(glasses, buffer, TEXTURE_LEFT, TEXTURE_RIGHT);
		auto left  = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> {TEXTURE_LEFT}, TEXTURE_DEPTH);
		pipeline->addStep<OffsetCameraStep>(false);
		pipeline->addStep<SetRenderTargetStep>(step3D, left);
		pipeline->addStep(step3D);
		auto right = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> {TEXTURE_RIGHT}, TEXTURE_DEPTH);
		pipeline->addStep<OffsetCameraStep>(true);
		pipeline->addStep<SetRenderTargetStep>(step3D, right);
		pipeline->addStep(step3D);
	    pipeline->addStep<TiltFiveSendFrameStep>(poseStep);
	}

    populatePlainPipeline(pipeline, client);
}

#else

#include "plain.h"
#include "log.h"

void populateTiltFivePipeline(RenderPipeline *pipeline, Client *client)
{
    populatePlainPipeline(pipeline, client);
    warningstream << "Tiltfive support is not enabled, falling back to default rendering mode" << std::endl;
}

#endif