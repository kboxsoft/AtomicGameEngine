// Copyright (c) 2014-2017, THUNDERBEAST GAMES LLC All rights reserved
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

#include <Atomic/Graphics/StaticModel.h>

#include "BakeMaterial.h"
#include "BakeModel.h"

namespace Atomic
{
    class Image;
}

using namespace Atomic;

namespace AtomicGlow
{

class SceneBaker;

class StaticModelBaker : public Object
{
    ATOMIC_OBJECT(StaticModelBaker, Object)

    public:

    StaticModelBaker(Context* context, SceneBaker* sceneBaker, StaticModel* staticModel);
    virtual ~StaticModelBaker();

    bool Preprocess();

    void TraceAORays(unsigned nsamples, float aoDepth, float multiply = 1.0f);
    void TraceSunLight();    

    void ProcessLightmap();

private:

    bool AddToEmbreeScene();

    static bool FillLexelsCallback(void* param, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage);    

    static void MyRTCFilterFunc(void* ptr, RTCRay& ray);

    struct Triangle
    {
        unsigned materialIndex_;
        Vector2 uv0_[3];
    };

    struct ShaderData
    {        
        StaticModelBaker* baker_;
        Triangle* triangle_;
        Vector3 triPositions_[3];
        Vector3 triNormals_[3];
        Vector3 faceNormal_;
        BakeMaterial* bakeMaterial_;
    };

    SharedPtr<Image> lightmap_;

    SharedArrayPtr<unsigned> indices_;    
    unsigned numIndices_;

    PODVector<Triangle> triangles_;

    PODVector<LMVertex> lmVertices_;
    PODVector<LMLexel> lmLexels_;

    SharedPtr<Node> node_;
    SharedPtr<StaticModel> staticModel_;
    SharedPtr<BakeModel> bakeModel_;

    WeakPtr<SceneBaker> sceneBaker_;

    uint32_t rtcGeomID_;

    // geometry bake materials
    PODVector<BakeMaterial*> bakeMaterials_;


};

}
