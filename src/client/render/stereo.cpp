// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "stereo.h"
#include "client/client.h"
#include "client/camera.h"
#include "constants.h"
#include "settings.h"

OffsetCameraStep::OffsetCameraStep(float eye_offset) : eye_offset(eye_offset)
{
}


OffsetCameraStep::OffsetCameraStep(bool right_eye)
{
	eye_offset = BS * g_settings->getFloat("3d_paralax_strength", -0.087f, 0.087f) * (right_eye ? 1 : -1);
}

void OffsetCameraStep::reset(PipelineContext &context)
{	
	scene::ICameraSceneNode* node = context.client->getSceneManager()->getActiveCamera();
	position = node->getPosition();
	core::vector3df toTarget = node->getTarget() - position;
	core::vector3df upVector = node->getUpVector();
	core::vector3df right = toTarget.crossProduct(upVector);
	right.normalize();
	errorstream << right.X << "," << right.Y << "," << right.Z << std::endl;
	position += eye_offset * right;
}

void OffsetCameraStep::run(PipelineContext &context)
{
	scene::ICameraSceneNode* node = context.client->getSceneManager()->getActiveCamera();
	core::vector3df target = node->getTarget() + position - node->getPosition();
	node->setPosition(position);
	node->setTarget(target);
	
}
