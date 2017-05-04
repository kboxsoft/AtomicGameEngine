// Copyright (c) 2014-2017, THUNDERBEAST GAMES LLC All rights reserved
// Copyright 2009-2017 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Atomic/Graphics/Zone.h>

#include "EmbreeScene.h"

#include "LightRay.h"
#include "BakeLight.h"
#include "BakeMesh.h"
#include "SceneBaker.h"

namespace AtomicGlow
{


BakeLight::BakeLight(Context* context, SceneBaker* sceneBaker) : BakeNode(context, sceneBaker)
{
}

BakeLight::~BakeLight()
{

}

// Zone Lights

ZoneBakeLight::ZoneBakeLight(Context* context, SceneBaker* sceneBaker) : BakeLight(context, sceneBaker)
{
}

ZoneBakeLight::~ZoneBakeLight()
{

}

void ZoneBakeLight::Light(LightRay* lightRay)
{
    LightRay::SamplePoint& source = lightRay->samplePoint_;

    const Color& color = zone_->GetAmbientColor();

    source.bakeMesh->SetRadiance(source.radianceX, source.radianceY, Vector3(color.r_, color.g_, color.b_));
}

void ZoneBakeLight::SetZone(Zone* zone)
{
    node_ = zone->GetNode();
    zone_ = zone;
}


// Directional Lights

DirectionalBakeLight::DirectionalBakeLight(Context* context, SceneBaker* sceneBaker) : BakeLight(context, sceneBaker)
{
}

DirectionalBakeLight::~DirectionalBakeLight()
{

}

void DirectionalBakeLight::Light(LightRay* lightRay)
{
    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetRTCScene();

    const float E = 0.001f;

    LightRay::SamplePoint& source = lightRay->samplePoint_;
    RTCRay& ray = lightRay->rtcRay_;

    lightRay->SetupRay(source.position, direction_);

    rtcOccluded(scene, ray);

    if (ray.geomID == RTC_INVALID_GEOMETRY_ID)
    {
        source.bakeMesh->SetRadiance(source.radianceX, source.radianceY, Vector3(1, 1, 1));
    }

}

void DirectionalBakeLight::SetLight(Atomic::Light* light)
{
    node_ = light->GetNode();

    direction_ = -node_->GetWorldDirection();
    direction_.Normalize();

}

}
