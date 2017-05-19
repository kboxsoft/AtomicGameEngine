
#include "LightMap.h"
#include "BakeMesh.h"
#include "RadianceMap.h"

namespace AtomicGlow
{

RadianceMap::RadianceMap(Context* context, BakeMesh* bakeMesh) : Object(context),
    bakeMesh_(bakeMesh),
    image_(new Image(context_)),
    packed_(false)
{

    image_->SetSize(bakeMesh->radianceWidth_, bakeMesh->radianceHeight_, 3);

    Color ambient = bakeMesh->GetAmbientColor();

    bool useDebugFillColor = false;

    if (useDebugFillColor)
    {
        ambient = Color::MAGENTA;
    }

    image_->Clear(ambient);

    Color c;
    for (unsigned y = 0; y < bakeMesh->radianceHeight_; y++)
    {
        for (unsigned x = 0; x < bakeMesh->radianceWidth_; x++)
        {
            const Vector3& rad = bakeMesh->radiance_[y * bakeMesh->radianceWidth_ + x];

            if (rad.x_ >= 0.0f)
            {
                Vector3 r = rad;

                if (r.Length() > 3.0f)
                {
                    r.Normalize();
                    r *= 3.0f;
                }

                c.r_ = r.x_;
                c.g_ = r.y_;
                c.b_ = r.z_;

                image_->SetPixel(x, y, c);
            }
        }
    }

    Blur();
    Downsample();

}

void RadianceMap::Blur()
{
    int width = image_->GetWidth();
    int height = image_->GetHeight();

    SharedPtr<Image> target(new Image(context_));
    target->SetSize(width, height, 3);

    Color color;
    float validPixels;
    int minK, maxK, minL, maxL;
    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            if (image_->GetPixel(i, j) == Color::BLACK)
                continue;

            color = Color::BLACK;
            validPixels = 0;

            minK = i - 1;
            if (minK < 0)
                minK = 0;
            maxK = i + 1;
            if (maxK >= width)
                maxK = width - 1;
            minL = j - 1;
            if (minL < 0)
                minL = 0;
            maxL = j + 1;
            if (maxL >= height)
                maxL = height - 1;

            for (int k = minK; k <= maxK; ++k)
            {
                for (int l = minL - 1; l < maxL; ++l)
                {
                    if (image_->GetPixel(k, l) == Color::BLACK)
                        continue;

                    color += image_->GetPixel(k, l);
                    ++validPixels;
                }
            }
            color.r_ /= validPixels;
            color.g_ /= validPixels;
            color.b_ /= validPixels;
            target->SetPixel(i, j, color);
        }
    }

    image_->SetData(target->GetData());
}


bool RadianceMap::Downsample()
{
    // Simple average downsample gives nice results

    SharedPtr<Image> tmp(new Image(context_));
    tmp->SetSize(image_->GetWidth()/2, image_->GetHeight()/2, 3);

    for (int y = 0; y < tmp->GetHeight(); y++)
    {
        for (int x = 0; x < tmp->GetWidth(); x++)
        {
            Color c = image_->GetPixel(x * 2, y * 2);
            c +=  image_->GetPixel(x * 2 + 1, y * 2);
            c +=  image_->GetPixel(x * 2, y * 2 + 1);
            c +=  image_->GetPixel(x * 2 + 1, y * 2 + 1);
            c.r_ /= 4.0f;
            c.g_ /= 4.0f;
            c.b_ /= 4.0f;
            c.a_ = 1.0f;
            tmp->SetPixel(x, y, c);
        }

    }

    image_->SetSize(tmp->GetWidth(), tmp->GetHeight(), 3);
    image_->SetData(tmp->GetData());

    return true;


}

RadianceMap::~RadianceMap()
{

}

}
