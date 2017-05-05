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

#include "EmbreeScene.h"

#include <Atomic/IO/Log.h>
#include <Atomic/Graphics/Zone.h>

#include "Raster.h"
#include "LightRay.h"
#include "SceneBaker.h"
#include "BakeMesh.h"

namespace AtomicGlow
{

RadianceMap::RadianceMap(Context* context) : Object(context),
    packed_(false)
{

}

RadianceMap::~RadianceMap()
{

}

BakeMesh::BakeMesh(Context* context, SceneBaker *sceneBaker) : BakeNode(context, sceneBaker),
    numVertices_(0),
    numTriangles_(0),
    radianceHeight_(0),
    radianceWidth_(0),
    embreeGeomID_(RTC_INVALID_GEOMETRY_ID)
{

}

BakeMesh::~BakeMesh()
{

}

void BakeMesh::Pack(unsigned lightmapIdx, Vector4 tilingOffset)
{
    if (radianceMap_.NotNull())
    {
        radianceMap_->packed_ = true;
    }

    if (staticModel_)
    {
        staticModel_->SetLightMask(0);
        staticModel_->SetLightmapIndex(lightmapIdx);
        staticModel_->SetLightmapTilingOffset(tilingOffset);
    }

}


bool BakeMesh::FillLexelsCallback(void* param, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage)
{
    return ((ShaderData*) param)->bakeMesh_->LightPixel((ShaderData*) param, x, y, barycentric, dx, dy, coverage);
}

bool BakeMesh::LightPixel(BakeMesh::ShaderData* shaderData, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage)
{

    const Vector3& rad = radiance_[y * radianceWidth_ + x];

    // check whether we've already lit this pixel
    if (rad.x_ >= 0.0f)
    {
        return true;
    }

    MMTriangle* tri = &triangles_[shaderData->triangleIdx_];
    MMVertex* verts[3];

    verts[0] = &vertices_[tri->indices_[0]];
    verts[1] = &vertices_[tri->indices_[1]];
    verts[2] = &vertices_[tri->indices_[2]];

    LightRay ray;
    LightRay::SamplePoint& sample = ray.samplePoint_;

    sample.bakeMesh = this;

    sample.triangle = shaderData->triangleIdx_;

    sample.radianceX = x;
    sample.radianceY = y;

    sample.position = verts[0]->position_ * barycentric.x_ +
                      verts[1]->position_ * barycentric.y_ +
                      verts[2]->position_ * barycentric.z_;

    sample.normal = verts[0]->normal_ * barycentric.x_ +
                    verts[1]->normal_ * barycentric.y_ +
                    verts[2]->normal_ * barycentric.z_;

    sample.uv0  = verts[0]->uv0_ * barycentric.x_ +
                  verts[1]->uv0_ * barycentric.y_ +
                  verts[2]->uv0_ * barycentric.z_;

    sample.uv1  = verts[0]->uv1_ * barycentric.x_ +
                  verts[1]->uv1_ * barycentric.y_ +
                  verts[2]->uv1_ * barycentric.z_;

    sceneBaker_->TraceRay(&ray, bakeLights_);

    return true;
}

void BakeMesh::ContributeRadiance(int x, int y, const Vector3& radiance)
{
    const Vector3& v = radiance_[y * radianceWidth_ + x];

    if (v.x_ < 0.0f)
    {
        radiance_[y * radianceWidth_ + x] = radiance;
    }
    else
    {
        radiance_[y * radianceWidth_ + x] += radiance;
    }

}


void BakeMesh::GenerateRadianceMap()
{
    if (radianceMap_.NotNull())
        return;

    radianceMap_ = new RadianceMap(context_);

    SharedPtr<Image> image (new Image(context_));

    radianceMap_->bakeMesh_ = SharedPtr<BakeMesh>(this);
    radianceMap_->image_ = image;

    image->SetSize(radianceWidth_, radianceHeight_, 3);
    image->Clear(Color::BLACK);

    Color c;
    for (unsigned y = 0; y < radianceHeight_; y++)
    {
        for (unsigned x = 0; x < radianceWidth_; x++)
        {
            const Vector3& rad = radiance_[y * radianceWidth_ + x];

            if (rad.x_ >= 0.0f)
            {
                c.r_ = rad.x_; //Min<float>(rad.x_, 1.0f);
                c.g_ = rad.y_;// Min<float>(rad.y_, 1.0f);
                c.b_ = rad.z_;// Min<float>(rad.z_, 1.0f);
                image->SetPixel(x, y, c);
            }
        }
    }

    // Dilate the image by 2 pixels to allow bilinear texturing near seams.
    // Note that this still allows seams when mipmapping, unless mipmap levels
    // are generated very carefully.

    SharedArrayPtr<Color> tmp(new Color[radianceWidth_ * radianceHeight_]);

    for (int step = 0; step < 2; step++)
    {
        memset (&tmp[0], 0, radianceWidth_ * radianceHeight_ * sizeof(Color));

        for (int y = 0; y < radianceHeight_ ; y++)
        {
            for (int x = 0; x < radianceWidth_; x++)
            {
                int center = x + y * radianceWidth_;

                Color color = image->GetPixel(x, y);
                Vector3& rad = radiance_[center];

                tmp[center] = color;

                if (rad.x_ < 0.0f)
                {
                    for (int k = 0; k < 9; k++)
                    {
                        int i = (k / 3) - 1, j = (k % 3) - 1;

                        if (i == 0 && j == 0)
                        {
                            continue;
                        }

                        i += x;
                        j += y;

                        if (i < 0 || j < 0 || i >= radianceWidth_ || j >=  radianceHeight_ )
                        {
                            continue;
                        }

                        Color color2 = image->GetPixel(i, j);

                        if (color2 != Color::BLACK)
                        {
                            tmp[center] = color2;
                            break;
                        }
                    }
                }
            }
        }

        for (int y = 0; y < radianceHeight_ ; y++)
        {
            for (int x = 0; x < radianceWidth_; x++)
            {
                image->SetPixel(x, y, tmp[y * radianceWidth_ + x]);
            }
        }
    }

    Color ambient = Color::BLACK;

    if (staticModel_ && staticModel_->GetZone())
    {
        ambient = staticModel_->GetZone()->GetAmbientColor();
    }

    if (ambient != Color::BLACK)
    {
        for (int y = 0; y < radianceHeight_ ; y++)
        {
            for (int x = 0; x < radianceWidth_; x++)
            {
                if (image->GetPixel(x, y) == Color::BLACK)
                    image->SetPixel(x, y, ambient);

            }
        }
    }

}

void BakeMesh::Light()
{
    if (!GetLightmap() || !radianceWidth_ || !radianceHeight_)
        return;

    // for all triangles

    // init radiance
    radiance_ = new Vector3[radianceWidth_ * radianceHeight_];

    Vector3 v(-1, -1, -1);
    for (unsigned i = 0; i < radianceWidth_ * radianceHeight_; i++)
    {
        radiance_[i] = v;
    }

    Vector2 extents(radianceWidth_, radianceHeight_);
    Vector2 triUV1[3];

    ShaderData shaderData;

    shaderData.bakeMesh_ = this;

    for (unsigned i = 0; i < numTriangles_; i++)
    {
        shaderData.triangleIdx_ = i;

        for (unsigned j = 0; j < 3; j++)
        {
            unsigned idx = triangles_[i].indices_[j];
            triUV1[j] = vertices_[idx].uv1_;
            triUV1[j].x_ *= float(radianceWidth_);
            triUV1[j].y_ *= float(radianceHeight_);
        }

        Raster::DrawTriangle(true, extents, true, triUV1, FillLexelsCallback, &shaderData );

    }

    GenerateRadianceMap();

}

void BakeMesh::Preprocess()
{

    RTCScene scene = sceneBaker_->GetEmbreeScene()->GetRTCScene();

    // Create the embree mesh
    embreeGeomID_ = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, numTriangles_, numVertices_);
    rtcSetUserData(scene, embreeGeomID_, this);
    rtcSetOcclusionFilterFunction(scene, embreeGeomID_, OcclusionFilter);

    // Populate vertices

    float* vertices = (float*) rtcMapBuffer(scene, embreeGeomID_, RTC_VERTEX_BUFFER);

    MMVertex* vIn = &vertices_[0];

    for (unsigned i = 0; i < numVertices_; i++, vIn++)
    {
        *vertices++ = vIn->position_.x_;
        *vertices++ = vIn->position_.y_;
        *vertices++ = vIn->position_.z_;

        // Note that RTC_VERTEX_BUFFER is 16 byte aligned, thus extra component
        // which isn't used, though we'll initialize it
        *vertices++ = 0.0f;
    }

    rtcUnmapBuffer(scene, embreeGeomID_, RTC_VERTEX_BUFFER);

    uint32_t* triangles = (uint32_t*) rtcMapBuffer(scene, embreeGeomID_, RTC_INDEX_BUFFER);

    for (size_t i = 0; i < numTriangles_; i++)
    {
        MMTriangle* tri = &triangles_[i];

        *triangles++ = tri->indices_[0];
        *triangles++ = tri->indices_[1];
        *triangles++ = tri->indices_[2];
    }

    rtcUnmapBuffer(scene, embreeGeomID_, RTC_INDEX_BUFFER);

    float lmScale = staticModel_->GetLightmapScale();

    // if we aren't lightmapped, we just contribute to occlusion
    if (!GetLightmap() || lmScale <= 0.0f)
        return;

    // TODO: global light scale for quick bakes and super sampling mode
    unsigned lmSize = staticModel_->GetLightmapSize();

    if (!lmSize)
    {
        // TODO: calculate surface area instead of using this rough metric
        Vector3 size = staticModel_->GetModel()->GetBoundingBox().Size();
        size *= node_->GetWorldScale();

        float sz = 1.0f - (Min<float>(size.Length(), 32.0f)/32.0f);

        sz += sz * 0.5f;

        float lexelScale = 16.0f + sz * 64.0f;

        lmSize = size.x_ * lexelScale * lmScale;
        lmSize += size.y_ * lexelScale * lmScale;
        lmSize += size.z_ * lexelScale * lmScale;

        if (lmSize > 512 && !IsPowerOfTwo(lmSize))
        {
            lmSize = NextPowerOfTwo(lmSize)/2;
        }

    }

    if (lmSize < 128)
        lmSize = 128;

    if (lmSize > 4096)
        lmSize = 4096;

    radianceWidth_ = lmSize;
    radianceHeight_ = lmSize;

    sceneBaker_->QueryLights(boundingBox_, bakeLights_);

}


bool BakeMesh::SetStaticModel(StaticModel* staticModel)
{
    if (!staticModel || !staticModel->GetNode())
        return false;

    staticModel_ = staticModel;
    node_ = staticModel_->GetNode();

    bakeModel_ = GetSubsystem<BakeModelCache>()->GetBakeModel(staticModel_->GetModel());

    if (bakeModel_.Null())
    {
        return false;
    }

    ModelPacker* modelPacker = bakeModel_->GetModelPacker();

    if (modelPacker->lodLevels_.Size() < 1)
    {
        return false;
    }

    MPLODLevel* lodLevel = modelPacker->lodLevels_[0];

    // LOD must have LM coords

    if (!lodLevel->HasElement(TYPE_VECTOR2, SEM_TEXCOORD, 1))
    {
        return false;
    }

    // materials

    if (staticModel_->GetNumGeometries() != lodLevel->mpGeometry_.Size())
    {
        ATOMIC_LOGERROR("BakeMesh::Preprocess() - Geometry mismatch");
        return false;
    }

    for (unsigned i = 0; i < staticModel_->GetNumGeometries(); i++)
    {
        BakeMaterial* bakeMaterial = GetSubsystem<BakeMaterialCache>()->GetBakeMaterial(staticModel_->GetMaterial(i));
        bakeMaterials_.Push(bakeMaterial);
    }

    // allocate

    numVertices_ = 0;
    unsigned totalIndices = 0;

    lodLevel->GetTotalCounts(numVertices_, totalIndices);

    if (!numVertices_ || ! totalIndices)
    {
        return false;
    }

    numTriangles_ = totalIndices/3;

    vertices_ = new MMVertex[numVertices_];
    triangles_ = new MMTriangle[numTriangles_];

    MMVertex* vOut = &vertices_[0];
    MMTriangle* tri = &triangles_[0];

    unsigned vertexStart = 0;
    unsigned indexStart = 0;

    const Matrix3x4& wtransform = node_->GetWorldTransform();

    for (unsigned i = 0; i < lodLevel->mpGeometry_.Size(); i++)
    {
        MPGeometry* mpGeo = lodLevel->mpGeometry_[i];

        // Copy Vertices

        MPVertex* vIn = &mpGeo->vertices_[0];

        for (unsigned j = 0; j < mpGeo->vertices_.Size(); j++)
        {
            vOut->position_ = wtransform * vIn->position_;
            vOut->normal_ = wtransform.Rotation() * vIn->normal_;
            vOut->uv0_ = vIn->uv0_;
            vOut->uv1_ = vIn->uv1_;

            boundingBox_.Merge(vOut->position_);

            vOut++;
            vIn++;
        }

        // Copy Indices

        for (unsigned j = 0; j < mpGeo->numIndices_; j+=3, tri++)
        {
            tri->materialIndex_ = i;

            tri->indices_[0] = mpGeo->indices_[j] + vertexStart;
            tri->indices_[1] = mpGeo->indices_[j + 1] + vertexStart;
            tri->indices_[2] = mpGeo->indices_[j + 2] + vertexStart;
        }

        indexStart  += mpGeo->numIndices_;
        vertexStart += mpGeo->vertices_.Size();
    }

    return true;

}

void BakeMesh::OcclusionFilter(void* ptr, RTCRay& ray)
{
    BakeMesh* bakeMesh = static_cast<BakeMesh*>(ptr);

    MMTriangle* tri = &bakeMesh->triangles_[ray.primID];

    BakeMaterial* material = bakeMesh->bakeMaterials_[tri->materialIndex_];

    Image* diffuse = material->GetDiffuseTexture();

    if (!diffuse)
    {
        return;
    }

    const Vector2& st0 = bakeMesh->vertices_[tri->indices_[0]].uv0_;
    const Vector2& st1 = bakeMesh->vertices_[tri->indices_[1]].uv0_;
    const Vector2& st2 = bakeMesh->vertices_[tri->indices_[2]].uv0_;

    const float u = ray.u, v = ray.v, w = 1.0f-ray.u-ray.v;

    const Vector2 st = w*st0 + u*st1 + v*st2;

    int x = diffuse->GetWidth() * st.x_;
    int y = diffuse->GetHeight() * st.y_;

    Color color = diffuse->GetPixel(x, y);

    if (color.a_ < 1.0f)
    {
        ray.geomID = RTC_INVALID_GEOMETRY_ID;
    }

}


}
