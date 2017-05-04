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

#include <xmmintrin.h>
#include <pmmintrin.h>
#include <cmath>
#include <cfloat>

#include "EmbreePrivate.h"

#include "LightRay.h"

namespace AtomicGlow
{

LightRay::LightRay()
{
    d_ = new EmbreeRayPrivate();

    Clear();
}

LightRay::~LightRay()
{
    delete d_;
}

void LightRay::Clear()
{
    const float E = 0.001f;

    SetOrigin(Vector3::ZERO);
    SetDir(Vector3::ZERO);
    SetTNear(E);
    SetTFar(100000.0f);

    d_->ray_.mask = 0xFFFFFFFF;
    d_->ray_.time = 0.0f;

    ClearHit();

}

void LightRay::ClearHit()
{
    d_->ray_.geomID = RTC_INVALID_GEOMETRY_ID;
    d_->ray_.primID = RTC_INVALID_GEOMETRY_ID;
    d_->ray_.instID = RTC_INVALID_GEOMETRY_ID;
    d_->ray_.u = 0.0f;
    d_->ray_.v = 0.0f;
    d_->ray_.Ng[0] = d_->ray_.Ng[1] = d_->ray_.Ng[2] = 0.0f;
}

void LightRay::SetOrigin(const Vector3& origin)
{
    d_->ray_.org[0] = origin.x_;
    d_->ray_.org[1] = origin.y_;
    d_->ray_.org[2] = origin.z_;
}

const Vector3 LightRay::GetOrigin() const
{
    return Vector3(d_->ray_.org[0], d_->ray_.org[1], d_->ray_.org[2]);
}

void LightRay::SetDir(const Vector3& dir)
{
    d_->ray_.dir[0] = dir.x_;
    d_->ray_.dir[1] = dir.y_;
    d_->ray_.dir[2] = dir.z_;

}

const Vector3 LightRay::GetDir() const
{
    return Vector3(d_->ray_.dir[0], d_->ray_.dir[1], d_->ray_.dir[2]);
}

// Start of ray segment
void LightRay::SetTNear(float tnear)
{
    d_->ray_.tnear = tnear;
}

float LightRay::GetTNear() const
{
    return d_->ray_.tnear;
}

// End of ray segment (set to hit distance)
void LightRay::SetTFar(float tfar)
{
    d_->ray_.tfar = tfar;
}

float LightRay::GetTFar() const
{
    return d_->ray_.tfar;
}

BakeMesh* LightRay::GetHitMesh()
{

}

Vector3 LightRay::GetHitNormal()
{

}

Vector3 LightRay::GetHitBaryCentric()
{
    const RTCRay& ray = d_->ray_;
    return Vector3 (ray.u, ray.v, 1.0f-ray.u-ray.v);
}

unsigned LightRay::GetHitTriangle()
{

}

}
