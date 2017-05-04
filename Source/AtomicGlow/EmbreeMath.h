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

#ifndef EMBREE_PRIVATE
#error EmbreeMath.h included outside of EmbreePrivate.h
#endif

#pragma once

#include <xmmintrin.h>
#include <pmmintrin.h>
#include <cmath>
#include <cfloat>

#include <ThirdParty/embree/common/math/vec2.h>
#include <ThirdParty/embree/common/math/vec3.h>
#include <ThirdParty/embree/common/math/vec4.h>
#include <ThirdParty/embree/common/math/linearspace3.h>

#include <Atomic/Math/Vector3.h>

// Embree math extensions

inline embree::Vec3fa ToVec3fa(const Atomic::Vector3& in)
{
    return embree::Vec3fa(in.x_, in.y_, in.z_);
}

namespace embree
{

struct DifferentialGeometry
{
  unsigned geomID;
  unsigned primID;
  float u,v;
  Vec3fa P;
  Vec3fa Ng;
  Vec3fa Ns;
  float tnear_eps;
};

/*! \brief utility library containing sampling functions */

// convention is to return the sample (Vec3fa) generated from given Vec2f 's'ample as last parameter
// sampling functions often come in pairs: sample and pdf (needed later for MIS)
// good reference is "Total Compendium" by Philip Dutre http://people.cs.kuleuven.be/~philip.dutre/GI/

inline Vec3fa cartesian(const float phi, const float sinTheta, const float cosTheta)
{
  float sinPhi, cosPhi;
  sincosf(phi, &sinPhi, &cosPhi);
  return Vec3fa(cosPhi * sinTheta,
                    sinPhi * sinTheta,
                    cosTheta);
}

inline Vec3fa cartesian(const float phi, const float cosTheta)
{
  return cartesian(phi, cos2sin(cosTheta), cosTheta);
}


/// cosine-weighted sampling of hemisphere oriented along the +z-axis
////////////////////////////////////////////////////////////////////////////////

inline Vec3fa cosineSampleHemisphere(const Vec2f s)
{
  const float phi =float(two_pi) * s.x;
  const float cosTheta = sqrt(s.y);
  const float sinTheta = sqrt(1.0f - s.y);
  return cartesian(phi, sinTheta, cosTheta);
}

inline float cosineSampleHemispherePDF(const Vec3fa &dir)
{
  return dir.z / float(pi);
}

inline float cosineSampleHemispherePDF(float cosTheta)
{
  return cosTheta / float(pi);
}


/// power cosine-weighted sampling of hemisphere oriented along the +z-axis
////////////////////////////////////////////////////////////////////////////////

inline Vec3fa powerCosineSampleHemisphere(const float n, const Vec2f &s)
{
  const float phi =float(two_pi) * s.x;
  const float cosTheta = pow(s.y, 1.0f / (n + 1.0f));
  return cartesian(phi, cosTheta);
}

inline float powerCosineSampleHemispherePDF(const float cosTheta, const float n) // TODO: order of arguments
{
  return (n + 1.0f) * (0.5f / float(pi)) * pow(cosTheta, n);
}

inline float powerCosineSampleHemispherePDF(const Vec3fa& dir, const float n) // TODO: order of arguments
{
  return (n + 1.0f) * (0.5f / float(pi)) * pow(dir.z, n);
}

/// sampling of cone of directions oriented along the +z-axis
////////////////////////////////////////////////////////////////////////////////

inline Vec3fa uniformSampleCone(const float cosAngle, const Vec2f &s)
{
  const float phi =float(two_pi) * s.x;
  const float cosTheta = 1.0f - s.y * (1.0f - cosAngle);
  return cartesian(phi, cosTheta);
}

inline float uniformSampleConePDF(const float cosAngle)
{
    return rcp(float(two_pi)*(1.0f - cosAngle));
}

inline float _uniformSampleConePDF(const float cosAngle)
{
    return rcp(float(two_pi)*(1.0f - cosAngle));
}


/// sampling of disk
////////////////////////////////////////////////////////////////////////////////

inline Vec3fa uniformSampleDisk(const float radius, const Vec2f &s)
{
  const float r = sqrtf(s.x) * radius;
  const float phi =float(two_pi) * s.y;
  float sinPhi, cosPhi;
  sincosf(phi, &sinPhi, &cosPhi);
  return Vec3fa(r * cosPhi, r * sinPhi, 0.f);
}

inline float uniformSampleDiskPDF(const float radius)
{
  return rcp(float(pi) * sqr(radius));
}

inline float _uniformSampleDiskPDF(const float radius)
{
  return rcp(float(pi) * sqr(radius));
}


/// sampling of triangle abc
////////////////////////////////////////////////////////////////////////////////

inline Vec3fa uniformSampleTriangle(const Vec3fa &a, const Vec3fa &b, const Vec3fa &c, const Vec2f &s)
{
  const float su = sqrtf(s.x);
  return c + (1.0f - su) * (a-c) + (s.y*su) * (b-c);
}

inline float uniformSampleTrianglePDF(const Vec3fa &a, const Vec3fa &b, const Vec3fa &c)
{
  return 2.0f * rcp(abs(length(cross(a-c, b-c))));
}

} // namespace embree

