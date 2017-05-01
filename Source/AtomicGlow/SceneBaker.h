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

#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#include <Atomic/Container/List.h>
#include <Atomic/Scene/Scene.h>

#include "StaticModelBaker.h"

using namespace Atomic;

namespace AtomicGlow
{

// fixme: this needs to be configurable
const int LIGHTMAP_WIDTH = 1024;
const int LIGHTMAP_HEIGHT = 1024;

class SceneBaker : public Object
{
    ATOMIC_OBJECT(SceneBaker, Object)

    public:

    SceneBaker(Context* context);
    virtual ~SceneBaker();

    bool Preprocess();

    bool Light();

    bool LoadScene(const String& filename);

    RTCDevice GetRTCDevice() const { return rtcDevice_; }
    RTCScene GetRTCScene() const { return rtcScene_; }

    //TODO: remove me, temporary sunlight info for testing
    Vector3 sunDir_;

private:

    void EmitLightmap(int lightMapIndex);

    bool TryAddStaticModelBaker(StaticModelBaker *bakeModel);

    SharedPtr<Scene> scene_;

    Vector<SharedPtr<StaticModelBaker>> staticModelBakers_;

    PODVector<StaticModelBaker*> workingSet_;

    RTCDevice rtcDevice_;
    RTCScene rtcScene_;

};

}
