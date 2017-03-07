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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../Graphics/Batch.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/MemoryBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Node.h"

#include "../DebugNew.h"
#include "TreeBillboardSet.h"

namespace Atomic
{

extern const char* GEOMETRY_CATEGORY;

static const float INV_SQRT_TWO = 1.0f / sqrtf(2.0f);

const char* treeFaceCameraModeNames[] =
{
    "None",
    "Rotate XYZ",
    "Rotate Y",
    "LookAt XYZ",
    "LookAt Y",
    "LookAt Mixed",
    "Direction",
    0
};

inline bool CompareTreeBillboards(TreeBillboard* lhs, TreeBillboard* rhs)
{
    return lhs->sortDistance_ > rhs->sortDistance_;
}

TreeBillboard::TreeBillboard()
{

}

TreeBillboard::~TreeBillboard()
{

}

TreeBillboardSet::TreeBillboardSet(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    animationLodBias_(1.0f),
    animationLodTimer_(0.0f),
    relative_(true),
    scaled_(true),
    sorted_(false),
    fixedScreenSize_(false),
    faceCameraMode_(FC_ROTATE_XYZ),
    minAngle_(0.0f),
    geometry_(new Geometry(context)),
    vertexBuffer_(new VertexBuffer(context_)),
    indexBuffer_(new IndexBuffer(context_)),
    bufferSizeDirty_(true),
    bufferDirty_(true),
    forceUpdate_(false),
    geometryTypeUpdate_(false),
    sortThisFrame_(false),
    hasOrthoCamera_(false),
    sortFrameNumber_(0),
    previousOffset_(Vector3::ZERO)
{
    geometry_->SetVertexBuffer(0, vertexBuffer_);
    geometry_->SetIndexBuffer(indexBuffer_);

    batches_.Resize(1);
    batches_[0].geometry_ = geometry_;
    batches_[0].geometryType_ = GEOM_BILLBOARD;
    batches_[0].worldTransform_ = &transforms_[0];
}

TreeBillboardSet::~TreeBillboardSet()
{
}

void TreeBillboardSet::RegisterObject(Context* context)
{
    context->RegisterFactory<TreeBillboardSet>(GEOMETRY_CATEGORY);

    ATOMIC_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    ATOMIC_MIXED_ACCESSOR_ATTRIBUTE("Material", GetMaterialAttr, SetMaterialAttr, ResourceRef, ResourceRef(Material::GetTypeStatic()),
        AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Relative Position", IsRelative, SetRelative, bool, true, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Relative Scale", IsScaled, SetScaled, bool, true, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Sort By Distance", IsSorted, SetSorted, bool, false, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Fixed Screen Size", IsFixedScreenSize, SetFixedScreenSize, bool, false, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
    ATOMIC_ATTRIBUTE("Cast Shadows", bool, castShadows_, false, AM_DEFAULT);
    ATOMIC_ENUM_ACCESSOR_ATTRIBUTE("Face Camera Mode", GetFaceCameraMode, SetFaceCameraMode, FaceCameraMode, treeFaceCameraModeNames, FC_ROTATE_XYZ, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Min Angle", GetMinAngle, SetMinAngle, float, 0.0f, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Shadow Distance", GetShadowDistance, SetShadowDistance, float, 0.0f, AM_DEFAULT);
    ATOMIC_ACCESSOR_ATTRIBUTE("Animation LOD Bias", GetAnimationLodBias, SetAnimationLodBias, float, 1.0f, AM_DEFAULT);
    ATOMIC_COPY_BASE_ATTRIBUTES(Drawable);
    ATOMIC_MIXED_ACCESSOR_ATTRIBUTE("TreeBillboards", GetTreeBillboardsAttr, SetTreeBillboardsAttr, VariantVector, Variant::emptyVariantVector,
        AM_FILE);
    ATOMIC_ACCESSOR_ATTRIBUTE("Network TreeBillboards", GetNetTreeBillboardsAttr, SetNetTreeBillboardsAttr, PODVector<unsigned char>,
        Variant::emptyBuffer, AM_NET | AM_NOEDIT);
}

void TreeBillboardSet::ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results)
{
    // If no TreeBillboard-level testing, use the Drawable test
    if (query.level_ < RAY_TRIANGLE)
    {
        Drawable::ProcessRayQuery(query, results);
        return;
    }

    // Check ray hit distance to AABB before proceeding with TreeBillboard-level tests
    if (query.ray_.HitDistance(GetWorldBoundingBox()) >= query.maxDistance_)
        return;

    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    Matrix3x4 TreeBillboardTransform = relative_ ? worldTransform : Matrix3x4::IDENTITY;
    Vector3 TreeBillboardScale = scaled_ ? worldTransform.Scale() : Vector3::ONE;

    for (unsigned i = 0; i < TreeBillboards_.Size(); ++i)
    {
        if (!TreeBillboards_[i]->enabled_)
            continue;

        // Approximate the TreeBillboards as spheres for raycasting
        float size = INV_SQRT_TWO * (TreeBillboards_[i]->size_.x_ * TreeBillboardScale.x_ + TreeBillboards_[i]->size_.y_ * TreeBillboardScale.y_);
        if (fixedScreenSize_)
            size *= TreeBillboards_[i]->screenScaleFactor_;
        Vector3 center = TreeBillboardTransform * TreeBillboards_[i]->position_;
        Sphere TreeBillboardSphere(center, size);

        float distance = query.ray_.HitDistance(TreeBillboardSphere);
        if (distance < query.maxDistance_)
        {
            // If the code reaches here then we have a hit
            RayQueryResult result;
            result.position_ = query.ray_.origin_ + distance * query.ray_.direction_;
            result.normal_ = -query.ray_.direction_;
            result.distance_ = distance;
            result.drawable_ = this;
            result.node_ = node_;
            result.subObject_ = i;
            results.Push(result);
        }
    }
}

void TreeBillboardSet::UpdateBatches(const FrameInfo& frame)
{
    // If beginning a new frame, assume no sorting first
    if (frame.frameNumber_ != sortFrameNumber_)
    {
        sortThisFrame_ = false;
        sortFrameNumber_ = frame.frameNumber_;
    }

    Vector3 worldPos = node_->GetWorldPosition();
    Vector3 offset = (worldPos - frame.camera_->GetNode()->GetWorldPosition());
    // Sort if position relative to camera has changed
    if (offset != previousOffset_ || frame.camera_->IsOrthographic() != hasOrthoCamera_)
    {
        if (sorted_)
            sortThisFrame_ = true;
        if (faceCameraMode_ == FC_DIRECTION)
            bufferDirty_ = true;

        hasOrthoCamera_ = frame.camera_->IsOrthographic();

        // Calculate fixed screen size scale factor for TreeBillboards if needed
        if (fixedScreenSize_)
            CalculateFixedScreenSize(frame);
    }

    distance_ = frame.camera_->GetDistance(GetWorldBoundingBox().Center());

    // Calculate scaled distance for animation LOD
    float scale = GetWorldBoundingBox().Size().DotProduct(DOT_SCALE);
    // If there are no TreeBillboards, the size becomes zero, and LOD'ed updates no longer happen. Disable LOD in that case
    if (scale > M_EPSILON)
        lodDistance_ = frame.camera_->GetLodDistance(distance_, scale, lodBias_);
    else
        lodDistance_ = 0.0f;

    batches_[0].distance_ = distance_;
    batches_[0].numWorldTransforms_ = 2;
    // TreeBillboard positioning
    transforms_[0] = relative_ ? node_->GetWorldTransform() : Matrix3x4::IDENTITY;
    // TreeBillboard rotation
    transforms_[1] = Matrix3x4(Vector3::ZERO, faceCameraMode_ != FC_NONE ? frame.camera_->GetFaceCameraRotation(
        node_->GetWorldPosition(), node_->GetWorldRotation(), faceCameraMode_, minAngle_) : node_->GetWorldRotation(), Vector3::ONE);
}

void TreeBillboardSet::UpdateGeometry(const FrameInfo& frame)
{
    // If rendering from multiple views and fixed screen size is in use, re-update scale factors before each render
    if (fixedScreenSize_ && viewCameras_.Size() > 1)
        CalculateFixedScreenSize(frame);

    // If using camera facing, re-update the rotation for the current view now
    if (faceCameraMode_ != FC_NONE)
    {
        transforms_[1] = Matrix3x4(Vector3::ZERO, frame.camera_->GetFaceCameraRotation(node_->GetWorldPosition(),
            node_->GetWorldRotation(), faceCameraMode_, minAngle_), Vector3::ONE);
    }

    if (bufferSizeDirty_ || indexBuffer_->IsDataLost())
        UpdateBufferSize();

    if (bufferDirty_ || sortThisFrame_ || vertexBuffer_->IsDataLost())
        UpdateVertexBuffer(frame);
}

UpdateGeometryType TreeBillboardSet::GetUpdateGeometryType()
{
    // If using camera facing, always need some kind of geometry update, in case the TreeBillboard set is rendered from several views
    if (bufferDirty_ || bufferSizeDirty_ || vertexBuffer_->IsDataLost() || indexBuffer_->IsDataLost() || sortThisFrame_ ||
        faceCameraMode_ != FC_NONE || fixedScreenSize_)
        return UPDATE_MAIN_THREAD;
    else
        return UPDATE_NONE;
}

void TreeBillboardSet::SetMaterial(Material* material)
{
    batches_[0].material_ = material;
    MarkNetworkUpdate();
}

void TreeBillboardSet::SetNumTreeBillboards(unsigned num)
{
    // Prevent negative value being assigned from the editor
    if (num > M_MAX_INT)
        num = 0;
    if (num > MAX_TREEBILLBOARDS)
        num = MAX_TREEBILLBOARDS;

    unsigned oldNum = TreeBillboards_.Size();
    if (num == oldNum)
        return;

    TreeBillboards_.Resize(num);

    // Set default values to new TreeBillboards
    for (unsigned i = oldNum; i < num; ++i)
    {
        TreeBillboard *bb = new TreeBillboard();
        TreeBillboards_[i] = bb;
        bb->position_ = Vector3::ZERO;
        bb->size_ = Vector2::ONE;
        bb->uv_ = Rect::POSITIVE;
        bb->color_ = Color(1.0f, 1.0f, 1.0f);
        bb->rotation_ = 0.0f;
        bb->direction_ = Vector3::UP;
        bb->enabled_ = false;
        bb->screenScaleFactor_ = 1.0f;
    }

    bufferSizeDirty_ = true;
    Commit();
}

void TreeBillboardSet::SetRelative(bool enable)
{
    relative_ = enable;
    Commit();
}

void TreeBillboardSet::SetScaled(bool enable)
{
    scaled_ = enable;
    Commit();
}

void TreeBillboardSet::SetSorted(bool enable)
{
    sorted_ = enable;
    Commit();
}

void TreeBillboardSet::SetFixedScreenSize(bool enable)
{
    fixedScreenSize_ = enable;
    Commit();
}

void TreeBillboardSet::SetFaceCameraMode(FaceCameraMode mode)
{
    if ((faceCameraMode_ != FC_DIRECTION && mode == FC_DIRECTION) || (faceCameraMode_ == FC_DIRECTION && mode != FC_DIRECTION))
    {
        faceCameraMode_ = mode;
        if (faceCameraMode_ == FC_DIRECTION)
            batches_[0].geometryType_ = GEOM_DIRBILLBOARD;
        else
            batches_[0].geometryType_ = GEOM_BILLBOARD;
        geometryTypeUpdate_ = true;
        bufferSizeDirty_ = true;
        Commit();
    }
    else
    {
        faceCameraMode_ = mode;
        MarkNetworkUpdate();
    }
}

void TreeBillboardSet::SetMinAngle(float angle)
{
    minAngle_ = angle;
    MarkNetworkUpdate();
}

void TreeBillboardSet::SetAnimationLodBias(float bias)
{
    animationLodBias_ = Max(bias, 0.0f);
    MarkNetworkUpdate();
}

void TreeBillboardSet::Commit()
{
    MarkPositionsDirty();
    MarkNetworkUpdate();
}

Material* TreeBillboardSet::GetMaterial() const
{
    return batches_[0].material_;
}

TreeBillboard* TreeBillboardSet::GetTreeBillboard(unsigned index)
{
    return index < TreeBillboards_.Size() ? TreeBillboards_[index] : (TreeBillboard*)0;
}

void TreeBillboardSet::SetMaterialAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    SetMaterial(cache->GetResource<Material>(value.name_));
}

void TreeBillboardSet::SetTreeBillboardsAttr(const VariantVector& value)
{
    unsigned index = 0;
    unsigned numTreeBillboards = index < value.Size() ? value[index++].GetUInt() : 0;
    SetNumTreeBillboards(numTreeBillboards);

    // Dealing with old TreeBillboard format
    if (value.Size() == TreeBillboards_.Size() * 6 + 1)
    {
        for (Vector<SharedPtr<TreeBillboard>>::Iterator ii = TreeBillboards_.Begin(); ii != TreeBillboards_.End() && index < value.Size(); ++ii)
        {
            TreeBillboard* i = *ii;
            i->position_ = value[index++].GetVector3();
            i->size_ = value[index++].GetVector2();
            Vector4 uv = value[index++].GetVector4();
            i->uv_ = Rect(uv.x_, uv.y_, uv.z_, uv.w_);
            i->color_ = value[index++].GetColor();
            i->rotation_ = value[index++].GetFloat();
            i->enabled_ = value[index++].GetBool();
        }
    }
    // New TreeBillboard format
    else
    {
        for (Vector<SharedPtr<TreeBillboard>>::Iterator ii = TreeBillboards_.Begin(); ii != TreeBillboards_.End() && index < value.Size(); ++ii)
        {
            TreeBillboard* i = *ii;
            i->position_ = value[index++].GetVector3();
            i->size_ = value[index++].GetVector2();
            Vector4 uv = value[index++].GetVector4();
            i->uv_ = Rect(uv.x_, uv.y_, uv.z_, uv.w_);
            i->color_ = value[index++].GetColor();
            i->rotation_ = value[index++].GetFloat();
            i->direction_ = value[index++].GetVector3();
            i->enabled_ = value[index++].GetBool();
        }
    }

    Commit();
}

void TreeBillboardSet::SetNetTreeBillboardsAttr(const PODVector<unsigned char>& value)
{
    MemoryBuffer buf(value);
    unsigned numTreeBillboards = buf.ReadVLE();
    SetNumTreeBillboards(numTreeBillboards);

    for (Vector<SharedPtr<TreeBillboard>>::Iterator ii = TreeBillboards_.Begin(); ii != TreeBillboards_.End(); ++ii)
    {
        TreeBillboard* i = *ii;
        i->position_ = buf.ReadVector3();
        i->size_ = buf.ReadVector2();
        i->uv_ = buf.ReadRect();
        i->color_ = buf.ReadColor();
        i->rotation_ = buf.ReadFloat();
        i->direction_ = buf.ReadVector3();
        i->enabled_ = buf.ReadBool();
    }

    Commit();
}

ResourceRef TreeBillboardSet::GetMaterialAttr() const
{
    return GetResourceRef(batches_[0].material_, Material::GetTypeStatic());
}

VariantVector TreeBillboardSet::GetTreeBillboardsAttr() const
{
    VariantVector ret;
    ret.Reserve(TreeBillboards_.Size() * 7 + 1);
    ret.Push(TreeBillboards_.Size());

    for (Vector<SharedPtr<TreeBillboard>>::ConstIterator ii = TreeBillboards_.Begin(); ii != TreeBillboards_.End(); ++ii)
    {
        const TreeBillboard* i = *ii;
        ret.Push(i->position_);
        ret.Push(i->size_);
        ret.Push(Vector4(i->uv_.min_.x_, i->uv_.min_.y_, i->uv_.max_.x_, i->uv_.max_.y_));
        ret.Push(i->color_);
        ret.Push(i->rotation_);
        ret.Push(i->direction_);
        ret.Push(i->enabled_);
    }

    return ret;
}

const PODVector<unsigned char>& TreeBillboardSet::GetNetTreeBillboardsAttr() const
{
    attrBuffer_.Clear();
    attrBuffer_.WriteVLE(TreeBillboards_.Size());

    for (Vector<SharedPtr<TreeBillboard>>::ConstIterator ii = TreeBillboards_.Begin(); ii != TreeBillboards_.End(); ++ii)
    {
        const TreeBillboard* i = *ii;
        attrBuffer_.WriteVector3(i->position_);
        attrBuffer_.WriteVector2(i->size_);
        attrBuffer_.WriteRect(i->uv_);
        attrBuffer_.WriteColor(i->color_);
        attrBuffer_.WriteFloat(i->rotation_);
        attrBuffer_.WriteVector3(i->direction_);
        attrBuffer_.WriteBool(i->enabled_);
    }

    return attrBuffer_.GetBuffer();
}

void TreeBillboardSet::OnWorldBoundingBoxUpdate()
{
    unsigned enabledTreeBillboards = 0;
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    Matrix3x4 TreeBillboardTransform = relative_ ? worldTransform : Matrix3x4::IDENTITY;
    Vector3 TreeBillboardScale = scaled_ ? worldTransform.Scale() : Vector3::ONE;
    BoundingBox worldBox;

    for (unsigned i = 0; i < TreeBillboards_.Size(); ++i)
    {
        if (!TreeBillboards_[i]->enabled_)
            continue;

        float size = INV_SQRT_TWO * (TreeBillboards_[i]->size_.x_ * TreeBillboardScale.x_ + TreeBillboards_[i]->size_.y_ * TreeBillboardScale.y_);
        if (fixedScreenSize_)
            size *= TreeBillboards_[i]->screenScaleFactor_;

        Vector3 center = TreeBillboardTransform * TreeBillboards_[i]->position_;
        Vector3 edge = Vector3::ONE * size;
        worldBox.Merge(BoundingBox(center - edge, center + edge));

        ++enabledTreeBillboards;
    }

    // Always merge the node's own position to ensure particle emitter updates continue when the relative mode is switched
    worldBox.Merge(node_->GetWorldPosition());

    worldBoundingBox_ = worldBox;
}

void TreeBillboardSet::UpdateBufferSize()
{
    unsigned numTreeBillboards = TreeBillboards_.Size();

    if (vertexBuffer_->GetVertexCount() != numTreeBillboards * 4 || geometryTypeUpdate_)
    {
        if (faceCameraMode_ == FC_DIRECTION)
        {
            vertexBuffer_->SetSize(numTreeBillboards * 4, MASK_POSITION | MASK_NORMAL | MASK_COLOR | MASK_TEXCOORD1 | MASK_TEXCOORD2, true);
            geometry_->SetVertexBuffer(0, vertexBuffer_);

        }
        else
        {
            vertexBuffer_->SetSize(numTreeBillboards * 4, MASK_POSITION | MASK_COLOR | MASK_TEXCOORD1 | MASK_TEXCOORD2, true);
            geometry_->SetVertexBuffer(0, vertexBuffer_);
        }
        geometryTypeUpdate_ = false;
    }
    if (indexBuffer_->GetIndexCount() != numTreeBillboards * 6)
        indexBuffer_->SetSize(numTreeBillboards * 6, false);

    bufferSizeDirty_ = false;
    bufferDirty_ = true;
    forceUpdate_ = true;

    if (!numTreeBillboards)
        return;

    // Indices do not change for a given TreeBillboard capacity
    unsigned short* dest = (unsigned short*)indexBuffer_->Lock(0, numTreeBillboards * 6, true);
    if (!dest)
        return;

    unsigned vertexIndex = 0;
    while (numTreeBillboards--)
    {
        dest[0] = (unsigned short)vertexIndex;
        dest[1] = (unsigned short)(vertexIndex + 1);
        dest[2] = (unsigned short)(vertexIndex + 2);
        dest[3] = (unsigned short)(vertexIndex + 2);
        dest[4] = (unsigned short)(vertexIndex + 3);
        dest[5] = (unsigned short)vertexIndex;

        dest += 6;
        vertexIndex += 4;
    }

    indexBuffer_->Unlock();
    indexBuffer_->ClearDataLost();
}

void TreeBillboardSet::UpdateVertexBuffer(const FrameInfo& frame)
{
    // If using animation LOD, accumulate time and see if it is time to update
    if (animationLodBias_ > 0.0f && lodDistance_ > 0.0f)
    {
        animationLodTimer_ += animationLodBias_ * frame.timeStep_ * ANIMATION_LOD_BASESCALE;
        if (animationLodTimer_ >= lodDistance_)
            animationLodTimer_ = fmodf(animationLodTimer_, lodDistance_);
        else
        {
            // No LOD if immediate update forced
            if (!forceUpdate_)
                return;
        }
    }

    unsigned numTreeBillboards = TreeBillboards_.Size();
    unsigned enabledTreeBillboards = 0;
    const Matrix3x4& worldTransform = node_->GetWorldTransform();
    Matrix3x4 TreeBillboardTransform = relative_ ? worldTransform : Matrix3x4::IDENTITY;
    Vector3 TreeBillboardScale = scaled_ ? worldTransform.Scale() : Vector3::ONE;

    // First check number of enabled TreeBillboards
    for (unsigned i = 0; i < numTreeBillboards; ++i)
    {
        if (TreeBillboards_[i]->enabled_)
            ++enabledTreeBillboards;
    }

    sortedTreeBillboards_.Resize(enabledTreeBillboards);
    unsigned index = 0;

    // Then set initial sort order and distances
    for (unsigned i = 0; i < numTreeBillboards; ++i)
    {
        TreeBillboard* TreeBillboard = TreeBillboards_[i];
        if (TreeBillboard->enabled_)
        {
            sortedTreeBillboards_[index++] = TreeBillboard;
            if (sorted_)
                TreeBillboard->sortDistance_ = frame.camera_->GetDistanceSquared(TreeBillboardTransform * TreeBillboards_[i]->position_);
        }
    }

    batches_[0].geometry_->SetDrawRange(TRIANGLE_LIST, 0, enabledTreeBillboards * 6, false);

    bufferDirty_ = false;
    forceUpdate_ = false;
    if (!enabledTreeBillboards)
        return;

    if (sorted_)
    {
        Sort(sortedTreeBillboards_.Begin(), sortedTreeBillboards_.End(), CompareTreeBillboards);
        Vector3 worldPos = node_->GetWorldPosition();
        // Store the "last sorted position" now
        previousOffset_ = (worldPos - frame.camera_->GetNode()->GetWorldPosition());
    }

    float* dest = (float*)vertexBuffer_->Lock(0, enabledTreeBillboards * 4, true);
    if (!dest)
        return;

    if (faceCameraMode_ != FC_DIRECTION)
    {
        for (unsigned i = 0; i < enabledTreeBillboards; ++i)
        {
            TreeBillboard& TreeBillboard = *sortedTreeBillboards_[i];

            Vector2 size(TreeBillboard.size_.x_ * TreeBillboardScale.x_, TreeBillboard.size_.y_ * TreeBillboardScale.y_);
            unsigned color = TreeBillboard.color_.ToUInt();
            if (fixedScreenSize_)
                size *= TreeBillboard.screenScaleFactor_;

            float rotationMatrix[2][2];
            SinCos(TreeBillboard.rotation_, rotationMatrix[0][1], rotationMatrix[0][0]);
            rotationMatrix[1][0] = -rotationMatrix[0][1];
            rotationMatrix[1][1] = rotationMatrix[0][0];

            dest[0] = TreeBillboard.position_.x_;
            dest[1] = TreeBillboard.position_.y_;
            dest[2] = TreeBillboard.position_.z_;
            ((unsigned&)dest[3]) = color;
            dest[4] = TreeBillboard.uv_.min_.x_;
            dest[5] = TreeBillboard.uv_.min_.y_;
            dest[6] = -size.x_ * rotationMatrix[0][0] + size.y_ * rotationMatrix[0][1];
            dest[7] = -size.x_ * rotationMatrix[1][0] + size.y_ * rotationMatrix[1][1];

            dest[8] = TreeBillboard.position_.x_;
            dest[9] = TreeBillboard.position_.y_;
            dest[10] = TreeBillboard.position_.z_;
            ((unsigned&)dest[11]) = color;
            dest[12] = TreeBillboard.uv_.max_.x_;
            dest[13] = TreeBillboard.uv_.min_.y_;
            dest[14] = size.x_ * rotationMatrix[0][0] + size.y_ * rotationMatrix[0][1];
            dest[15] = size.x_ * rotationMatrix[1][0] + size.y_ * rotationMatrix[1][1];

            dest[16] = TreeBillboard.position_.x_;
            dest[17] = TreeBillboard.position_.y_;
            dest[18] = TreeBillboard.position_.z_;
            ((unsigned&)dest[19]) = color;
            dest[20] = TreeBillboard.uv_.max_.x_;
            dest[21] = TreeBillboard.uv_.max_.y_;
            dest[22] = size.x_ * rotationMatrix[0][0] - size.y_ * rotationMatrix[0][1];
            dest[23] = size.x_ * rotationMatrix[1][0] - size.y_ * rotationMatrix[1][1];

            dest[24] = TreeBillboard.position_.x_;
            dest[25] = TreeBillboard.position_.y_;
            dest[26] = TreeBillboard.position_.z_;
            ((unsigned&)dest[27]) = color;
            dest[28] = TreeBillboard.uv_.min_.x_;
            dest[29] = TreeBillboard.uv_.max_.y_;
            dest[30] = -size.x_ * rotationMatrix[0][0] - size.y_ * rotationMatrix[0][1];
            dest[31] = -size.x_ * rotationMatrix[1][0] - size.y_ * rotationMatrix[1][1];

            dest += 32;
        }
    }
    else
    {
        for (unsigned i = 0; i < enabledTreeBillboards; ++i)
        {
            TreeBillboard& TreeBillboard = *sortedTreeBillboards_[i];

            Vector2 size(TreeBillboard.size_.x_ * TreeBillboardScale.x_, TreeBillboard.size_.y_ * TreeBillboardScale.y_);
            unsigned color = TreeBillboard.color_.ToUInt();
            if (fixedScreenSize_)
                size *= TreeBillboard.screenScaleFactor_;

            float rot2D[2][2];
            SinCos(TreeBillboard.rotation_, rot2D[0][1], rot2D[0][0]);
            rot2D[1][0] = -rot2D[0][1];
            rot2D[1][1] = rot2D[0][0];

            dest[0] = TreeBillboard.position_.x_;
            dest[1] = TreeBillboard.position_.y_;
            dest[2] = TreeBillboard.position_.z_;
            dest[3] = TreeBillboard.direction_.x_;
            dest[4] = TreeBillboard.direction_.y_;
            dest[5] = TreeBillboard.direction_.z_;
            ((unsigned&)dest[6]) = color;
            dest[7] = TreeBillboard.uv_.min_.x_;
            dest[8] = TreeBillboard.uv_.min_.y_;
            dest[9] = -size.x_ * rot2D[0][0] + size.y_ * rot2D[0][1];
            dest[10] = -size.x_ * rot2D[1][0] + size.y_ * rot2D[1][1];

            dest[11] = TreeBillboard.position_.x_;
            dest[12] = TreeBillboard.position_.y_;
            dest[13] = TreeBillboard.position_.z_;
            dest[14] = TreeBillboard.direction_.x_;
            dest[15] = TreeBillboard.direction_.y_;
            dest[16] = TreeBillboard.direction_.z_;
            ((unsigned&)dest[17]) = color;
            dest[18] = TreeBillboard.uv_.max_.x_;
            dest[19] = TreeBillboard.uv_.min_.y_;
            dest[20] = size.x_ * rot2D[0][0] + size.y_ * rot2D[0][1];
            dest[21] = size.x_ * rot2D[1][0] + size.y_ * rot2D[1][1];

            dest[22] = TreeBillboard.position_.x_;
            dest[23] = TreeBillboard.position_.y_;
            dest[24] = TreeBillboard.position_.z_;
            dest[25] = TreeBillboard.direction_.x_;
            dest[26] = TreeBillboard.direction_.y_;
            dest[27] = TreeBillboard.direction_.z_;
            ((unsigned&)dest[28]) = color;
            dest[29] = TreeBillboard.uv_.max_.x_;
            dest[30] = TreeBillboard.uv_.max_.y_;
            dest[31] = size.x_ * rot2D[0][0] - size.y_ * rot2D[0][1];
            dest[32] = size.x_ * rot2D[1][0] - size.y_ * rot2D[1][1];

            dest[33] = TreeBillboard.position_.x_;
            dest[34] = TreeBillboard.position_.y_;
            dest[35] = TreeBillboard.position_.z_;
            dest[36] = TreeBillboard.direction_.x_;
            dest[37] = TreeBillboard.direction_.y_;
            dest[38] = TreeBillboard.direction_.z_;
            ((unsigned&)dest[39]) = color;
            dest[40] = TreeBillboard.uv_.min_.x_;
            dest[41] = TreeBillboard.uv_.max_.y_;
            dest[42] = -size.x_ * rot2D[0][0] - size.y_ * rot2D[0][1];
            dest[43] = -size.x_ * rot2D[1][0] - size.y_ * rot2D[1][1];

            dest += 44;
        }
    }

    vertexBuffer_->Unlock();
    vertexBuffer_->ClearDataLost();
}

void TreeBillboardSet::MarkPositionsDirty()
{
    Drawable::OnMarkedDirty(node_);
    bufferDirty_ = true;
}

void TreeBillboardSet::CalculateFixedScreenSize(const FrameInfo& frame)
{
    float invViewHeight = 1.0f / frame.viewSize_.y_;
    float halfViewWorldSize = frame.camera_->GetHalfViewSize();

    if (!frame.camera_->IsOrthographic())
    {
        Matrix4 viewProj(frame.camera_->GetProjection() * frame.camera_->GetView());
        const Matrix3x4& worldTransform = node_->GetWorldTransform();
        Matrix3x4 TreeBillboardTransform = relative_ ? worldTransform : Matrix3x4::IDENTITY;

        for (unsigned i = 0; i < TreeBillboards_.Size(); ++i)
        {
            Vector4 projPos(viewProj * Vector4(TreeBillboardTransform * TreeBillboards_[i]->position_, 1.0f));
            TreeBillboards_[i]->screenScaleFactor_ = invViewHeight * halfViewWorldSize * projPos.w_;
        }
    }
    else
    {
        for (unsigned i = 0; i < TreeBillboards_.Size(); ++i)
            TreeBillboards_[i]->screenScaleFactor_ = invViewHeight * halfViewWorldSize;
    }

    bufferDirty_ = true;
    forceUpdate_ = true;
    worldBoundingBoxDirty_ = true;
}

}
