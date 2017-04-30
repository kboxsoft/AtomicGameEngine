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

#include <Atomic/IO/Log.h>
#include <Atomic/Resource/ResourceCache.h>
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

bool SceneBaker::Light()
{
    List<SharedPtr<StaticModelBaker>>::Iterator itr = staticModelBakers_.Begin();

    while (itr != staticModelBakers_.End())
    {
        //(*itr)->TraceAORays(256, 0.5f);
        (*itr)->TraceSunLight();
        (*itr)->ProcessLightmap();
        itr++;
    }

    return true;

}

bool SceneBaker::Preprocess()
{
    List<SharedPtr<StaticModelBaker>>::Iterator itr = staticModelBakers_.Begin();

    while (itr != staticModelBakers_.End())
    {
        (*itr)->Preprocess();
        itr++;
    }

    rtcCommit(rtcScene_);

    return true;
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

    PODVector<StaticModel*> staticModels;
    scene_->GetComponents<StaticModel>(staticModels, true);

    for (unsigned i = 0; i < staticModels.Size(); i++)
    {
        StaticModel* staticModel = staticModels[i];

        if (staticModel->GetLightmap() && staticModel->GetModel())
        {
            StaticModelBaker* baker = new StaticModelBaker(context_, this, staticModel);
            staticModelBakers_.Push(SharedPtr<StaticModelBaker>(baker));
        }

    }

    return true;
}

}
