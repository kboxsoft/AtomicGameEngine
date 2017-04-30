//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
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

#if defined(_WIN32) || defined(_WIN64)
#define fmax max
#define fmin min
#endif

#include "../Graphics/Drawable.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Material.h"
#include "../Graphics/Zone.h"
#include "../Graphics/StaticModel.h"
#include "../Scene/Node.h"

namespace Atomic
{


class Water : public StaticModel
{
    ATOMIC_OBJECT(Water, Drawable);

public:
    /// Construct.
    ///
	Water(Context *context)
		: StaticModel(context), initialized_(false)
	{
	}

	virtual ~Water()
	{
	}

    /// Register object factory. Drawable must be registered first.
    static void RegisterObject(Context* context);


    void SetAutoUpdate(bool autoUpdate)
    {
        autoUpdate_ = autoUpdate;
    }

 

protected:
	SharedPtr<Scene> scene_;
	SharedPtr<Node> cameraNode_;
	Plane waterPlane_;
	Plane waterClipPlane_;
	SharedPtr<Node> waterNode_;
	SharedPtr<Node> reflectionCameraNode_;
    SharedPtr<Texture2D> waterTexture_;
    SharedPtr<Material> waterMaterial_;
    SharedPtr<Light> sunlight_;

    bool autoUpdate_;

    void OnNodeSet(Node* node);

    void HandleSceneUpdate(StringHash eventType, VariantMap& eventData);
    void HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData);

    void Initialize();
	bool initialized_;
	bool flipped_;
    void ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results);

	float fogStart_;
	float fogEnd_;
	Color fogColor_;

};

}
