// ATOMIC NOTE:
// THIS CODE IN THIS FILE WAS NOT PART OF THE MAIN URHO3D CODEBASE 
// AT TIME OF WRITING BUT WAS CONTRIBUTED TO THE URHO3D FORUM
// BY LUMAK  WITH THE STANDARD URHO3D MIT LICENCE BELOW.

//
// Copyright (c) 2008-2016 the Urho3D project.
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

#include <Atomic/Core/CoreEvents.h>
#include <Atomic/Engine/Engine.h>
#include <Atomic/Graphics/Camera.h>
#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Graphics/Material.h>
#include <Atomic/Graphics/Model.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/Renderer.h>
#include <Atomic/Graphics/StaticModel.h>
#include <Atomic/Graphics/Geometry.h>
#include <Atomic/Graphics/VertexBuffer.h>
#include <Atomic/Graphics/IndexBuffer.h>
#include <Atomic/Container/ArrayPtr.h>
#include <Atomic/Input/Input.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Graphics/DebugRenderer.h>

//=============================================================================
//=============================================================================
#define ONE_SEC_DURATION 1000

// wrap vert index visualization helper around preprocessor for optimization
#if defined(_DEBUG) || defined(DEBUG)
#define VERT_INDEX_VISUAL
#endif


namespace Atomic
{

class Node;
class Scene;
class Text3D;

//=============================================================================
//=============================================================================
struct PRotScale
{
    Vector3     pos;
    Quaternion  rot;
    float       scale;
};

class GeomReplicator : public StaticModel
{
    ATOMIC_OBJECT(GeomReplicator, StaticModel);

    struct MoveAccumulator
    {
        MoveAccumulator() 
            : origPos(Vector3::ZERO), deltaMovement(Vector3::ZERO), reversing(false)
        {
            timeAccumlated = Random() * 0.2f;
        }

        Vector3 origPos;
        Vector3 deltaMovement;
        Timer   timer;
        float   timeAccumlated;
        bool    reversing;
    };

public:
    static void RegisterObject(Context* context)
    {
        context->RegisterFactory<GeomReplicator>();
    }

	GeomReplicator(Context *context)
		: StaticModel(context), numVertsPerGeom(0), batchCount_(0)//, showGeomVertIndices_(false)
    {
    }

    virtual ~GeomReplicator()
    {
		cycleTimer_ = 0;
    }

    unsigned Replicate( PODVector<PRotScale> &qplist,  Vector3 &normalOverride);
    bool ConfigWindVelocity(const PODVector<unsigned> &vertIndicesToMove, unsigned batchCount, 
                            const Vector3 &velocity, float cycleTimer);
    void WindAnimationEnabled(bool enable);
   // void ShowGeomVertIndices(bool show);

protected:
    unsigned ReplicateIndices(IndexBuffer *idxbuffer, unsigned numVertices, unsigned expandSize);
    void AnimateVerts();
   // void RenderGeomVertIndices();
    void HandleUpdate(StringHash eventType, VariantMap& eventData);

protected:
    PODVector<MoveAccumulator>  animatedVertexList_;
    PODVector<unsigned>         vertIndicesToMove_;

    unsigned                    numVertsPerGeom;
    unsigned                    batchCount_;
    unsigned                    currentVertexIdx_;
    Timer                       timerUpdate_;
    Vector3                     windVelocity_;
    float                       cycleTimer_;
    float                       timeStepAccum_;

    // dbg
  //  Vector<Node*>               nodeText3DVertList_;
 //   bool                        showGeomVertIndices_;

protected:
    enum FrameRateType { FrameRate_MSec = 32    };
    enum MaxTimeType   { MaxTime_Elapsed = 1000 };
};
}