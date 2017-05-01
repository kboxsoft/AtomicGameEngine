//
// Copyright (c) 2014-2016, THUNDERBEAST GAMES LLC All rights reserved
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

#include "Precompiled.h"
#include "../Core/Context.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/GraphicsEvents.h"
#include "../Graphics/View.h"
#include "../Scene/Scene.h"
#include "../Graphics/Camera.h"
#include "../Scene/SceneEvents.h"
#include "../Graphics/Light.h"
#include "../Resource/ResourceCache.h"
#include "../Graphics/Technique.h"
#include "../Environment/Water.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Drawable.h"
#include "../IO/Log.h"
#include "../Graphics/RenderPath.h"
#include "../Resource/XMLFile.h"

#if defined(_MSC_VER)
#include "stdint.h"
#endif

#if defined(EMSCRIPTEN) || defined(ATOMIC_PLATFORM_LINUX)
#include <stdint.h>
#endif

namespace Atomic
{

extern const char* GEOMETRY_CATEGORY;


void Water::RegisterObject(Context* context)
{
    context->RegisterFactory<Water>(GEOMETRY_CATEGORY);
    ATOMIC_COPY_BASE_ATTRIBUTES(Drawable);
}


void Water::OnNodeSet(Node* node)
{
    Drawable::OnNodeSet(node);

    if (node && node->GetScene())
    {
        SubscribeToEvent(node->GetScene(), E_SCENEPOSTUPDATE, ATOMIC_HANDLER(Water, HandleSceneUpdate));
        SubscribeToEvent(E_BEGINVIEWUPDATE, ATOMIC_HANDLER(Water, HandleBeginViewUpdate));
    }

}

void Water::HandleBeginViewUpdate(StringHash eventType, VariantMap& eventData)
{
#ifdef ATOMIC_OPENGL
    flipped_ = false;
    View* view = static_cast<View*>(eventData[BeginViewUpdate::P_VIEW].GetPtr());
    if (view && view->GetRenderTarget())
        flipped_ = true;

#endif

}

void Water::HandleSceneUpdate(StringHash eventType, VariantMap& eventData)
{

    using namespace SceneUpdate;

    if (!initialized_)
        Initialize();

    float timeStep = eventData[P_TIMESTEP].GetFloat();

    if (autoUpdate_)
    {
		// In case resolution has changed, adjust the reflection camera aspect ratio
		Graphics* graphics = GetSubsystem<Graphics>();
		Camera* reflectionCamera = reflectionCameraNode_->GetComponent<Camera>();
		reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());


		ResourceCache *cache = GetSubsystem<ResourceCache>();
		Viewport *viewport = GetSubsystem<Renderer>()->GetViewport(0);
		RenderPath* effectRenderPath = viewport->GetRenderPath();

		//Check if under water
		Zone *zone = scene_->GetComponent<Zone>(true);
		if (zone)
		{
			Vector3 camPos = cameraNode_->GetPosition();
			Vector3 waterPos = waterNode_->GetPosition();
			StaticModel* water = waterNode_->GetComponent<StaticModel>();
			BoundingBox bounds = water->GetWorldBoundingBox();
			//We need the camera to be under the water plane but also within its x / z dimensions.
			if (camPos.y_ < waterPos.y_
				&& camPos.x_ < bounds.max_.x_ && camPos.z_ < bounds.max_.z_
				&& camPos.x_ > bounds.min_.x_ && camPos.z_ > bounds.min_.z_)
			{
				zone->SetFogStart(0);
				zone->SetFogEnd(200);
				zone->SetFogColor(Color(0.24 , 0.39, 0.47));
				waterNode_->SetRotation(Quaternion(180, Vector3::LEFT));
				effectRenderPath->SetEnabled("Underwater", true);
			}
			else {
				zone->SetFogStart(fogStart_);
				zone->SetFogEnd(fogEnd_);
				zone->SetFogColor(fogColor_);
				waterNode_->SetRotation(Quaternion::IDENTITY);
				effectRenderPath->SetEnabled("Underwater", false);
			}

		}
    }

}



void Water::Initialize()
{
	Graphics* graphics = GetSubsystem<Graphics>();
	Renderer* renderer = GetSubsystem<Renderer>();
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	Viewport *viewport = GetSubsystem<Renderer>()->GetViewport(0);
	RenderPath* effectRenderPath = viewport->GetRenderPath();
	effectRenderPath->Append(cache->GetResource<XMLFile>("PostProcess/Underwater.xml"));
	effectRenderPath->SetEnabled("Underwater", false);

	scene_ = GetScene();

	Zone *zone = scene_->GetComponent<Zone>(true);
	if (zone)
	{
		fogStart_ = zone->GetFogStart();
		fogEnd_ = zone->GetFogEnd();
		fogColor_ = zone->GetFogColor();
	}

	PODVector<Node*> cameraNodes;
	Camera* camera = 0;
	scene_->GetChildrenWithComponent(cameraNodes, Camera::GetTypeStatic(), true);
	if (!cameraNodes.Size())
	{
		ATOMIC_LOGERROR("Water couldn't find main camera");
	}
	camera = cameraNodes[0]->GetComponent<Camera>();

	cameraNode_ = cameraNodes[0];
	camera = cameraNodes[0]->GetComponent<Camera>();

	// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen
//	SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
	//renderer->SetViewport(0, viewport);
	waterNode_ = GetNode();

	StaticModel* water = waterNode_->GetComponent<StaticModel>();
	//water->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
	//water->SetMaterial(cache->GetResource<Material>("Materials/Water.xml"));
	// Set a different viewmask on the water plane to be able to hide it from the reflection camera
	water->SetViewMask(0x80000000);

	// Create a mathematical plane to represent the water in calculations
	waterPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f), waterNode_->GetWorldPosition());
	// Create a downward biased plane for reflection view clipping. Biasing is necessary to avoid too aggressive clipping
	waterClipPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f), waterNode_->GetWorldPosition() -
		Vector3(0.0f, 0.1f, 0.0f));

	// Create camera for water reflection
	// It will have the same farclip and position as the main viewport camera, but uses a reflection plane to modify
	// its position when rendering
	reflectionCameraNode_ = cameraNode_->CreateChild();
	Camera* reflectionCamera = reflectionCameraNode_->CreateComponent<Camera>();
	reflectionCamera->SetFarClip(750.0);
	reflectionCamera->SetViewMask(0x7fffffff); // Hide objects with only bit 31 in the viewmask (the water plane)
	reflectionCamera->SetAutoAspectRatio(false);
	reflectionCamera->SetUseReflection(true);
	reflectionCamera->SetReflectionPlane(waterPlane_);
	reflectionCamera->SetUseClipping(true); // Enable clipping of geometry behind water plane
	reflectionCamera->SetClipPlane(waterClipPlane_);
	// The water reflection texture is rectangular. Set reflection camera aspect ratio to match
	reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());
	// View override flags could be used to optimize reflection rendering. For example disable shadows
	//reflectionCamera->SetViewOverrideFlags(VO_DISABLE_SHADOWS);

	// Create a texture and setup viewport for water reflection. Assign the reflection texture to the diffuse
	// texture unit of the water material
	int texSize = 1024;
	SharedPtr<Texture2D> renderTexture(new Texture2D(context_));
	renderTexture->SetSize(texSize, texSize, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
	renderTexture->SetFilterMode(FILTER_BILINEAR);
	RenderSurface* surface = renderTexture->GetRenderSurface();
	SharedPtr<Viewport> rttViewport(new Viewport(context_, scene_, reflectionCamera));
	surface->SetViewport(0, rttViewport);
	Material* waterMat = cache->GetResource<Material>("Materials/DeepWater.xml");
	waterMat->SetTexture(TU_DIFFUSE, renderTexture);
	water->SetMaterial(waterMat);
    initialized_ = true;
	autoUpdate_ = true;

}

void Water::ProcessRayQuery(const RayOctreeQuery& query, PODVector<RayQueryResult>& results)
{
    // Do not record a raycast result for water as it would block all other results
}




}
