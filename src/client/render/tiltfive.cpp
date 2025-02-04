// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 mbredif, indyteo


#include "config.h"

#if USE_TILTFIVE

#include "tiltfive.h"
#include "math.h"
#include "client/hud.h"
#include "client/camera.h"
#include "client/minimap.h"
#include "client/clientmap.h"
#include "client/content_cao.h"
#include "XrSetupCamera.h"

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

TiltFiveGetPoseStep::TiltFiveGetPoseStep(T5Glasses glasses, ViewState *view, TextureBuffer *buffer, u8 left, u8 right) :
	glasses(glasses), view(view), buffer(buffer), left(left), right(right)
{
    float aspectRatio = view->Width / (float) view->Height;
    T5_FrameInfo& frameInfo = view->frameInfo;
    frameInfo.vci.startY_VCI = -tan(0.5f * view->FoV);
    frameInfo.vci.startX_VCI = frameInfo.vci.startY_VCI * aspectRatio;
    frameInfo.vci.width_VCI = -2.0f * frameInfo.vci.startX_VCI;
    frameInfo.vci.height_VCI = -2.0f * frameInfo.vci.startY_VCI;
    frameInfo.texWidth_PIX = view->Width;
    frameInfo.texHeight_PIX = view->Height;
    frameInfo.isUpsideDown = false;
    frameInfo.isSrgb = false;
    frameInfo.rotToLVC_GBD = { 0., 0., 0., 0. };
    frameInfo.rotToRVC_GBD = { 0., 0., 0., 0. };
    frameInfo.posLVC_GBD = { 0., 0., 0. };
    frameInfo.posRVC_GBD = { 0., 0., 0. };
}

void TiltFiveGetPoseStep::run(PipelineContext &context)
{

    for (unsigned int i = 0; i < wands.size(); ++i)
    {
        T5_WandReport& lastWandReport = lastWandReports[i];
        auto report = wands[i]->getLatestReport();
        if(!report) {
		    errorstream << "Error while reporting wand : " << report.error().message() << std::endl;
            continue;
        }
        if(!report->buttonsValid) continue;
        view->gbd.X += view->speed * view->scaling * report->stick.x;
        view->gbd.Z += view->speed * view->scaling * report->stick.y;
        view->gbd.Y += view->speed * view->scaling * (int(report->buttons.two) - int(report->buttons.one));
        if(report->buttons.three && !lastWandReport.buttons.three) {
            view->scaling *= 2;
            if (view->scaling > 4000)
                view->scaling = 250;
        }
        if(report->buttons.t5 && !lastWandReport.buttons.t5) {
            view->center_mode = ViewState::CenterMode(view->center_mode+1);
            if (view->center_mode == ViewState::CENTER_MODES)
                view->center_mode = ViewState::CENTER_ON_TARGET;
            errorstream << "TODO: tiltfive mode" << view->center_mode << std::endl;
        }
        if(report->buttons.a) context.client->getEnv().setTimeOfDay(6000);
        lastWandReport = *report;
    }
    scene::ICameraSceneNode* cameraNode = context.client->getCamera()->getCameraNode();
    bool wasPoseValid = view->isPoseValid;
    view->isPoseValid = false;
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
    view->isPoseValid = true;
    if (!wasPoseValid) warningstream << *pose << std::endl;
    T5_FrameInfo& frameInfo = view->frameInfo;
    frameInfo.rotToLVC_GBD = pose->rotToGLS_GBD;
    frameInfo.rotToRVC_GBD = pose->rotToGLS_GBD;
	frameInfo.leftTexHandle  = reinterpret_cast<void*>(static_cast<uintptr_t>(buffer->getTexture(left)->getOpenGLTextureName()));
	frameInfo.rightTexHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(buffer->getTexture(right)->getOpenGLTextureName()));
    cameraNode->setNearValue(view->ZNear);
    cameraNode->setFarValue(view->ZFar);
    cameraNode->setFOV(view->FoV);// AngleUp, info.AngleDown, info.AngleRight, info.AngleLeft);
    cameraNode->setAspectRatio(view->AspectRatio);

    core::vector3df pos(pose->posGLS_GBD.x, pose->posGLS_GBD.z, pose->posGLS_GBD.y);
    core::quaternion quat(pose->rotToGLS_GBD.x, pose->rotToGLS_GBD.z, pose->rotToGLS_GBD.y, pose->rotToGLS_GBD.w);
    quat.normalize();

    auto ipd = glasses->getIpd();
    core::vector3df move(ipd ? *ipd  * 0.5f : 0,0,0);
    core::vector3df target(0,-1,0);
    core::vector3df up(0,0,1);
    move = quat * move;
    view->TargetVector = quat * target;
    view->UpVector = quat * up;
    //quat.W = -quat.W; // inverse rotation
    //quat.toEuler(view->Rotation);
    //view->Rotation *= core::RADTODEG64;

    core::vector3df left = pos - move;
    core::vector3df right = pos + move;
    view->Position[0] =  pos   * view->scaling + view->gbd;
    view->Position[1] =  left  * view->scaling + view->gbd;
    view->Position[2] =  right * view->scaling + view->gbd;
    frameInfo.posLVC_GBD = {left.X, left.Z, left.Y};
    frameInfo.posRVC_GBD = {right.X, right.Z, right.Y};
    
    // from  Camera::updateOffset()
	f32 CAMERA_OFFSET_STEP = 200;
    v3f cp = view->Position[0] / BS;
    v3s16 camera_offset(
		floorf(cp.X / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP,
		floorf(cp.Y / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP,
		floorf(cp.Z / CAMERA_OFFSET_STEP) * CAMERA_OFFSET_STEP
	);
    core::vector3df camera_offset_pos = intToFloat(camera_offset, BS);
    view->Position[0] -=  camera_offset_pos;
    view->Position[1] -=  camera_offset_pos;
    view->Position[2] -=  camera_offset_pos;

    float fovy = view->FoV;
    float fovx = 2 * atan(view->AspectRatio * tan(0.5 * fovy));
    float fovmax = std::max(fovx, fovy);
    irr::video::SColor light_color(255, 255, 255, 255);

    ClientEnvironment& env = context.client->getEnv();
    env.getClientMap().updateCamera(view->Position[0], view->TargetVector, fovmax, camera_offset, light_color);
	env.updateCameraOffset(camera_offset);
    context.client->getCamera()->setCameraMode(CAMERA_MODE_THIRD);
	GenericCAO * playercao = env.getLocalPlayer()->getCAO();
    playercao->updateMeshCulling();
	playercao->setChildrenVisible(true);
    playercao->updateAttachments(); // fix display of player in 3rd person mode
	env.getClientMap().updateDrawList();
}

// TiltFiveSendFrameStep

TiltFiveSendFrameStep::TiltFiveSendFrameStep(T5Glasses glasses, ViewState *view) :
	glasses(glasses), view(view)
{
}

void TiltFiveSendFrameStep::run(PipelineContext &context)
{
    if (!view->isPoseValid) return;
    auto result = glasses->sendFrame(&view->frameInfo);
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

    CameraState* camState = pipeline->createOwned<CameraState>();
    pipeline->addStep<SaveCameraState>(camState);

    v2f virtual_size_scale = v2f(1.0f, 1.0f);
    auto draw3d = pipeline->own(create3DStage(client, virtual_size_scale));
    RenderTarget *screen = pipeline->createOwned<ScreenTarget>();
	pipeline->addStep<SetRenderTargetStep>(draw3d, screen);
	pipeline->addStep(draw3d);

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

        ViewState* viewState = pipeline->createOwned<ViewState>(1216, 768, 48.0*core::DEGTORAD64, 1., 100000.);
        auto driver = client->getSceneManager()->getVideoDriver();
        video::ECOLOR_FORMAT color_format = selectColorFormat(driver);
        video::ECOLOR_FORMAT depth_format = selectDepthFormat(driver);
        TextureBuffer *buffer = pipeline->createOwned<TextureBuffer>();
        static const u8 TEXTURE_LEFT = 0;
        static const u8 TEXTURE_RIGHT = 1;
        static const u8 TEXTURE_DEPTH = 2;
        core::dimension2du size(viewState->Width, viewState->Height);
        buffer->setTexture(TEXTURE_LEFT, size, "3d_render_left", color_format);
        buffer->setTexture(TEXTURE_RIGHT, size, "3d_render_right", color_format);
        buffer->setTexture(TEXTURE_DEPTH, size, "3d_depthmap_tiltfive", depth_format);
		TextureBufferOutput *left = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> {TEXTURE_LEFT}, TEXTURE_DEPTH);
		TextureBufferOutput *right = pipeline->createOwned<TextureBufferOutput>(buffer, std::vector<u8> {TEXTURE_RIGHT}, TEXTURE_DEPTH);

		auto getPoseStep = pipeline->addStep<TiltFiveGetPoseStep>(glasses, viewState, buffer, TEXTURE_LEFT, TEXTURE_RIGHT);
		
        pipeline->addStep<SetRenderTargetStep>(draw3d, left);
		pipeline->addStep<XrSetupCamera>(viewState, 1);
		pipeline->addStep(draw3d);

		pipeline->addStep<SetRenderTargetStep>(draw3d, right);
		pipeline->addStep<XrSetupCamera>(viewState, 2);
		pipeline->addStep(draw3d);
	    
        pipeline->addStep<TiltFiveSendFrameStep>(glasses, viewState);


        auto wandHelper = glasses->getWandStreamHelper();
        auto wands_result = wandHelper->listWands();
        if (wands_result && !wands_result->empty()) {
            getPoseStep->wands = *wands_result;
            getPoseStep->lastWandReports.resize(getPoseStep->wands.size());
            warningstream << "Wand(s) connected to glasses " << glassesId << std::endl;
            for(auto& wand : *wands_result)
                warningstream << "Found :  " << wand << std::endl;
        }

	}
    pipeline->addStep<RestoreCameraState>(camState);

	pipeline->addStep<DrawWield>()->setRenderTarget(screen);
	pipeline->addStep<MapPostFxStep>()->setRenderTarget(screen);
	pipeline->addStep<DrawHUD>()->setRenderTarget(screen);
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