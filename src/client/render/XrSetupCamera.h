#pragma once

#include "pipeline.h"
#include "quaternion.h"
#include "client/camera.h"
#include "t5/TiltFiveNative.hpp"

struct CameraState : public RenderPipelineObject
{
	core::vector3df position;
	core::vector3df rotation;
	core::vector3df upVector;
	f32 fov, aspectratio;
	f32 znear, zfar;
	bool TargetAndRotationAreBound;
	CameraMode camera_mode;
	v3s16 camera_offset;
};

class SaveCameraState : public TrivialRenderStep
{
public:
	SaveCameraState() = delete;
	SaveCameraState(CameraState* camState);
	virtual void run(PipelineContext &context) override;

private:
	CameraState* state;
};

class RestoreCameraState : public TrivialRenderStep
{
public:
	RestoreCameraState() = delete;
	RestoreCameraState(CameraState* camState);
	virtual void run(PipelineContext &context) override;
	
private:
	CameraState* state;
};

struct InputState : public RenderPipelineObject {
};

struct ViewState : public RenderPipelineObject
{
    ViewState(u32 width, u32 height, float fovy, float znear, float zfar);
	// TextureBufferOutput* RenderTarget[2];
	T5_FrameInfo frameInfo;
	// Viewport
	u32 Width;
	u32 Height;
	bool isPoseValid;

	// HMD translation/orientation of eye relative to playspace origin
	core::vector3df Position[3];
	core::vector3df TargetVector;
	core::vector3df UpVector;

    f32 AspectRatio;
    f32 FoV; // radians
	f32 ZNear;
	f32 ZFar;
    float speed = 0.005;
    float scaling = 2000;
    core::vector3df gbd = {0,410,0};
	
	enum CenterMode {
		CENTER_ON_PLAYER = 0,
		CENTER_ON_PLAYER_GROUND = 1,
		CENTER_ON_TARGET = 2,
		CENTER_MODES = 3
	} center_mode;
};

//! Setup the camera for rendering to an XR view target
class XrSetupCamera : public TrivialRenderStep
{
public:
	XrSetupCamera(ViewState* viewState, int viewId);
	virtual void run(PipelineContext &context) override;
	
private:
	ViewState* view;
    int viewId;
};
