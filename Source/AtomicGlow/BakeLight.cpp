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

#include "EmbreePrivate.h"
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

// Directional Lights

// for very small cones treat as singular light, because float precision is not good enough
static const float DIRECTIONAL_COS_ANGLE_MAX = 0.99999988f;

BakeLightDirectional::BakeLightDirectional(Context* context, SceneBaker* sceneBaker) : BakeLight(context, sceneBaker)
{
}

BakeLightDirectional::~BakeLightDirectional()
{

}

void BakeLightDirectional::Light(LightRay* lightRay)
{
    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetEmbree()->rtcScene_;

    const float E = 0.001f;

    LightRay::SampleSource& source = lightRay->GetSampleSource();

    RTCRay& ray = lightRay->d_->ray_;

    ray.org[0] = source.position.x_;
    ray.org[1] = source.position.y_;
    ray.org[2] = source.position.z_;

    Vector3 dir = -node_->GetWorldDirection();
    dir.Normalize();

    ray.dir[0] = dir.x_;
    ray.dir[1] = dir.y_;
    ray.dir[2] = dir.z_;

    ray.tnear = E;
    ray.tfar = 100000.0f;

    ray.mask = 0xFFFFFFFF;
    ray.geomID = RTC_INVALID_GEOMETRY_ID;

    rtcOccluded(scene, ray);

    if (ray.geomID == RTC_INVALID_GEOMETRY_ID)
    {
        source.bakeMesh->SetRadiance(source.radianceX, source.radianceY, Vector3(1, 0, 0));
    }

}

void BakeLightDirectional::SetLight(Atomic::Light* light)
{
    node_ = light->GetNode();
}

/*

void BakeLightDirectional::Sample(SampleResult& result, const embree::DifferentialGeometry& dg, const embree::Vec2f& s)
{

    result.dir = frame_.vz;
    result.dist = embree::inf;
    result.pdf = pdf_;

    if (cosAngle_ < DIRECTIONAL_COS_ANGLE_MAX)
    {
        result.dir = frame_ * embree::uniformSampleCone(cosAngle_, s);
    }

    result.weight = radiance_; // *pdf/pdf cancel

}

void BakeLightDirectional::Eval(EvalResult& result, const embree::DifferentialGeometry& dg, const embree::Vec3fa& dir)
{
    result.dist = embree::inf;

    if (cosAngle_ < DIRECTIONAL_COS_ANGLE_MAX && embree::dot(frame_.vz, dir) > cosAngle_)
    {
      result.value = radiance_ * pdf_;
      result.pdf = pdf_;
    }
    else
    {
      result.value = embree::Vec3fa(0.f);
      result.pdf = 0.f;
    }

}
*/


}
