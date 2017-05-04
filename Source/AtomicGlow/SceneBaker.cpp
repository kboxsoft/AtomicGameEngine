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

#include <xmmintrin.h>
#include <pmmintrin.h>
#include <cmath>
#include <cfloat>

#include <ThirdParty/STB/stb_rect_pack.h>

#include <Atomic/IO/Log.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Graphics/Light.h>
#include <Atomic/Graphics/StaticModel.h>

#include "BakeModel.h"
#include "BakeMesh.h"
#include "BakeLight.h"
#include "EmbreeScene.h"
#include "SceneBaker.h"

namespace AtomicGlow
{

SceneBaker::SceneBaker(Context* context) : Object(context)
{
    embreeScene_ = new EmbreeScene(context_);
}

SceneBaker::~SceneBaker()
{

}

/*
bool SceneBaker::TryAddStaticModelBaker(StaticModelBaker *bakeModel)
{
    Image* lightmap = bakeModel->GetLightmap();

    if (bakeModel->GetLightmapPacked() || !lightmap || !lightmap->GetWidth() || !lightmap->GetHeight())
        return false;

    // FIXME: configurable
    int numnodes = LIGHTMAP_WIDTH;

    SharedArrayPtr<unsigned char> nodes(new unsigned char[sizeof(stbrp_node) * numnodes]);
    SharedArrayPtr<unsigned char> rects(new unsigned char[sizeof(stbrp_rect) * (workingSet_.Size() + 1)]);

    stbrp_context rectContext;

    stbrp_init_target (&rectContext, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, (stbrp_node *) nodes.Get(), numnodes);
    stbrp_rect* rect = (stbrp_rect*) rects.Get();

    // add working set, we do this brute force for best results
    for (unsigned i = 0; i < workingSet_.Size(); i++)
    {
        StaticModelBaker* baker = workingSet_[i];
        Image* lightmap = baker->GetLightmap();

        rect->id = (int) i;
        rect->w = lightmap->GetWidth();
        rect->h = lightmap->GetHeight();

        rect++;
    }

    rect->id = (int) workingSet_.Size();
    rect->w = lightmap->GetWidth();
    rect->h = lightmap->GetHeight();


    if (!stbrp_pack_rects (&rectContext, (stbrp_rect *)rects.Get(), workingSet_.Size() + 1))
    {
        return false;
    }

    return true;

}

void SceneBaker::EmitLightmap(int lightMapIndex)
{
    // FIXME: configurable
    int width = LIGHTMAP_WIDTH;
    int height = LIGHTMAP_HEIGHT;

    // see note in stbrp_init_target docs
    int numnodes = width;

    SharedArrayPtr<unsigned char> nodes(new unsigned char[sizeof(stbrp_node) * numnodes]);
    SharedArrayPtr<unsigned char> rects(new unsigned char[sizeof(stbrp_rect) * workingSet_.Size()]);

    stbrp_context rectContext;

    stbrp_init_target (&rectContext, width, height, (stbrp_node *) nodes.Get(), numnodes);
    stbrp_rect* rect = (stbrp_rect*) rects.Get();

    for (unsigned i = 0; i < workingSet_.Size(); i++)
    {
        StaticModelBaker* baker = workingSet_[i];
        Image* lightmap = baker->GetLightmap();

        if (!lightmap)
            continue;

        rect->id = (int) i;
        rect->w = lightmap->GetWidth();
        rect->h = lightmap->GetHeight();

        rect++;
    }

    if (!stbrp_pack_rects (&rectContext, (stbrp_rect *)rects.Get(), workingSet_.Size()))
    {
        ATOMIC_LOGERROR("SceneBaker::Light() - not all rects packed");
        return;
    }

    SharedPtr<Image> image(new Image(context_));
    image->SetSize(width, height, 3);
    image->Clear(Color::BLACK);

    rect = (stbrp_rect*) rects.Get();

    for (unsigned i = 0; i < workingSet_.Size(); i++)
    {
        StaticModelBaker* baker = workingSet_[i];
        Image* lightmap = baker->GetLightmap();
        StaticModel* staticModel = baker->GetStaticModel();

        if (!rect->was_packed)
        {
            ATOMIC_LOGINFO("SceneBaker::Light() - skipping unpacked lightmap");
            continue;
        }

        baker->SetLightmapPacked(true);

        // TODO: apply lightmap scaling from static model setting
        image->SetSubimage(lightmap, IntRect(rect->x, rect->y, rect->x + lightmap->GetWidth(), rect->y + lightmap->GetHeight()));

        staticModel->SetLightmapIndex(lightMapIndex);
        staticModel->SetLightmapTilingOffset(Vector4(float(lightmap->GetWidth())/float(width),
                                                     float(lightmap->GetHeight())/float(height),
                                                     float(rect->x)/float(width),
                                                     float(rect->y)/float(height)));

        rect++;
    }

    FilterLightmap(image);
#ifdef ATOMIC_PLATFORM_WINDOWS
    String filename = ToString("C:/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightMapIndex);
#else
    String filename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightMapIndex);
#endif
    image->SavePNG(filename);

    workingSet_.Clear();

}

void SceneBaker::FilterLightmap(Image* lightmap)
{
    // Box 3x3 Filter

    SharedPtr<Image> tmp(new Image(context_));
    tmp->SetSize(lightmap->GetWidth(), lightmap->GetHeight(), lightmap->GetComponents());
    tmp->Clear(Color::BLACK);

    for (int y = 0; y < lightmap->GetHeight(); y++)
    {
        for (int x = 0; x < lightmap->GetWidth(); x++)
        {
            Color color = lightmap->GetPixel(x-1, y);
            color += lightmap->GetPixel(x, y);
            color += lightmap->GetPixel(x+1, y);

            color.r_ /= 3.0f;
            color.g_ /= 3.0f;
            color.b_ /= 3.0f;

            tmp->SetPixel(x, y, color);
        }
    }

    for (int y = 0; y < lightmap->GetHeight(); y++)
    {
        for (int x = 0; x < lightmap->GetWidth(); x++)
        {
            Color color = tmp->GetPixel(x, y-1);
            color += tmp->GetPixel(x, y);
            color += tmp->GetPixel(x, y+1);

            color.r_ /= 3.0f;
            color.g_ /= 3.0f;
            color.b_ /= 3.0f;

            lightmap->SetPixel(x, y, color);

        }

    }

}
*/

void SceneBaker::TraceRay(LightRay* lightRay, const PODVector<BakeLight*>& bakeLights_)
{
    for (unsigned i = 0; i < bakeLights_.Size(); i++)
    {
        bakeLights_[i]->Light(lightRay);
    }
}

bool SceneBaker::Light()
{    
    for (unsigned i = 0; i < bakeMeshes_.Size(); i++)
    {
        BakeMesh* mesh = bakeMeshes_[i];
        mesh->Light();
    }


/*
enum RTCIntersectFlags
{
  RTC_INTERSECT_COHERENT                 = 0,  //!< optimize for coherent rays
  RTC_INTERSECT_INCOHERENT               = 1   //!< optimize for incoherent rays
};

struct RTCIntersectContext
{
  RTCIntersectFlags flags;   //!< intersection flags
  void* userRayExt;          //!< can be used to pass extended ray data to callbacks
};

    RTCIntersectContext context;
    context.flags = g_iflags;
    rtcIntersect1Ex(g_scene,&context,ray);
*/


    /*
    StaticModelBaker* bakeModel;

    Vector<SharedPtr<StaticModelBaker>>::Iterator itr = staticModelBakers_.Begin();

    while (itr != staticModelBakers_.End())
    {
        bakeModel = *itr;

        if (!bakeModel->GetLightmap())
        {
            itr++;
            continue;
        }

        //bakeModel->TraceAORays(512, 1.0f);
        bakeModel->TraceSunLight();
        bakeModel->ProcessLightmap();
        itr++;
    }

    int lightmapIndex = 0;

    for (unsigned i = 0; i < staticModelBakers_.Size(); i++)
    {
        bakeModel = staticModelBakers_[i];

        if (!bakeModel->GetLightmap() || bakeModel->GetLightmapPacked())
            continue;

        Image* lightmap = bakeModel->GetLightmap();

        if (lightmap->GetWidth() >= LIGHTMAP_WIDTH || lightmap->GetHeight() >= LIGHTMAP_HEIGHT)
        {
#ifdef ATOMIC_PLATFORM_WINDOWS
            String filename = ToString("C:/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightmapIndex);
#else
            String filename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightmapIndex);
#endif            

            FilterLightmap(lightmap);
            lightmap->SavePNG(filename);

            bakeModel->SetLightmapPacked(true);
            bakeModel->GetStaticModel()->SetLightmapIndex(lightmapIndex);
            bakeModel->GetStaticModel()->SetLightmapTilingOffset(Vector4(1.0f, 1.0f, 0.0f, 0.0f));

            lightmapIndex++;
            continue;
        }

        workingSet_.Push(bakeModel);

        for (unsigned j = 0; j < staticModelBakers_.Size(); j++)
        {
            if (i == j)
                continue;

            StaticModelBaker* model = staticModelBakers_[j];

            if (TryAddStaticModelBaker(model))
            {
                workingSet_.Push(model);
            }

        }

        EmitLightmap(lightmapIndex++);
        workingSet_.Clear();

    }

#ifdef ATOMIC_PLATFORM_WINDOWS
    String scenefilename = ToString("C:/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Scenes/LitScene.scene");
#else
    String scenefilename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Scenes/LitScene.scene");
#endif            

    File saveFile(context_, scenefilename, FILE_WRITE);
    scene_->SaveXML(saveFile);
    */

    return true;

}

void SceneBaker::QueryLights(const BoundingBox& bbox, PODVector<BakeLight*>& lights)
{
    lights.Clear();

    for (unsigned i = 0; i < bakeLights_.Size(); i++)
    {

        // TODO: filter on range, groups, etc
        lights.Push(bakeLights_[i]);

    }

}

bool SceneBaker::Preprocess()
{
    Vector<SharedPtr<BakeMesh>>::Iterator itr = bakeMeshes_.Begin();

    while (itr != bakeMeshes_.End())
    {
        (*itr)->Preprocess();
        itr++;
    }   

    embreeScene_->Commit();

    return true;
}

/*
static bool CompareStaticModelBaker(StaticModelBaker* lhs, StaticModelBaker* rhs)
{
    int lhsWeight = lhs->GetLightmap() ? lhs->GetLightmap()->GetWidth() : 0;
    lhsWeight += lhs->GetLightmap() ? lhs->GetLightmap()->GetHeight() : 0;

    int rhsWeight = rhs->GetLightmap() ? rhs->GetLightmap()->GetWidth() : 0;
    rhsWeight += rhs->GetLightmap() ? rhs->GetLightmap()->GetHeight() : 0;

    return lhsWeight < rhsWeight;
}
*/


bool SceneBaker::LoadScene(const String& filename)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();    
    SharedPtr<File> file = cache->GetFile(filename);

    if (!file || !file->IsOpen())
    {
        return false;
    }

    scene_ = new Scene(context_);

    if (!scene_->LoadXML(*file))
    {
        scene_ = 0;
        return false;
    }

    PODVector<Node*> lightNodes;
    scene_->GetChildrenWithComponent<Atomic::Light>(lightNodes, true);

    for (unsigned i = 0; i < lightNodes.Size(); i++)
    {
        Atomic::Light* light = lightNodes[i]->GetComponent<Atomic::Light>();

        if (light->GetLightType() == LIGHT_DIRECTIONAL)
        {
            SharedPtr<BakeLightDirectional> dlight(new BakeLightDirectional(context_, this));
            dlight->SetLight(light);
            bakeLights_.Push(dlight);
        }
    }

    PODVector<StaticModel*> staticModels;
    scene_->GetComponents<StaticModel>(staticModels, true);

    for (unsigned i = 0; i < staticModels.Size(); i++)
    {
        StaticModel* staticModel = staticModels[i];

        if (staticModel->GetModel() && (staticModel->GetLightmap() ||staticModel->GetCastShadows()))
        {
            SharedPtr<BakeMesh> meshMap (new BakeMesh(context_, this));
            meshMap->SetStaticModel(staticModel);
            bakeMeshes_.Push(meshMap);
        }

    }

    // Sort by lightmap size
    // Sort(staticModelBakers_.Begin(), staticModelBakers_.End(), CompareStaticModelBaker);

    return true;
}

}
