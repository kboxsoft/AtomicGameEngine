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

#include <Atomic/Core/StringUtils.h>
#include <Atomic/Resource/Image.h>
#include <Atomic/Scene/Node.h>

#include "Raster.h"
#include "SceneBaker.h"
#include "StaticModelBaker.h"

#ifdef __llvm__
double omp_get_wtime() { return 1; }
int omp_get_max_threads() { return 1; }
int omp_get_thread_num() { return 1; }
#else
#include <omp.h>
#endif

namespace AtomicGlow
{

// http://www.altdevblogaday.com/2012/05/03/generating-uniformly-distributed-points-on-sphere/
static inline void GetRandomDirection(Vector3& result)
{
    float z = 2.0f * rand() / RAND_MAX - 1.0f;
    float t = 2.0f * rand() / RAND_MAX * 3.14f;
    float r = sqrt(1.0f - z * z);
    result.x_ = r * (float) cos(t);
    result.y_ = r * (float) sin(t);
    result.z_ = z;
}

StaticModelBaker::StaticModelBaker(Context* context, SceneBaker *sceneBaker, StaticModel *staticModel) : Object(context),
    numIndices_(0),
    rtcTriMesh_(0)
{
    sceneBaker_ = sceneBaker;
    staticModel_ = staticModel;
    node_ = staticModel_->GetNode();
}

StaticModelBaker::~StaticModelBaker()
{
}

bool StaticModelBaker::AddToEmbreeScene()
{
    RTCScene scene = sceneBaker_->GetRTCScene();

    // Create the embree mesh
    rtcTriMesh_ = rtcNewTriangleMesh(scene, RTC_GEOMETRY_STATIC, numIndices_ / 3, lmVertices_.Size());

    // Populate vertices

    float* vertices = (float*) rtcMapBuffer(scene, rtcTriMesh_, RTC_VERTEX_BUFFER);

    for (unsigned i = 0; i < lmVertices_.Size(); i++)
    {
        const LMVertex& v = lmVertices_[i];

        *vertices++ = v.position_.x_;
        *vertices++ = v.position_.y_;
        *vertices++ = v.position_.z_;

        vertices++;
    }

    rtcUnmapBuffer(scene, rtcTriMesh_, RTC_VERTEX_BUFFER);

    uint32_t* triangles = (uint32_t*) rtcMapBuffer(scene, rtcTriMesh_, RTC_INDEX_BUFFER);

    for (size_t i = 0; i < numIndices_; i++)
    {
        *triangles++ = indices_[i];
    }

    rtcUnmapBuffer(scene, rtcTriMesh_, RTC_INDEX_BUFFER);

    return true;
}

void StaticModelBaker::ProcessLightmap()
{
    // Dilate the image by 2 pixels to allow bilinear texturing near seams.
    // Note that this still allows seams when mipmapping, unless mipmap levels
    // are generated very carefully.
    for (int step = 0; step < 2; step++)
    {
        SharedArrayPtr<Color> tmp(new Color[lightmap_->GetWidth() * lightmap_->GetHeight()]);
        memset (&tmp[0], 0, lightmap_->GetWidth() * lightmap_->GetHeight() * sizeof(Color));

        for (int y = 0; y < lightmap_->GetHeight() ; y++)
        {
            for (int x = 0; x < lightmap_->GetWidth(); x++)
            {
                int center = x + y * lightmap_->GetWidth();

                const LMLexel& lexel = lmLexels_[center];
                const Vector3& norm = lexel.normal_;

                tmp[center] = lexel.color_;

                if (norm.x_ == 0 && norm.y_ == 0 && norm.z_ == 0 && lexel.color_ == Color::BLACK)
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

                        if (i < 0 || j < 0 || i >= lightmap_->GetWidth() || j >=  lightmap_->GetHeight() )
                        {
                            continue;
                        }

                        const LMLexel& lexel2 = lmLexels_[i + j * lightmap_->GetWidth()];

                        if (lexel2.color_ != Color::BLACK)
                        {
                            tmp[center] = lexel2.color_;
                            break;
                        }
                    }
                }
            }
        }

        for (unsigned i = 0; i < lmLexels_.Size(); i++)
        {
            lmLexels_[i].color_ = tmp[i];
        }

    }

    for (unsigned i = 0; i < lmLexels_.Size(); i++)
    {
        const LMLexel& lexel = lmLexels_[i];
        lightmap_->SetPixelInt(lexel.pixelCoord_.x_, lexel.pixelCoord_.y_, 0, lexel.color_.ToUInt());
    }

    String filename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/%s_AOBake.png", node_->GetName().CString());
    lightmap_->SavePNG(filename);
}

void StaticModelBaker::TraceSunLight()
{
    RTCScene scene = sceneBaker_->GetRTCScene();

    const float E = 0.001f;

    RTCRay ray;

    Vector3 sunDir(0.6f, -1.0f, 0.8f);
    sunDir = -sunDir;
    sunDir.Normalize();

    for (unsigned i = 0; i < lmLexels_.Size(); i++)
    {
        LMLexel& lexel = lmLexels_[i];

        if (lexel.normal_ == Vector3::ZERO)
            continue;

        if (lexel.normal_.DotProduct(sunDir) < 0.0f)
        {
            continue;
        }

        ray.primID = RTC_INVALID_GEOMETRY_ID;
        ray.instID = RTC_INVALID_GEOMETRY_ID;
        ray.time = 0.f;

        ray.org[0] = lexel.position_.x_;
        ray.org[1] = lexel.position_.y_;
        ray.org[2] = lexel.position_.z_;

        ray.dir[0] = sunDir.x_;
        ray.dir[1] = sunDir.y_;
        ray.dir[2] = sunDir.z_;

        ray.tnear = E;
        ray.tfar = 100000.0f;

        ray.mask = 0xFFFFFFFF;
        ray.geomID = RTC_INVALID_GEOMETRY_ID;

        rtcOccluded(scene, ray);

        if (ray.geomID != 0)//  || ray.geomID == rtcTriMesh_)
        {
            lexel.color_ = Color(1.0f, 1.0f, 1.0f);
        }


    }

}

void StaticModelBaker::TraceAORays(unsigned nsamples, float aoDepth, float multiply)
{
    RTCScene scene = sceneBaker_->GetRTCScene();

    // Iterate over each pixel in the light map, row by row.
    // ATOMIC_LOGINFOF("Rendering ambient occlusion (%d threads)...", omp_get_max_threads());

    double begintime = omp_get_wtime();

    SharedArrayPtr<unsigned char> results(new unsigned char[lightmap_->GetWidth() * lightmap_->GetHeight()]);
    memset(&results[0], 0, sizeof(unsigned char) * lightmap_->GetWidth() * lightmap_->GetHeight());

    const unsigned npixels = lightmap_->GetWidth() * lightmap_->GetHeight();

    const float E = 0.001f;

#pragma omp parallel
    {
        srand(omp_get_thread_num());
        RTCRay ray;
        ray.primID = RTC_INVALID_GEOMETRY_ID;
        ray.instID = RTC_INVALID_GEOMETRY_ID;
        ray.mask = 0xFFFFFFFF;
        ray.time = 0.f;

#pragma omp for

        for (unsigned i = 0; i < npixels; i++)
        {
            LMLexel& lexel = lmLexels_[i];

            if (lexel.normal_ == Vector3::ZERO)
                continue;

            ray.org[0] = lexel.position_.x_;
            ray.org[1] = lexel.position_.y_;
            ray.org[2] = lexel.position_.z_;

            int nhits = 0;

            // Shoot rays through the differential hemisphere.
            for (unsigned nsamp = 0; nsamp < nsamples; nsamp++)
            {
                Vector3 rayDir;
                GetRandomDirection(rayDir);

                float dotp = lexel.normal_.x_ * rayDir.x_ +
                        lexel.normal_.y_ * rayDir.y_ +
                        lexel.normal_.z_ * rayDir.z_;

                if (dotp < 0)
                {
                    rayDir = -rayDir;
                }

                ray.dir[0] = rayDir.x_;
                ray.dir[1] = rayDir.y_;
                ray.dir[2] = rayDir.z_;

                ray.tnear = E;

                float variance = 0.0f;//(aoDepth * (float) rand() / (float) RAND_MAX);

                ray.tfar = aoDepth + variance;

                ray.geomID = RTC_INVALID_GEOMETRY_ID;
                rtcOccluded(scene, ray);

                if (ray.geomID == 0)
                {
                    nhits++;
                }
            }

            float ao = multiply * (1.0f - (float) nhits / nsamples);
            float result = Min<float>(1.0f, ao);

            // TODO: ambient
            if (result > 0.6f)
            {
                result = 0.6f;
            }

            lexel.color_ = Color(result, result, result);
        }
    }
}

bool StaticModelBaker::FillLexelsCallback(void* param, int x, int y, const Vector3& barycentric,const Vector3& dx, const Vector3& dy, float coverage)
{
    ShaderData* shaderData = (ShaderData*) param;
    StaticModelBaker* bake = shaderData->baker_;

    LMLexel& lexel = bake->lmLexels_[ y * bake->lightmap_->GetWidth() + x];

    lexel.position_ = shaderData->triPositions_[0] * barycentric.x_ +
            shaderData->triPositions_[1] *  barycentric.y_ +
            shaderData->triPositions_[2] * barycentric.z_;

    // TODO: ambient
    lexel.color_ = Color(.6f, .6f, .6f);

    lexel.normal_ = shaderData->faceNormal_;

    return true;
}

bool StaticModelBaker::Preprocess()
{
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

    unsigned totalVertices = 0;
    lodLevel->GetTotalCounts(totalVertices, numIndices_);

    if (!totalVertices || ! numIndices_)
    {
        return false;
    }

    // allocate

    lmVertices_.Resize(totalVertices);
    indices_ = new unsigned[numIndices_];

    LMVertex* vOut = &lmVertices_[0];

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

            vOut++;
            vIn++;
        }

        // Copy Indices

        for (unsigned j = 0; j < mpGeo->numIndices_; j++)
        {
            indices_[j + indexStart] = mpGeo->indices_[j] + vertexStart;
        }

        indexStart  += mpGeo->numIndices_;
        vertexStart += mpGeo->vertices_.Size();
    }

    lightmap_ = new Image(context_);

    unsigned lmSize = staticModel_->GetLightmapSize() ? staticModel_->GetLightmapSize() : 256;

    if (lmSize > 4096)
        lmSize = 4096;

    unsigned w, h;
    w = h = lmSize;

    lightmap_->SetSize(w, h, 2, 3);

    lmLexels_.Resize(w * h);

    for (unsigned y = 0; y < h; y++)
    {
        for (unsigned x = 0; x < w ; x++)
        {
            LMLexel& lexel = lmLexels_[y * w + x];
            lexel.color_ = Color::BLACK;
            lexel.pixelCoord_.x_ = x;
            lexel.pixelCoord_.y_ = y;
            lexel.normal_ = Vector3::ZERO;
            lexel.position_ = Vector3::ZERO;
        }
    }

    // for all triangles

    Vector2 extents(lightmap_->GetWidth(), lightmap_->GetHeight());
    Vector2 triUV[3];

    ShaderData shaderData;

    shaderData.baker_ = this;

    for (unsigned i = 0; i < numIndices_; i += 3)
    {
        shaderData.triPositions_[0] = lmVertices_[indices_[i]].position_;
        shaderData.triPositions_[1] = lmVertices_[indices_[i + 1]].position_;
        shaderData.triPositions_[2] = lmVertices_[indices_[i + 2]].position_;

        shaderData.triNormals_[0] = lmVertices_[indices_[i]].normal_;
        shaderData.triNormals_[1] = lmVertices_[indices_[i + 1]].normal_;
        shaderData.triNormals_[2] = lmVertices_[indices_[i + 2]].normal_;

        triUV[0] = lmVertices_[indices_[i]].uv1_;
        triUV[1] = lmVertices_[indices_[i + 1]].uv1_;
        triUV[2] = lmVertices_[indices_[i + 2]].uv1_;

        triUV[0].x_ *= w;
        triUV[1].x_ *= w;
        triUV[2].x_ *= w;

        triUV[0].y_ *= h;
        triUV[1].y_ *= h;
        triUV[2].y_ *= h;

        /*
        Vector3 A = shaderData.triPositions_[1] - shaderData.triPositions_[0];
        Vector3 B = shaderData.triPositions_[2] - shaderData.triPositions_[0];
        shaderData.faceNormal_ = A.CrossProduct(B);
        shaderData.faceNormal_.Normalize();
        */

        shaderData.faceNormal_ = shaderData.triNormals_[0];
        shaderData.faceNormal_ += shaderData.triNormals_[1];
        shaderData.faceNormal_ += shaderData.triNormals_[2];

        shaderData.faceNormal_ /= 3.0f;

        Raster::DrawTriangle(true, extents, true, triUV, FillLexelsCallback, &shaderData );

    }

    AddToEmbreeScene();

    return true;
}

}
