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

#include <Atomic/Resource/Image.h>

#include "LightMap.h"

namespace AtomicGlow
{

LightMap::LightMap(Context* context) : Object(context)
{

}

LightMap::~LightMap()
{

}

void LightMap::BoxFilterImage()
{
    // Box 3x3 Filter

    SharedPtr<Image> tmp(new Image(context_));
    tmp->SetSize(image_->GetWidth(), image_->GetHeight(), image_->GetComponents());
    tmp->Clear(Color::BLACK);

    for (int y = 0; y < image_->GetHeight(); y++)
    {
        for (int x = 0; x < image_->GetWidth(); x++)
        {
            Color color = image_->GetPixel(x-1, y);
            color += image_->GetPixel(x, y);
            color += image_->GetPixel(x+1, y);

            color.r_ /= 3.0f;
            color.g_ /= 3.0f;
            color.b_ /= 3.0f;

            tmp->SetPixel(x, y, color);
        }
    }

    for (int y = 0; y < image_->GetHeight(); y++)
    {
        for (int x = 0; x < image_->GetWidth(); x++)
        {
            Color color = tmp->GetPixel(x, y-1);
            color += tmp->GetPixel(x, y);
            color += tmp->GetPixel(x, y+1);

            color.r_ /= 3.0f;
            color.g_ /= 3.0f;
            color.b_ /= 3.0f;

            image_->SetPixel(x, y, color);

        }

    }

}

}
