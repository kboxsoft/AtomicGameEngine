
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

using namespace Atomic;

namespace AtomicGlow
{

class EmbreeRayPrivate;

class BakeMesh;

class LightRay
{

public:

    LightRay();
    ~LightRay();

    struct SampleSource
    {
        SampleSource()
        {
            Clear();
        }

        void Clear()
        {
            bakeMesh = 0;
            triangle = 0;
            radianceX = 0;
            radianceY = 0;

            position = normal = Vector3::ZERO;
            uv0 = uv0 = Vector2::ZERO;
        }

        BakeMesh* bakeMesh;
        unsigned triangle;

        // coords in radiance_
        unsigned radianceX;
        unsigned radianceY;

        Vector3 position;
        Vector3 normal;
        Vector2 uv0;
        Vector2 uv1;
    };

    SampleSource& GetSampleSource() { return sampleSrc_; }

    // Ray origin
    void SetOrigin(const Vector3& origin);
    const Vector3 GetOrigin() const;

    // Ray direction
    void SetDir(const Vector3& dir);
    const Vector3 GetDir() const;

    // Start of ray segment
    void SetTNear(float tnear);
    float GetTNear() const;

    // End of ray segment (set to hit distance)
    void SetTFar(float tfar);
    float GetTFar() const;

    BakeMesh* GetHitMesh();

    Vector3 GetHitNormal();
    Vector3 GetHitBaryCentric();

    unsigned GetHitTriangle();

    void Clear();

    void ClearHit();

    EmbreeRayPrivate* d_;

    SampleSource sampleSrc_;

};

}
