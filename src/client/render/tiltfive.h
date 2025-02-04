// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 mbredif, indyteo

#pragma once

#include "client/client.h"
#include "pipeline.h"

#if USE_TILTFIVE

#include "TiltFiveNative.hpp"
#include "stereo.h"
#include "ICameraSceneNode.h"
#include "XrSetupCamera.h"

using T5Client  = std::shared_ptr<tiltfive::Client>;
using T5Glasses = std::shared_ptr<tiltfive::Glasses>;
using T5Wand = std::shared_ptr<tiltfive::Wand>;

class TiltFiveGetPoseStep : public TrivialRenderStep
{
public:
	TiltFiveGetPoseStep(T5Glasses glasses, ViewState *view, TextureBuffer *buffer, u8 left, u8 right);
	void run(PipelineContext &context) override;
    T5Glasses glasses;
	std::vector<T5Wand> wands;
	std::vector<T5_WandReport> lastWandReports;
	ViewState *view;
private:
	TextureBuffer *buffer;
	u8 left;
	u8 right;
};

class TiltFiveSendFrameStep : public TrivialRenderStep
{
public:
	TiltFiveSendFrameStep(T5Glasses glasses, ViewState *view);
	void run(PipelineContext &context) override;
private:
	T5Glasses glasses;
	ViewState *view;
};

#endif

void populateTiltFivePipeline(RenderPipeline *pipeline, Client *client);