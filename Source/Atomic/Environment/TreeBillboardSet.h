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

#include "../Graphics/Drawable.h"
#include "../IO/VectorBuffer.h"
#include "../Math/Color.h"
#include "../Math/Matrix3x4.h"
#include "../Math/Rect.h"

namespace Atomic
{

class IndexBuffer;
class VertexBuffer;

/// One TreeBillboard in the TreeBillboard set.
class ATOMIC_API TreeBillboard : public RefCounted
{
    friend class TreeBillboardSet;
    friend class ParticleEmitter;

    ATOMIC_REFCOUNTED(TreeBillboard);

public:
    TreeBillboard();
    virtual ~TreeBillboard();

    const Vector3& GetPosition() const { return position_; }
    void SetPosition(const Vector3 &position) { position_ = position; }

    const Vector2 GetSize() const { return size_; }
    void SetSize(const Vector2 &size) { size_ = size; }

    const Rect& GetUV() const { return uv_; }
    void SetUV(const Rect &uv) { uv_ = uv; }

    const Color& GetColor() const { return color_; }
    void SetColor(const Color &color) { color_ = color; }

    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

    const Vector3& GetDirection() const { return direction_; }
    void SetDirection(const Vector3& direction) { direction_ = direction; }

    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    float GetSortDistance() const { return sortDistance_; }
    void SetSortDistance(float sortDistance) { sortDistance_ = sortDistance; }

    /// Position.
    Vector3 position_;
    /// Two-dimensional size. If TreeBillboardSet has fixed screen size enabled, this is measured in pixels instead of world units.
    Vector2 size_;
    /// UV coordinates.
    Rect uv_;
    /// Color.
    Color color_;
    /// Rotation.
    float rotation_;
    /// Direction (For direction based TreeBillboard only).
    Vector3 direction_;
    /// Enabled flag.
    bool enabled_;
    /// Sort distance. Used internally.
    float sortDistance_;
    /// Scale factor for fixed screen size mode. Used internally.
    float screenScaleFactor_;
};


static const unsigned MAX_TREEBILLBOARDS = 65536 / 4;

/// %TreeBillboard component.
class ATOMIC_API TreeBillboardSet : public Drawable
{
    ATOMIC_OBJECT(TreeBillboardSet, Drawable);

public:
    /// Construct.
    TreeBillboardSet(Context* context);
    /// Destruct.
    virtual ~TreeBillboardSet();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Process octree raycast. May be called from a worker thread.
    virtual void ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results);
    /// Calculate distance and prepare batches for rendering. May be called from worker thread(s), possibly re-entrantly.
    virtual void UpdateBatches(const FrameInfo& frame);
    /// Prepare geometry for rendering. Called from a worker thread if possible (no GPU update.)
    virtual void UpdateGeometry(const FrameInfo& frame);
    /// Return whether a geometry update is necessary, and if it can happen in a worker thread.
    virtual UpdateGeometryType GetUpdateGeometryType();

    /// Set material.
    void SetMaterial(Material* material);
    /// Set number of TreeBillboards.
    void SetNumTreeBillboards(unsigned num);
    /// Set whether TreeBillboards are relative to the scene node. Default true.
    void SetRelative(bool enable);
    /// Set whether scene node scale affects TreeBillboards' size. Default true.
    void SetScaled(bool enable);
    /// Set whether TreeBillboards are sorted by distance. Default false.
    void SetSorted(bool enable);
    /// Set whether TreeBillboards have fixed size on screen (measured in pixels) regardless of distance to camera. Default false.
    void SetFixedScreenSize(bool enable);
    /// Set how the TreeBillboards should rotate in relation to the camera. Default is to follow camera rotation on all axes (FC_ROTATE_XYZ.)
    void SetFaceCameraMode(FaceCameraMode mode);
    /// Set minimal angle between TreeBillboard normal and look-at direction.
    void SetMinAngle(float angle);
    /// Set animation LOD bias.
    void SetAnimationLodBias(float bias);
    /// Mark for bounding box and vertex buffer update. Call after modifying the TreeBillboards.
    void Commit();

    /// Return material.
    Material* GetMaterial() const;

    /// Return number of TreeBillboards.
    unsigned GetNumTreeBillboards() const { return TreeBillboards_.Size(); }

    /// Return all TreeBillboards.
    Vector<SharedPtr<TreeBillboard>>& GetTreeBillboards() { return TreeBillboards_; }

    /// Return TreeBillboard by index.
    TreeBillboard* GetTreeBillboard(unsigned index);

    /// Return whether TreeBillboards are relative to the scene node.
    bool IsRelative() const { return relative_; }

    /// Return whether scene node scale affects TreeBillboards' size.
    bool IsScaled() const { return scaled_; }

    /// Return whether TreeBillboards are sorted.
    bool IsSorted() const { return sorted_; }

    /// Return whether TreeBillboards are fixed screen size.
    bool IsFixedScreenSize() const { return fixedScreenSize_; }

    /// Return how the TreeBillboards rotate in relation to the camera.
    FaceCameraMode GetFaceCameraMode() const { return faceCameraMode_; }

    /// Return minimal angle between TreeBillboard normal and look-at direction.
    float GetMinAngle() const { return minAngle_; }

    /// Return animation LOD bias.
    float GetAnimationLodBias() const { return animationLodBias_; }

    /// Set material attribute.
    void SetMaterialAttr(const ResourceRef& value);
    /// Set TreeBillboards attribute.
    void SetTreeBillboardsAttr(const VariantVector& value);
    /// Set TreeBillboards attribute for network replication.
    void SetNetTreeBillboardsAttr(const PODVector<unsigned char>& value);
    /// Return material attribute.
    ResourceRef GetMaterialAttr() const;
    /// Return TreeBillboards attribute.
    VariantVector GetTreeBillboardsAttr() const;
    /// Return TreeBillboards attribute for network replication.
    const PODVector<unsigned char>& GetNetTreeBillboardsAttr() const;

protected:
    /// Recalculate the world-space bounding box.
    virtual void OnWorldBoundingBoxUpdate();
    /// Mark TreeBillboard vertex buffer to need an update.
    void MarkPositionsDirty();

    /// TreeBillboards.
    // ATOMIC BEGIN
    Vector<SharedPtr<TreeBillboard>> TreeBillboards_;
    // ATOMIC END
    /// Animation LOD bias.
    float animationLodBias_;
    /// Animation LOD timer.
    float animationLodTimer_;
    /// TreeBillboards relative flag.
    bool relative_;
    /// Scale affects TreeBillboard scale flag.
    bool scaled_;
    /// TreeBillboards sorted flag.
    bool sorted_;
    /// TreeBillboards fixed screen size flag.
    bool fixedScreenSize_;
    /// TreeBillboard rotation mode in relation to the camera.
    FaceCameraMode faceCameraMode_;
    /// Minimal angle between TreeBillboard normal and look-at direction.
    float minAngle_;

private:
    /// Resize TreeBillboard vertex and index buffers.
    void UpdateBufferSize();
    /// Rewrite TreeBillboard vertex buffer.
    void UpdateVertexBuffer(const FrameInfo& frame);
    /// Calculate TreeBillboard scale factors in fixed screen size mode.
    void CalculateFixedScreenSize(const FrameInfo& frame);

    /// Geometry.
    SharedPtr<Geometry> geometry_;
    /// Vertex buffer.
    SharedPtr<VertexBuffer> vertexBuffer_;
    /// Index buffer.
    SharedPtr<IndexBuffer> indexBuffer_;
    /// Transform matrices for position and TreeBillboard orientation.
    Matrix3x4 transforms_[2];
    /// Buffers need resize flag.
    bool bufferSizeDirty_;
    /// Vertex buffer needs rewrite flag.
    bool bufferDirty_;
    /// Force update flag (ignore animation LOD momentarily.)
    bool forceUpdate_;
    /// Update TreeBillboard geometry type
    bool geometryTypeUpdate_;
    /// Sorting flag. Triggers a vertex buffer rewrite for each view this TreeBillboard set is rendered from.
    bool sortThisFrame_;
    /// Whether was last rendered from an ortho camera.
    bool hasOrthoCamera_;
    /// Frame number on which was last sorted.
    unsigned sortFrameNumber_;
    /// Previous offset to camera for determining whether sorting is necessary.
    Vector3 previousOffset_;
    /// TreeBillboard pointers for sorting.
    Vector<TreeBillboard*> sortedTreeBillboards_;
    /// Attribute buffer for network replication.
    mutable VectorBuffer attrBuffer_;
};

}
