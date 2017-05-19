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

#include <ThirdParty/STB/stb_rect_pack.h>

#include <Atomic/IO/Log.h>
#include <Atomic/Resource/Image.h>

#include "BakeMesh.h"
#include "LightMapPacker.h"

namespace AtomicGlow
{

LightMapPacker::LightMapPacker(Context* context) : Object(context)
{

}

LightMapPacker::~LightMapPacker()
{

}

void LightMapPacker::AddRadianceMap(RadianceMap* radianceMap)
{
    if (!radianceMap)
        return;

    radMaps_.Push(SharedPtr<RadianceMap>(radianceMap));

}


bool LightMapPacker::TryAddRadianceMap(RadianceMap* radMap)
{

    if (radMap->packed_)
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
        RadianceMap* wmap = workingSet_[i];

        rect->id = (int) i;
        rect->w = wmap->GetWidth();
        rect->h = wmap->GetHeight();

        rect++;
    }

    rect->id = (int) workingSet_.Size();
    rect->w = radMap->GetWidth();
    rect->h = radMap->GetHeight();


    if (!stbrp_pack_rects (&rectContext, (stbrp_rect *)rects.Get(), workingSet_.Size() + 1))
    {
        return false;
    }

    return true;

}

void LightMapPacker::EmitLightmap(unsigned lightMapID)
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
        RadianceMap* radMap = workingSet_[i];

        rect->id = (int) i;
        rect->w = radMap->GetWidth();
        rect->h = radMap->GetHeight();

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

    Color ambient = Color::BLACK;

    for (unsigned i = 0; i < workingSet_.Size(); i++)
    {
        RadianceMap* radMap = workingSet_[i];

        if (!rect->was_packed)
        {
            ATOMIC_LOGERROR("LightMapPacker::Light() - skipping unpacked lightmap");
            continue;
        }

        image->SetSubimage(radMap->image_, IntRect(rect->x, rect->y, rect->x + radMap->image_->GetWidth(), rect->y + radMap->image_->GetHeight()));

        radMap->bakeMesh_->Pack(lightMapID, Vector4(float(radMap->image_->GetWidth())/float(width),
                                                    float(radMap->image_->GetHeight())/float(height),
                                                    float(rect->x)/float(width),
                                                    float(rect->y)/float(height)));

        if (ambient == Color::BLACK)
        {
            // TODO, different zone ambients?
            ambient = radMap->bakeMesh_->GetAmbientColor();
        }

        rect++;
    }    

    // TODO: add this as a config option?
    bool useDebugFillColor = false;

    if (useDebugFillColor)
    {
        ambient = Color::MAGENTA;
    }

    if (ambient != Color::BLACK)
    {
        for (int y = 0; y < image->GetHeight() ; y++)
        {
            for (int x = 0; x < image->GetWidth(); x++)
            {
                if (image->GetPixel(x, y) == Color::BLACK)
                {
                    image->SetPixel(x, y, ambient);
                }
            }
        }
    }

    SharedPtr<LightMap> lightmap(new LightMap(context_));
    lightMaps_.Push(lightmap);

    lightmap->SetID(lightMapID);
    lightmap->SetImage(image);

    workingSet_.Clear();

}

void LightMapPacker::SaveLightmaps()
{
    for (unsigned i = 0; i < lightMaps_.Size(); i++)
    {
        LightMap* lightmap = lightMaps_[i];

#ifdef ATOMIC_PLATFORM_WINDOWS
        String filename = ToString("C:/Dev/atomic/AtomicExamplesPrivate/AtomicGlowTests/TestScene1/Resources/Textures/Scene_Lightmap%u.png", lightmap->GetID());
#else
        String filename = ToString("/Users/jenge/Dev/atomic/AtomicExamplesPrivate/AtomicGlowTests/TestScene1/Resources/Textures/Scene_Lightmap%u.png", lightmap->GetID());
#endif

        lightmap->GetImage()->SavePNG(filename);

    }

}

static bool CompareRadianceMap(RadianceMap* lhs, RadianceMap* rhs)
{
    int lhsWeight = lhs->GetWidth();
    lhsWeight += lhs->GetHeight();

    int rhsWeight = rhs->GetWidth();
    rhsWeight += rhs->GetHeight();

    // sort from biggest to smallest
    return lhsWeight > rhsWeight;
}


void LightMapPacker::Pack()
{
    unsigned lightmapID = 0;

    SharedPtr<LightMap> lightmap;

    Sort(radMaps_.Begin(), radMaps_.End(), CompareRadianceMap);

    for (unsigned i = 0; i < radMaps_.Size(); i++)
    {
        RadianceMap* radMap = radMaps_[i];

        if (radMap->packed_)
            continue;

        if (radMap->GetWidth() >= LIGHTMAP_WIDTH || radMap->GetHeight() >= LIGHTMAP_HEIGHT)
        {
            lightmap = new LightMap(context_);
            lightMaps_.Push(lightmap);

            lightmap->SetID(lightmapID);
            lightmap->SetImage(radMap->image_);

            radMap->bakeMesh_->Pack(lightmapID, Vector4(1.0f, 1.0f, 0.0f, 0.0f));

            lightmapID++;

            continue;
        }

        workingSet_.Push(radMap);

        for (unsigned j = 0; j < radMaps_.Size(); j++)
        {
            if (i == j)
                continue;

            RadianceMap* otherMap = radMaps_[j];

            if (TryAddRadianceMap(otherMap))
            {
                workingSet_.Push(otherMap);
            }

        }

        EmitLightmap(lightmapID++);
        workingSet_.Clear();

    }

}


}
