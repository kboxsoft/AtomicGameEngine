
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

    image_->Clear(Color::MAGENTA);

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

    // We need to add per radiance pixel/tri information so don't
    // consider other tri radiance when blurring, etc otherwise
    // introduces artifacts
    //Blur();
    //Downsample();
    //FillInvalidRadiance(3);

}

struct CoordDistanceComparer
{
    bool operator() (Pair<int, int> &left, Pair<int, int> &right)
    {
        return (left.first_*left.first_ + left.second_*left.second_) <
            (right.first_*right.first_ + right.second_*right.second_);
    }
};

void RadianceMap::BuildSearchPattern(int searchSize, Vector<Pair<int, int>>& searchPattern)
{
    searchPattern.Clear();
    for (int i = -searchSize; i <= searchSize; ++i)
    {
        for (int j = -searchSize; j <= searchSize; ++j)
        {
            if (i == 0 && j == 0)
                continue;
            searchPattern.Push(Pair<int, int>(i, j));
        }
    }
    CoordDistanceComparer comparer;
    Sort(searchPattern.Begin(), searchPattern.End(), comparer);
}

void RadianceMap::FillInvalidRadiance(int bleedRadius)
{
    Vector<Pair<int, int>> searchPattern;
    BuildSearchPattern(bleedRadius, searchPattern);

    Vector<Pair<int, int> >::Iterator iter;

    int width = image_->GetWidth();
    int height = image_->GetHeight();

    SharedPtr<Image> target(new Image(context_));
    target->SetSize(width, height, 3);
    target->Clear(Color::BLACK);

    for (int i = 0; i < width; ++i)
    {
        for (int j = 0; j < height; ++j)
        {
            Color c = image_->GetPixel(i, j);

            if ( c != Color::MAGENTA)
            {
                target->SetPixel(i, j, c);
                continue;
            }

            // Invalid pixel found
            for (iter = searchPattern.Begin(); iter != searchPattern.End(); ++iter)
            {
                int x = i + iter->first_;
                int y = j + iter->second_;

                if (x < 0 || x >= width)
                    continue;

                if (y < 0 || y >= width)
                    continue;

                // If search pixel is valid assign it to the invalid pixel and stop searching
                c = image_->GetPixel(x, y);
                if (c != Color::MAGENTA)
                {
                    target->SetPixel(x, y, c);
                    break;
                }
            }
        }
    }

    image_->SetData(target->GetData());
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
    tmp->Clear(Color::BLACK);

    for (int y = 0; y < tmp->GetHeight(); y++)
    {
        for (int x = 0; x < tmp->GetWidth(); x++)
        {

            int validColors = 0;

            Color c = Color::BLACK;

            Color tc = image_->GetPixel(x * 2, y * 2);
            if (tc != Color::BLACK)
            {
                c += tc;
                validColors++;
            }

            tc = image_->GetPixel(x * 2 + 1, y * 2);
            if (tc != Color::BLACK)
            {
                c += tc;
                validColors++;
            }

            tc = image_->GetPixel(x * 2, y * 2 + 1);
            if (tc != Color::BLACK)
            {
                c += tc;
                validColors++;
            }

            tc = image_->GetPixel(x * 2 + 1, y * 2 + 1);
            if (tc != Color::BLACK)
            {
                c += tc;
                validColors++;
            }

            if (!validColors)
                continue;

            c.r_ /= validColors;
            c.g_ /= validColors;
            c.b_ /= validColors;
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
