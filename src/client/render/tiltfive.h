// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 mbredif, indyteo

#pragma once

#include "client/client.h"
#include "pipeline.h"

#if USE_TILTFIVE

#include "t5/TiltFiveNative.hpp"
#include "stereo.h"
#include "ICameraSceneNode.h"

using T5Client  = std::shared_ptr<tiltfive::Client>;
using T5Glasses = std::shared_ptr<tiltfive::Glasses>;

class TiltFiveGetPoseStep : public TrivialRenderStep
{
public:
	TiltFiveGetPoseStep(T5Glasses glasses, TextureBuffer *buffer, u8 left, u8 right);
	virtual ~TiltFiveGetPoseStep();

	void reset(PipelineContext &context) override;
	void run(PipelineContext &context) override;
	T5_FrameInfo frameInfo;
    T5Glasses glasses;
	bool isPoseValid;
	irr::scene::ICameraSceneNode *camera;
	irr::scene::ICameraSceneNode *t5camera;
private:
	TextureBuffer *buffer;
	u8 left;
	u8 right;
	float fovy;
	float aspectRatio;
};

class TiltFiveSendFrameStep : public TrivialRenderStep
{
public:
	TiltFiveSendFrameStep(TiltFiveGetPoseStep *step);

	void reset(PipelineContext &context) override;
	void run(PipelineContext &context) override;
private:
	TiltFiveGetPoseStep *step;
};

#endif

void populateTiltFivePipeline(RenderPipeline *pipeline, Client *client);