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

#include "Embree.h"

#include <Atomic/Core/Thread.h>
#include <Atomic/Core/Mutex.h>
#include <Atomic/Graphics/StaticModel.h>

#include "BakeMaterial.h"
#include "BakeModel.h"
#include "BakeNode.h"
#include "RadianceMap.h"

namespace AtomicGlow
{

using namespace Atomic;

class LightRay;
class SceneBaker;
class BakeLight;
class BakeMesh;

class BakeMesh : public BakeNode
{
    ATOMIC_OBJECT(BakeMesh, BakeNode)

    public:

    struct MMVertex
    {
        Vector3 position_;
        Vector3 normal_;
        Vector2 uv0_;
        Vector2 uv1_;
    };

    struct MMTriangle
    {
        // index into bakeMaterials_ vector
        unsigned materialIndex_;

        // index into vertices_ array
        unsigned indices_[3];
    };

    struct MMSample
    {
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


    BakeMesh(Context* context, SceneBaker* sceneBaker);
    virtual ~BakeMesh();

    bool SetStaticModel(StaticModel* staticModel);

    /// Get world space bounding box
    const BoundingBox& GetBoundingBox() const { return boundingBox_; }    
    const SharedArrayPtr<MMVertex>& GetVertices(unsigned& numVertices) const { numVertices = numVertices_; return vertices_; }
    const SharedArrayPtr<MMTriangle>& GetTriangles(unsigned numTriangles) const { numTriangles = numTriangles_; return triangles_; }

    void Preprocess();

    bool GetLightmap() const { return staticModel_.Null() ? false : staticModel_->GetLightmap(); }

    // would like this to be runnable on own thread
    void Light();

    void ContributeRadiance(const LightRay* lightRay, const Vector3 &radiance);

    int GetRadianceWidth() const { return radianceWidth_; }
    int GetRadianceHeight() const { return radianceHeight_; }

    inline bool GetRadiance(int x, int y, Vector3& rad, int& triIndex) const
    {
        rad = Vector3::ZERO;
        triIndex = -1;

        if (x < 0 || x >= radianceWidth_)
            return false;

        if (y < 0 || y >= radianceHeight_)
            return false;

        rad = radiance_[y * radianceWidth_ + x];

        triIndex = radianceTriIndices_[y * radianceWidth_ + x];

        if (triIndex < 0 || rad.x_ < 0.0f)
            return false;

        return true;
    }

    unsigned GetGeomID() const { return embreeGeomID_; }

    SharedPtr<RadianceMap> GetRadianceMap() const { return radianceMap_; }

    StaticModel* GetStaticModel() const { return staticModel_; }

    void Pack(unsigned lightmapIdx, Vector4 tilingOffset);

    const Color& GetAmbientColor() const { return ambientColor_; }

private:

    struct ShaderData
    {
        BakeMesh* bakeMesh_;
        unsigned triangleIdx_;
    };

    static bool FillLexelsCallback(void* param, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage);
    static void OcclusionFilter(void* ptr, RTCRay& ray);

    static void LightTrianglesWork(const WorkItem* item, unsigned threadIndex);
    void HandleLightTrianglesWorkCompleted(StringHash eventType, VariantMap& eventData);

    bool LightPixel(ShaderData* shaderData, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage);
    bool LightSample(MMSample* sample);

    void GenerateRadianceMap();

    // mesh geometry, in world space

    BoundingBox boundingBox_;

    SharedArrayPtr<MMVertex> vertices_;
    unsigned numVertices_;
    SharedArrayPtr<MMTriangle> triangles_;
    unsigned numTriangles_;

    SharedPtr<StaticModel> staticModel_;

    // resources
    PODVector<BakeMaterial*> bakeMaterials_;
    SharedPtr<BakeModel> bakeModel_;    

    // lights affecting this mesh
    PODVector<BakeLight*> bakeLights_;

    SharedPtr<RadianceMap> radianceMap_;

    // can be accessed from multiple threads
    SharedArrayPtr<Vector3> radiance_;
    // radiance -> triangle contributor
    SharedArrayPtr<int> radianceTriIndices_;

    unsigned radianceHeight_;
    unsigned radianceWidth_;

    unsigned embreeGeomID_;

    // multithreading

    Mutex meshMutex_;    
    unsigned numWorkItems_;

    Color ambientColor_;

};


}
