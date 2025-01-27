
#include "XrSetupCamera.h"
#include "client/client.h"
#include "client/camera.h"
#include "math.h"

SaveCameraState::SaveCameraState(CameraState* camState) : state(camState) {}

void SaveCameraState::run(PipelineContext &context)
{
    scene::ICameraSceneNode* cameraNode = context.client->getCamera()->getCameraNode();
    state->position = cameraNode->getPosition();
    state->upVector  = cameraNode->getUpVector();
    state->rotation = cameraNode->getRotation();
    state->znear = cameraNode->getNearValue();
    state->zfar = cameraNode->getFarValue();
    state->aspectratio = cameraNode->getAspectRatio();
    state->fov = cameraNode->getFOV();
    state->TargetAndRotationAreBound = cameraNode->getTargetAndRotationBinding();
};


RestoreCameraState::RestoreCameraState(CameraState* camState) : state(camState) {}

void RestoreCameraState::run(PipelineContext &context)
{
    scene::ICameraSceneNode* cameraNode = context.client->getCamera()->getCameraNode();
    cameraNode->bindTargetAndRotation(state->TargetAndRotationAreBound);
    cameraNode->setPosition(state->position);
    cameraNode->setUpVector(state->upVector);
    cameraNode->setRotation(state->rotation);
    cameraNode->setNearValue(state->znear);
    cameraNode->setFarValue(state->zfar);
    cameraNode->setAspectRatio(state->aspectratio);
    cameraNode->setFOV(state->fov);
    cameraNode->updateAbsolutePosition();
    cameraNode->updateMatrices();
}

ViewState::ViewState(u32 width, u32 height, float fovy, float znear, float zfar)
    : Width(width), Height(height), isPoseValid(false), FoV(fovy), ZNear(znear), ZFar(zfar)
{
    AspectRatio = Width / float(Height);
}

XrSetupCamera::XrSetupCamera(ViewState* viewState, int viewId)
    : view(viewState), viewId(viewId) {}

void XrSetupCamera::run(PipelineContext &context)
{
    scene::ICameraSceneNode* cameraNode = context.client->getCamera()->getCameraNode();
    cameraNode->bindTargetAndRotation(false);
    cameraNode->setPosition(view->Position[viewId]);
    cameraNode->updateAbsolutePosition();
    cameraNode->setTarget(view->Position[viewId] + view->TargetVector);
    cameraNode->setUpVector(view->UpVector);
    cameraNode->updateMatrices();
}