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

#pragma once

#include "BakeNode.h"

namespace Atomic
{
    class Light;
}

using namespace Atomic;

namespace AtomicGlow
{

class LightRay;
class BakeMesh;

class BakeLight : public BakeNode
{
    ATOMIC_OBJECT(BakeLight, BakeNode)

    public:

    BakeLight(Context* context, SceneBaker *sceneBaker);
    virtual ~BakeLight();

    virtual void SetLight(Atomic::Light* light) = 0;

    virtual void Light(LightRay* ray) = 0;

protected:

    /*

    struct SampleResult
    {
      // radiance that arrives at the given point divided by pdf
      embree::Vec3fa weight;
      // largest valid t_far value for a shadow ray
      embree::Vec3fa dir;
      // largest valid t_far value for a shadow ray
      float dist;
      // probability density that this sample was taken
      float pdf;
    };

    struct EvalResult
    {
      // radiance that arrives at the given point (not weighted by pdf)
      embree::Vec3fa value;
      float dist;
      // probability density that the direction would have been sampled
      float pdf;
    };

    /// Compute the weighted radiance at a point caused by a sample on the light source
    /// by convention, giving (0, 0) as "random" numbers should sample the "center" of the light source
    virtual void Sample(SampleResult& result, const embree::DifferentialGeometry& dg, const embree::Vec2f& s) = 0;

    /// Compute the radiance, distance and pdf caused by the light source (pointed to by the given direction)
    virtual void Eval(EvalResult& result, const embree::DifferentialGeometry& dg, const embree::Vec3fa& dir) = 0;
    */

private:

};

class BakeLightDirectional : public BakeLight
{
    ATOMIC_OBJECT(BakeLightDirectional, BakeLight)

    public:

    BakeLightDirectional(Context* context, SceneBaker* sceneBaker);
    virtual ~BakeLightDirectional();

    void Set(const Vector3& direction, const Vector3& radiance, float cosAngle);

    void Light(LightRay* lightRay);

    void SetLight(Atomic::Light* light);


protected:

    Vector3 direction_;

    /*
    void Sample(SampleResult& result, const embree::DifferentialGeometry& dg, const embree::Vec2f &s);

    void Eval(EvalResult& result, const embree::DifferentialGeometry& dg, const embree::Vec3fa& dir);
    */

private:

    /*
    // Coordinate frame, with vz == direction *towards* the light source
    embree::LinearSpace3fa frame_;
    // RGB color and intensity of light
    embree::Vec3fa radiance_;
    // Angular limit of the cone light in an easier to use form: cosine of the half angle in radians
    float cosAngle_;
    // Probability to sample a direction to the light
    float pdf_;
    */

};

}
