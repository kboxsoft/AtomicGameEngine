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

#include "GlowSettings.h"
#include "EmbreeScene.h"

#include "LightRay.h"
#include "BakeLight.h"
#include "BakeMesh.h"
#include "SceneBaker.h"

namespace AtomicGlow
{

const float LIGHT_ANGLE_EPSILON = 0.001f;

// http://www.altdevblogaday.com/2012/05/03/generating-uniformly-distributed-points-on-sphere/
static inline void GetRandomDirection(Vector3& result)
{
    float z = 2.0f * rand() / RAND_MAX - 1.0f;
    float t = 2.0f * rand() / RAND_MAX * 3.14f;
    float r = sqrt(1.0f - z * z);
    result.x_ = r * (float) cos(t);
    result.y_ = r * (float) sin(t);
    result.z_ = z;
}

BakeLight::BakeLight(Context* context, SceneBaker* sceneBaker) : BakeNode(context, sceneBaker),
    range_(0.0f)
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

    if (source.normal == Vector3::ZERO)
        return;

    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetRTCScene();

    const Color& color = zone_->GetAmbientColor();

    Vector3 rad(color.r_, color.g_, color.b_);

    if (!GlobalGlowSettings.aoEnabled_)
    {
        source.bakeMesh->ContributeRadiance(lightRay, rad, GLOW_LIGHTMODE_AMBIENT);
        return;
    }


    // TODO: AO using ray packets/streams

    RTCRay& ray = lightRay->rtcRay_;

    unsigned nsamples = GlobalGlowSettings.nsamples_;

    // this needs to be based on model/scale likely?
    float aoDepth = GlobalGlowSettings.aoDepth_;

    // smallest percent of ao value to use
    float aoMin = GlobalGlowSettings.aoMin_;

    // brightness control
    float multiply = GlobalGlowSettings.aoMultiply_;

    // Shoot rays through the differential hemisphere.
    int nhits = 0;
    float avgDepth = 0.0f;
    for (unsigned nsamp = 0; nsamp < nsamples; nsamp++)
    {
        Vector3 rayDir;
        GetRandomDirection(rayDir);

        float dotp = source.normal.x_ * rayDir.x_ +
                     source.normal.y_ * rayDir.y_ +
                     source.normal.z_ * rayDir.z_;

        if (dotp < 0.1f)
        {
            continue;
        }

        float variance = 0.0f;//nsamples <= 32 ? 0.0f : aoDepth * ((float) rand() / (float) RAND_MAX) * 0.25f;

        float depth = aoDepth + variance;

        lightRay->SetupRay(source.position, rayDir, .001f, depth);

        rtcOccluded(scene, ray);

        if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
        {
            avgDepth += Min<float>(ray.tfar, aoDepth);
            nhits++;
        }
    }    

    if (nhits)// && (nsamples <= 32 ? true : nhits > 4))
    {
        avgDepth /= float(nhits);
        avgDepth /= aoDepth;

        avgDepth = Clamp<float>(avgDepth, 0.1f, 1.0f) * 100.0f;
        avgDepth *= avgDepth;
        float ao = avgDepth / 10000.0f;

        ao = aoMin + ao/2.0f;
        ao *= multiply;
        ao = Clamp<float>(ao, aoMin, 1.0f);

        rad *= ao;
    }

    source.bakeMesh->ContributeRadiance(lightRay, rad, GLOW_LIGHTMODE_AMBIENT);
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

    LightRay::SamplePoint& source = lightRay->samplePoint_;
    RTCRay& ray = lightRay->rtcRay_;

    float angle = direction_.DotProduct(source.normal);

    if (angle < 0.0f)
        return;

    lightRay->SetupRay(source.position, direction_);

    rtcOccluded(scene, ray);

    // obstructed?  TODO: glass, etc
    if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
        return;

    Vector3 rad(color_.r_, color_.g_, color_.b_);

    rad*=angle;

    source.bakeMesh->ContributeRadiance(lightRay, rad);

}

void DirectionalBakeLight::SetLight(Atomic::Light* light)
{
    node_ = light->GetNode();

    color_ = light->GetColor();

    direction_ = -node_->GetWorldDirection();
    direction_.Normalize();

}

// Point Lights

PointBakeLight::PointBakeLight(Context* context, SceneBaker* sceneBaker) : BakeLight(context, sceneBaker)
{
}

PointBakeLight::~PointBakeLight()
{

}

void PointBakeLight::Light(LightRay* lightRay)
{
    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetRTCScene();

    LightRay::SamplePoint& source = lightRay->samplePoint_;
    RTCRay& ray = lightRay->rtcRay_;

    Vector3 dir = position_ - source.position;

    float dist = dir.Length();

    if (range_ <= 0.0f || dist >= range_)
        return;

    dir.Normalize();

    float dot = dir.DotProduct(source.normal);

    if (dot < 0.0f)
        return;

    lightRay->SetupRay(source.position, dir, .001f, dist);

    rtcOccluded(scene, ray);

    // obstructed?  TODO: glass, etc
    if (ray.geomID != RTC_INVALID_GEOMETRY_ID)
        return;

    Vector3 rad(color_.r_, color_.g_, color_.b_);

    rad *= Max<float> (1.0f - ( dist * 1.2 / range_), 0.0f);
    rad *= dot;

    if (rad.Length() > M_EPSILON)
        source.bakeMesh->ContributeRadiance(lightRay, rad);

}

void PointBakeLight::SetLight(Atomic::Light* light)
{
    node_ = light->GetNode();

    color_ = light->GetColor();
    position_ = node_->GetWorldPosition();

    range_ = light->GetRange();

}

// Bounce Lights

BounceBakeLight::BounceBakeLight(Context* context, SceneBaker* sceneBaker) : BakeLight(context, sceneBaker)
{
}

BounceBakeLight::~BounceBakeLight()
{

}

void BounceBakeLight::Light(LightRay* lightRay)
{
    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetRTCScene();
    LightRay::SamplePoint& source = lightRay->samplePoint_;          
    RTCRay& ray = lightRay->rtcRay_;

    Vector3 totalRad;
    int numRad = 0;

    for (unsigned i = 0; i < bounceSamples_.Size(); i++)
    {
        const BounceSample& b = bounceSamples_[i];

        // don't light self
        if (source.triangle == b.triIndex_)
            continue;

        BakeMesh::MMTriangle* tri = &bakeMesh_->triangles_[b.triIndex_];

        if (tri->normal_.DotProduct(source.normal) > 0.0f)
            continue;

        Vector3 dir = b.position_ - source.position;

        Vector3 r = b.radiance_;

        r /= 3.0;

        float range = r.Length() * 8.0f;

        float dist = dir.Length();

        if (dist > range)
            continue;

        dir.Normalize();

        float dot = dir.DotProduct(source.normal);

        if (dot < 0.0f)
            continue;

        Vector3 rad = b.srcColor_;

        rad *= Max<float> (1.0f - ( dist * 1.2 / range), 0.0f);
        //rad *= dot;

        totalRad += rad;
        numRad++;
    }

    if (!numRad)
        return;

    // average rad is what we want?
    totalRad /= numRad;

    if (totalRad.Length() > M_EPSILON)
        source.bakeMesh->ContributeRadiance(lightRay, totalRad, GLOW_LIGHTMODE_INDIRECT);

}

void BounceBakeLight::AddBounceSample(const BounceSample& bounceSample)
{
    bounceSamples_.Push(bounceSample);
}

void BounceBakeLight::SetBakeMesh(BakeMesh* bakeMesh)
{
    bakeMesh_ = bakeMesh;
    node_ = bakeMesh->GetNode();

    color_ = Color::MAGENTA;
    range_ = -1.0f;
    position_ = Vector3::ZERO;
}

void BounceBakeLight::SetLight(Atomic::Light* light)
{
    node_ = light->GetNode();

    color_ = light->GetColor();
    position_ = node_->GetWorldPosition();

    range_ = light->GetRange();

}



}
