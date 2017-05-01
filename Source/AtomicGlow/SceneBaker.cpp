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
#include "SceneBaker.h"

namespace AtomicGlow
{


static void RTCErrorCallback(const RTCError code, const char* str)
{
    ATOMIC_LOGERRORF("RTC Error %d: %s", code, str);
}


SceneBaker::SceneBaker(Context* context) : Object(context),
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

SceneBaker::~SceneBaker()
{

}

bool SceneBaker::TryAddStaticModelBaker(StaticModelBaker *bakeModel)
{
    Image* lightmap = bakeModel->GetLightmap();

    if (bakeModel->GetLightmapPacked() || !lightmap || !lightmap->GetWidth() || !lightmap->GetHeight())
        return false;

    // FIXME: configurable
    int numnodes = LIGHTMAP_WIDTH;

    SharedArrayPtr<unsigned char> nodes(new unsigned char[sizeof(stbrp_node) * numnodes]);
    SharedArrayPtr<unsigned char> rects(new unsigned char[sizeof(stbrp_rect) * workingSet_.Size() + 1]);

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

    String filename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightMapIndex);
    image->SavePNG(filename);

    workingSet_.Clear();

}

bool SceneBaker::Light()
{
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

        //baker->TraceAORays(256, 1.0f);
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
            String filename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Textures/Scene_Lightmap%i.png", lightmapIndex);
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

    String scenefilename = ToString("/Users/jenge/Dev/atomic/AtomicTests/AtomicGlowTest/Resources/Scenes/LitScene.scene");

    File saveFile(context_, scenefilename, FILE_WRITE);
    scene_->SaveXML(saveFile);

    return true;

}

bool SceneBaker::Preprocess()
{
    Vector<SharedPtr<StaticModelBaker>>::Iterator itr = staticModelBakers_.Begin();

    while (itr != staticModelBakers_.End())
    {
        (*itr)->Preprocess();
        itr++;
    }

    rtcCommit(rtcScene_);

    return true;
}

static bool CompareStaticModelBaker(StaticModelBaker* lhs, StaticModelBaker* rhs)
{
    int lhsWeight = lhs->GetLightmap() ? lhs->GetLightmap()->GetWidth() : 0;
    lhsWeight += lhs->GetLightmap() ? lhs->GetLightmap()->GetHeight() : 0;

    int rhsWeight = rhs->GetLightmap() ? rhs->GetLightmap()->GetWidth() : 0;
    rhsWeight += rhs->GetLightmap() ? rhs->GetLightmap()->GetHeight() : 0;

    return lhsWeight < rhsWeight;
}


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


    sunDir_ = Vector3(-0.6f, 1.0f, -0.8f);
    sunDir_.Normalize();

    PODVector<Node*> lightNodes;
    scene_->GetChildrenWithComponent<Atomic::Light>(lightNodes, true);

    for (unsigned i = 0; i < lightNodes.Size(); i++)
    {
        Atomic::Light* light = lightNodes[i]->GetComponent<Atomic::Light>();

        if (light->GetLightType() == LIGHT_DIRECTIONAL)
        {
            sunDir_ = lightNodes[i]->GetDirection();
            sunDir_ = -sunDir_;
            sunDir_.Normalize();
        }
    }

    PODVector<StaticModel*> staticModels;
    scene_->GetComponents<StaticModel>(staticModels, true);

    for (unsigned i = 0; i < staticModels.Size(); i++)
    {
        StaticModel* staticModel = staticModels[i];

        if (staticModel->GetModel() && (staticModel->GetLightmap() ||staticModel->GetCastShadows()))
        {
            StaticModelBaker* baker = new StaticModelBaker(context_, this, staticModel);
            staticModelBakers_.Push(SharedPtr<StaticModelBaker>(baker));
        }

    }

    // Sort by lightmap size
    Sort(staticModelBakers_.Begin(), staticModelBakers_.End(), CompareStaticModelBaker);

    return true;
}

}
