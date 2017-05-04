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

#include "EmbreePrivate.h"

#include <Atomic/IO/Log.h>

#include "BakeMesh.h"
#include "EmbreeScene.h"

namespace AtomicGlow
{

static void RTCErrorCallback(const RTCError code, const char* str)
{
    ATOMIC_LOGERRORF("RTC Error %d: %s", code, str);
}


EmbreeScenePrivate::EmbreeScenePrivate(EmbreeScene* embreeScene):
    embreeScene_(embreeScene),
    rtcDevice_(0),
    rtcScene_(0)
{

    // Intel says to do this, so we're doing it.
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    // Create the embree device and scene.
    rtcDevice_ = rtcNewDevice(NULL);

    rtcDeviceSetErrorFunction(rtcDevice_, RTCErrorCallback);

    rtcScene_ = rtcDeviceNewScene(rtcDevice_, RTC_SCENE_STATIC, RTC_INTERSECT1);
}


void EmbreeScene::Commit()
{
    rtcCommit(d_->rtcScene_);
}

bool EmbreeScene::AddMeshMap(BakeMesh* meshMap)
{
    RTCScene rtcScene = d_->rtcScene_;

    unsigned numTriangles;
    unsigned numVertices;

    const SharedArrayPtr<BakeMesh::MMTriangle> triangles = meshMap->GetTriangles(numTriangles);
    const SharedArrayPtr<BakeMesh::MMVertex> vertices = meshMap->GetVertices(numVertices);

    if (!numTriangles || !numVertices)
        return false;

    // Create the embree mesh
    unsigned rtcGeomID = rtcNewTriangleMesh(rtcScene, RTC_GEOMETRY_STATIC, numTriangles, numVertices);
    rtcSetUserData(rtcScene, rtcGeomID, meshMap);

    //rtcSetOcclusionFilterFunction(scene, rtcGeomID, MyRTCFilterFunc);

    // Populate vertices

    float* vOut = (float*) rtcMapBuffer(rtcScene, rtcGeomID, RTC_VERTEX_BUFFER);

    for (unsigned i = 0; i < numVertices; i++)
    {
        const BakeMesh::MMVertex& v = vertices[i];

        *vOut++ = v.position_.x_;
        *vOut++ = v.position_.y_;
        *vOut++ = v.position_.z_;

        // buffer is 16 byte aligned
        vOut++;
    }

    rtcUnmapBuffer(rtcScene, rtcGeomID, RTC_VERTEX_BUFFER);

    uint32_t* triOut = (uint32_t*) rtcMapBuffer(rtcScene, rtcGeomID, RTC_INDEX_BUFFER);

    for (size_t i = 0; i < numTriangles; i++)
    {
        const BakeMesh::MMTriangle& tri = triangles[i];

        *triOut++ = tri.indices_[0];
        *triOut++ = tri.indices_[1];
        *triOut++ = tri.indices_[2];
    }

    rtcUnmapBuffer(rtcScene, rtcGeomID, RTC_INDEX_BUFFER);

    meshMapLookup_[rtcGeomID] = meshMap;

    return true;

}

EmbreeScene::EmbreeScene(Context* context) : Object(context)
{
    d_ = new EmbreeScenePrivate(this);
}

EmbreeScene::~EmbreeScene()
{
    delete d_;
}

}
