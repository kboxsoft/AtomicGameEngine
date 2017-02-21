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

#include "GeomReplicator.h"

//=============================================================================
//=============================================================================
#define ONE_SEC_DURATION 1000

// wrap vert index visualization helper around preprocessor for optimization
#if defined(_DEBUG) || defined(DEBUG)
#define VERT_INDEX_VISUAL
#endif


namespace Atomic
{

//=============================================================================
//=============================================================================
unsigned GeomReplicator::Replicate(PODVector<PRotScale> &qplist, Vector3 &normalOverride)
{

    Geometry *pGeometry = GetModel()->GetGeometry(0, 0);
    VertexBuffer *pVbuffer = pGeometry->GetVertexBuffer(0);
    IndexBuffer *pIbuffer = pGeometry->GetIndexBuffer();
    unsigned uElementMask = pVbuffer->GetElementMask();
    unsigned vertexSize = pVbuffer->GetVertexSize();
    unsigned numVertices = pVbuffer->GetVertexCount();

    // for movement
    numVertsPerGeom = numVertices;

    // retain bbox as the size grows
    BoundingBox bbox;

    // cpy orig vbuffs
    unsigned origVertsBuffSize = vertexSize * numVertices;
    SharedArrayPtr<unsigned char> origVertBuff( new unsigned char[origVertsBuffSize] );
    const unsigned char *pVertexData = (const unsigned char*)pVbuffer->Lock(0, pVbuffer->GetVertexCount());

    if (pVertexData)
    {
        memcpy(origVertBuff.Get(), pVertexData, origVertsBuffSize);
        pVbuffer->Unlock();
    }

    // replicate
    pVbuffer->SetSize( numVertices * qplist.Size(), uElementMask );
    pVertexData = (unsigned char*)pVbuffer->Lock(0, pVbuffer->GetVertexCount());
    bool overrideNormal = normalOverride != Vector3::ZERO;

    if ( pVertexData )
    {
        for ( unsigned i = 0; i < qplist.Size(); ++i )
        {
            Quaternion rot(qplist[i].rot);
            Matrix3x4 mat(qplist[i].pos, rot, qplist[i].scale);
            unsigned begOfGeomAnimVertIndex = animatedVertexList_.Size();

            for ( unsigned j = 0; j < numVertices; ++j )
            {
                unsigned char *pOrigDataAlign = (unsigned char *)(origVertBuff.Get() + j * vertexSize);
                unsigned char *pDataAlign = (unsigned char *)(pVertexData + (i * numVertices + j) * vertexSize);
                unsigned sizeRemaining = vertexSize;

                // position
                const Vector3 &vPos = *reinterpret_cast<Vector3*>( pOrigDataAlign );
                Vector3 &nPos = *reinterpret_cast<Vector3*>( pDataAlign );
                nPos = mat * vPos;

                pOrigDataAlign += sizeof(Vector3);
                pDataAlign     += sizeof(Vector3);
                sizeRemaining  -= sizeof(Vector3);

                // for movement
                MoveAccumulator movPt;
                movPt.origPos = nPos;

                // sync timers for verts in the same geom
                if ( j > 0 )
                {
                    movPt.timeAccumlated = animatedVertexList_[begOfGeomAnimVertIndex].timeAccumlated;
                }
                animatedVertexList_.Push(movPt);

                // bbox
                bbox.Merge(nPos);

                // normal - let's not make any assumptions that the normals exist for every model
                if ( uElementMask & MASK_NORMAL )
                {
                    const Vector3 &vNorm = *reinterpret_cast<Vector3*>( pOrigDataAlign );
                    Vector3 &norm = *reinterpret_cast<Vector3*>( pDataAlign );

                    if ( !overrideNormal )
                    {
                        norm = rot * vNorm;
                    }
                    else
                    {
                        norm = normalOverride;
                    }

                    pOrigDataAlign += sizeof(Vector3);
                    pDataAlign     += sizeof(Vector3);
                    sizeRemaining  -= sizeof(Vector3);
                }
                
                // how about tangents?
                
                // copy everything else excluding what's copied already
                memcpy(pDataAlign, pOrigDataAlign, sizeRemaining);
            }
        }

        //unlock
        pVbuffer->Unlock();
    }

    // replicate indices
    unsigned newIdxCount = ReplicateIndices(pIbuffer, numVertices, qplist.Size());

    // set draw range and bounding box
    pGeometry->SetDrawRange(TRIANGLE_LIST, 0, newIdxCount);
    SetBoundingBox( bbox );

    return qplist.Size();
}



unsigned GeomReplicator::ReplicateIndices(IndexBuffer *idxbuffer, unsigned numVertices, unsigned expandSize)
{
    unsigned numIndices = idxbuffer->GetIndexCount();
    unsigned origIdxBuffSize = numIndices * sizeof(unsigned short);
    unsigned newIdxCount = expandSize * numIndices;
    bool isOver64k = newIdxCount > 1024*64;
    SharedArrayPtr<unsigned short> origIdxBuff( new unsigned short[numIndices] );

    // copy orig indices
    void *pIndexData = (void*)idxbuffer->Lock(0, idxbuffer->GetIndexCount());

    if (pIndexData)
    {
        memcpy(origIdxBuff.Get(), pIndexData, origIdxBuffSize);
        idxbuffer->Unlock();
    }

    // replicate indices
    if (isOver64k)
    {
        PODVector<int> newIndexList(newIdxCount);

        for (unsigned i = 0; i < expandSize; ++i)
        {
            for (unsigned j = 0; j < numIndices; ++j)
            {
                newIndexList[i*numIndices + j] = i*numVertices + origIdxBuff[j];
            }
        }

        idxbuffer->SetSize(newIdxCount, true);
        idxbuffer->SetData(&newIndexList[0]);
    }
    else
    {
        PODVector<unsigned short> newIndexList(newIdxCount);

        for (unsigned i = 0; i < expandSize; ++i)
        {
            for (unsigned j = 0; j < numIndices; ++j)
            {
                newIndexList[i*numIndices + j] = i*numVertices + origIdxBuff[j];
            }
        }

        idxbuffer->SetSize(newIdxCount, false);
        idxbuffer->SetData(&newIndexList[0]);
    }

    return newIdxCount;
}

bool GeomReplicator::ConfigWindVelocity(const PODVector<unsigned> &vertIndicesToMove, unsigned batchCount, 
                                        const Vector3 &velocity, float cycleTimer)
{
    vertIndicesToMove_ = vertIndicesToMove;
    windVelocity_      = velocity;
    cycleTimer_        = cycleTimer;
    batchCount_        = batchCount;
    currentVertexIdx_  = 0;
    timeStepAccum_     = 0.0f;

    // validate vert indices
    assert(vertIndicesToMove.Size() <= numVertsPerGeom && "number of indices to move is greater than the orig geom index size");

    for ( unsigned i = 0; i < vertIndicesToMove.Size(); ++i )
    {
        assert(vertIndicesToMove[i] < numVertsPerGeom && "vert index must be contained within the original geom size" );
    }

    timerUpdate_.Reset();
    return true;
}

void GeomReplicator::AnimateVerts()
{
    Geometry *pGeometry = GetModel()->GetGeometry(0, 0);
    VertexBuffer *pVbuffer = pGeometry->GetVertexBuffer(0);
    unsigned vertexSize = pVbuffer->GetVertexSize();
    unsigned vertsToMove = 0;

    // update animation
    for ( unsigned i = 0; i < batchCount_; ++i )
    {
        unsigned idx = (currentVertexIdx_ + i)*numVertsPerGeom;

        if (idx >= animatedVertexList_.Size() )
            break;

        vertsToMove++;

        for ( unsigned j = 0; j < vertIndicesToMove_.Size(); ++j )
        {
            unsigned vertIdx = idx + vertIndicesToMove_[j];

            int ielptime = animatedVertexList_[vertIdx].timer.GetMSec(true);
            if ( ielptime > MaxTime_Elapsed ) ielptime = MaxTime_Elapsed;

            float elapsedTime = (float)ielptime / 1000.0f;
            
            if ( !animatedVertexList_[vertIdx].reversing )
            {
                Vector3 delta = windVelocity_ * elapsedTime;
                animatedVertexList_[vertIdx].deltaMovement += delta;
                animatedVertexList_[vertIdx].timeAccumlated += elapsedTime;

                if ( animatedVertexList_[vertIdx].timeAccumlated > cycleTimer_ )
                {
                    animatedVertexList_[vertIdx].reversing = true;
                }
            }
            else
            {
                // slowed on reverse
                elapsedTime *= 0.5f;
                Vector3 delta = windVelocity_ * elapsedTime;
                animatedVertexList_[vertIdx].deltaMovement -= delta;
                animatedVertexList_[vertIdx].timeAccumlated -= elapsedTime;

                if ( animatedVertexList_[vertIdx].timeAccumlated < 0.0f )
                {
                    animatedVertexList_[vertIdx].deltaMovement = Vector3::ZERO;
                    animatedVertexList_[vertIdx].timeAccumlated = 0.0f;
                    animatedVertexList_[vertIdx].reversing = false;
                }
            }

            //// dbg text3d
            //#ifdef VERT_INDEX_VISUAL
            //if ( showGeomVertIndices_ && currentVertexIdx_ == 0 && i == 0 )
            //{
            //    Vector3 pos = animatedVertexList_[vertIdx].origPos + animatedVertexList_[vertIdx].deltaMovement;
            //    nodeText3DVertList_[vertIdx]->SetPosition(pos);
            //}
            //#endif
        }
    }

    // update vertex buffer
    unsigned char *pVertexData = (unsigned char*)pVbuffer->Lock(currentVertexIdx_ * numVertsPerGeom, vertsToMove * numVertsPerGeom);

    if ( pVertexData )
    {
        for ( unsigned i = 0; i < batchCount_; ++i )
        {
            unsigned idx = (currentVertexIdx_ + i)*numVertsPerGeom;

            if (idx >= animatedVertexList_.Size() )
                break;

            for ( unsigned j = 0; j < vertIndicesToMove_.Size(); ++j )
            {
                unsigned vertIdx = idx + vertIndicesToMove_[j];
                unsigned char *pDataAlign = (unsigned char *)(pVertexData + (i*numVertsPerGeom + vertIndicesToMove_[j]) * vertexSize );

                Vector3 &pos = *reinterpret_cast<Vector3*>( pDataAlign );
                pos = animatedVertexList_[vertIdx].origPos + animatedVertexList_[vertIdx].deltaMovement;
            }
        }

        pVbuffer->Unlock();
    }

    // update batch idx
    currentVertexIdx_ += batchCount_;

    if (currentVertexIdx_*numVertsPerGeom >= animatedVertexList_.Size()) 
    {
        currentVertexIdx_ = 0;
    }
}

void GeomReplicator::WindAnimationEnabled(bool enable)
{
    if (enable)
    {
        SubscribeToEvent(E_UPDATE, ATOMIC_HANDLER(GeomReplicator, HandleUpdate));
    }
    else
    {
        UnsubscribeFromEvent(E_UPDATE);
    }
}

//void GeomReplicator::ShowGeomVertIndices(bool show)
//{
//    #ifdef VERT_INDEX_VISUAL
//    showGeomVertIndices_ = show;
//
//    for ( unsigned i = 0; i < nodeText3DVertList_.Size(); ++i )
//    {
//        nodeText3DVertList_[i]->SetEnabled( showGeomVertIndices_ );
//    }
//    #endif
//}

//void GeomReplicator::RenderGeomVertIndices()
//{
//    #ifdef VERT_INDEX_VISUAL
//    if ( showGeomVertIndices_ )
//    {
//        DebugRenderer *dbgRenderer = GetScene()->GetComponent<DebugRenderer>();
//
//        for ( unsigned i = 1; i < nodeText3DVertList_.Size(); ++i )
//        {
//            dbgRenderer->AddLine( nodeText3DVertList_[i-1]->GetPosition(), nodeText3DVertList_[i]->GetPosition(), Color::GREEN );
//        }
//        dbgRenderer->AddLine( nodeText3DVertList_[0]->GetPosition(), nodeText3DVertList_[nodeText3DVertList_.Size()-1]->GetPosition(), Color::GREEN );
//    }
//    #endif
//}

void GeomReplicator::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    float timeStep = eventData[P_TIMESTEP].GetFloat();

    if ( timerUpdate_.GetMSec(false) >= FrameRate_MSec )
    {
        AnimateVerts();

        timerUpdate_.Reset();
    }

   // RenderGeomVertIndices();
}

}