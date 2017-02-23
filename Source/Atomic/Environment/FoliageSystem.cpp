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
#include "../Environment/FoliageSystem.h"
#include "../Graphics/Renderer.h"
#include <Atomic/Math/Vector3.h>
#include <Atomic/IO/Log.h>
#if defined(_MSC_VER)
#include "stdint.h"
#endif

#if defined(EMSCRIPTEN) || defined(ATOMIC_PLATFORM_LINUX)
#include <stdint.h>
#endif

namespace Atomic
{

	extern const char* GEOMETRY_CATEGORY;

	FoliageSystem::FoliageSystem(Context *context) : Component(context)
	{
		initialized_ = false;
		context_ = context;
	}

	FoliageSystem::~FoliageSystem()
	{
	}

	//void FoliageSystem::ApplyAttributes()
	//{
	
	//}



	void FoliageSystem::RegisterObject(Context* context)
	{
		context->RegisterFactory<FoliageSystem>(GEOMETRY_CATEGORY);
		ATOMIC_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
	}


	void FoliageSystem::HandleComponentRemoved(StringHash eventType, VariantMap& eventData)
	{
		Component* component = static_cast<Component*> (eventData[Atomic::ComponentRemoved::P_COMPONENT].GetPtr());
		if (component == this) {
			for (HashMap<IntVector2, GeomReplicator*>::Iterator i = vegReplicators_.Begin(); i != vegReplicators_.End(); ++i)
			{
			//	i->second_->Remove();
			}
		}

	}


	void FoliageSystem::Initialize()
	{
		initialized_ = true;
		SubscribeToEvent(node_->GetScene(), E_COMPONENTREMOVED, ATOMIC_HANDLER(FoliageSystem, HandleComponentRemoved));
	}

	void FoliageSystem::OnSetEnabled()
	{
		bool enabled = IsEnabledEffective();

		for (HashMap<IntVector2, GeomReplicator*>::Iterator i = vegReplicators_.Begin(); i != vegReplicators_.End(); ++i)
		{
		//	i->second_->SetEnabled(false);
		}

	}



	void FoliageSystem::OnNodeSet(Node* node)
	{
		if (node && !initialized_)
		{
			node_ = node;
			node->AddListener(this);

			PODVector<Terrain*> terrains;
			node->GetDerivedComponents<Terrain>(terrains);

			if (terrains.Size() > 0)
			{
				terrain_ = terrains[0];
				SubscribeToEvent(node->GetScene(), E_SCENEDRAWABLEUPDATEFINISHED, ATOMIC_HANDLER(FoliageSystem, HandleDrawableUpdateFinished));
				SubscribeToEvent(E_BEGINFRAME, ATOMIC_HANDLER(FoliageSystem, HandlePostUpdate));
				// TODO: Make this better
				// If we try to get height of the terrain right away it will be zero because it's not finished loading. So I wait until the scene has finished
				// updating all its drawables (for want of a better event) and then initialize the grass if it isn't already initialized.
			}
		}
	}
	void FoliageSystem::HandleDrawableUpdateFinished(StringHash eventType, VariantMap& eventData)
	{
		if (!initialized_)
			Initialize();
		this->UnsubscribeFromEvent(E_SCENEDRAWABLEUPDATEFINISHED);
		
	}

	void FoliageSystem::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
	{
		if (!initialized_)
			return;

		Renderer* r = GetSubsystem<Renderer>();
		if (!r)
			return;

		Viewport* viewport = r->GetViewport(0);
		if (!viewport)
			return;

		IntVector2 terrainsize = (terrain_->GetNumPatches() * terrain_->GetPatchSize());
		IntVector2 cellsize = terrainsize / 16;

		Camera *cam =  viewport->GetCamera();
		if (cam) {
			Vector3 campos = cam->GetNode()->GetPosition();
			campos.y_ = 0;

			IntVector2 campos2d = IntVector2(campos.x_, campos.z_);


			IntVector2 sector = IntVector2(  floor(campos2d.x_ / cellsize.x_) -1, floor(campos2d.y_ / cellsize.y_));
		

			ATOMIC_LOGDEBUG(sector.ToString());

			
			if (lastSector_ != sector)
			{

				PODVector<IntVector2> activeset;

				activeset.Push(sector);
				activeset.Push(sector + IntVector2(1, 1));
				activeset.Push(sector + IntVector2(1, -1));
				activeset.Push(sector + IntVector2(-1, 1));
				activeset.Push(sector + IntVector2(-1, -1));
				activeset.Push(sector + IntVector2(1, 0));
				activeset.Push(sector + IntVector2(0, 1));
				activeset.Push(sector + IntVector2(-1, 0));
				activeset.Push(sector + IntVector2(0, -1));

				//grass remove unused
				for (HashMap<IntVector2, GeomReplicator*>::Iterator i = vegReplicators_.Begin(); i != vegReplicators_.End(); ++i) {
					if (!activeset.Contains(i->first_)) {
						i->second_->Remove();
						vegReplicators_.Erase(i->first_);
					}
				}
				////trees remove unused
				//for (HashMap<IntVector2, GeomReplicator*>::Iterator i = treeReplicators_.Begin(); i != treeReplicators_.End(); ++i) {
				//	if (!activeset.Contains(i->first_)) {
				//		i->second_->Remove();
				//		treeReplicators_.Erase(i->first_);
				//	}
				//}

				//sectorSet_ = true;
				lastSector_ = sector;

				//grass create new/missing
				for (PODVector<IntVector2> ::Iterator i = activeset.Begin(); i != activeset.End(); ++i) {
					if (!vegReplicators_.Contains(i->Data())) {
						DrawGrass(i->Data(), cellsize);
					}
				}

				////trees create new/missing
				//for (PODVector<IntVector2> ::Iterator i = activeset.Begin(); i != activeset.End(); ++i) {
				//	if (!treeReplicators_.Contains(i->Data())) {
				//		DrawTrees(i->Data(), cellsize);
				//	}
				//}
				
				//DrawGrass(sector, cellsize);
				//DrawGrass(sector + IntVector2(1,1), cellsize);
				//DrawGrass(sector + IntVector2(1, -1), cellsize);
				//DrawGrass(sector + IntVector2(-1, 1), cellsize);
				//DrawGrass(sector + IntVector2(-1, -1), cellsize);
				//DrawGrass(sector + IntVector2(1, 0), cellsize);
				//DrawGrass(sector + IntVector2(0, 1), cellsize);
				//DrawGrass(sector + IntVector2(-1, 0), cellsize);
				//DrawGrass(sector + IntVector2(0, -1), cellsize);
			}
		}
	}

	void FoliageSystem::DrawTrees(IntVector2 sector, IntVector2 cellsize) {
		const unsigned NUM_OBJECTS = 10;

		if (!terrain_) {
			ATOMIC_LOGERROR("Foliage system couldn't find terrain");
			return;
		}
		Vector3 position = Vector3((sector.x_ * cellsize.x_), 0, (sector.y_ * cellsize.y_));
		ATOMIC_LOGDEBUG("New trees " + position.ToString() + " Sector: " + sector.ToString());
		ResourceCache* cache = GetSubsystem<ResourceCache>();

		PODVector<PRotScale> qpList_;
		//	Vector3 rotatedpos = (rot.Inverse() * qp.pos);  //  (rot.Inverse() * qp.pos) + terrainpos;
		for (unsigned i = 0; i < NUM_OBJECTS; ++i)
		{
			PRotScale qp;


			qp.pos = (node_->GetRotation().Inverse() * Vector3(Random(cellsize.x_*5), 0.0f, Random(cellsize.y_*5))) + (node_->GetRotation().Inverse() * position);
			qp.rot = Quaternion(0.0f, Random(360.0f), 0.0f);
			qp.pos.y_ = terrain_->GetHeight(node_->GetRotation() * qp.pos) - 2.2f;
			qp.scale = 7.5f + Random(11.0f);
			qpList_.Push(qp);
		}

		Model *pModel = cache->GetResource<Model>("Models/Veg/vegbrush.mdl");
		SharedPtr<Model> cloneModel = pModel->Clone();


		Node *treenode = node_->CreateChild();
		GeomReplicator *trees = treenode->CreateComponent<GeomReplicator>();
		trees->SetModel(cloneModel);
		trees->SetMaterial(cache->GetResource<Material>("Models/Veg/trees-alphamask.xml"));

		Vector3 lightDir(0.6f, -1.0f, 0.8f);

		lightDir = -1.0f * lightDir.Normalized();
		trees->Replicate(qpList_, lightDir);

		// specify which verts in the geom to move
		// - for the vegbrush model, the top two vertex indeces are 2 and 3
		PODVector<unsigned> topVerts;
		topVerts.Push(2);
		topVerts.Push(3);

		// specify the number of geoms to update at a time
		unsigned batchCount = 10000;

		// wind velocity (breeze velocity shown)
		Vector3 windVel(0.1f, -0.1f, 0.1f);

		// specify the cycle timer
		float cycleTimer = 1.4f;

		trees->ConfigWindVelocity(topVerts, batchCount, windVel, cycleTimer);
		trees->WindAnimationEnabled(true);

		treeReplicators_.InsertNew(sector, trees);

	}

	Vector2 FoliageSystem::CustomWorldToNormalized(Image *height, Terrain *terrain, Vector3 world)
	{
		if (!terrain || !height) return Vector2(0, 0);
		Vector3 spacing = terrain->GetSpacing();
		int patchSize = terrain->GetPatchSize();
		IntVector2 numPatches = IntVector2((height->GetWidth() - 1) / patchSize, (height->GetHeight() - 1) / patchSize);
		Vector2 patchWorldSize = Vector2(spacing.x_*(float)(patchSize*numPatches.x_), spacing.z_*(float)(patchSize*numPatches.y_));
		Vector2 patchWorldOrigin = Vector2(-0.5f * patchWorldSize.x_, -0.5f * patchWorldSize.y_);
		return Vector2((world.x_ - patchWorldOrigin.x_) / patchWorldSize.x_, (world.z_ - patchWorldOrigin.y_) / patchWorldSize.y_);
	}


	void FoliageSystem::DrawGrass(IntVector2 sector, IntVector2 cellsize) {
		const unsigned NUM_OBJECTS = 1000;

		if (!terrain_){
			ATOMIC_LOGERROR("Foliage system couldn't find terrain");
			return;
		}
		Vector3 position = Vector3((sector.x_ * cellsize.x_), 0, (sector.y_ * cellsize.y_));
		ATOMIC_LOGDEBUG("New grass " + position.ToString() + " Sector: " + sector.ToString());
		ResourceCache* cache = GetSubsystem<ResourceCache>();

		Texture2D* splattex = (Texture2D*)terrain_->GetMaterial()->GetTexture(TU_DIFFUSE);
		Image* splatmap;
		if (splattex) {
			String splattexname = splattex->GetName();
			splatmap = cache->GetResource<Image>(splattexname);
		}

		Quaternion rot = terrain_->GetNode()->GetRotation();
		Image* height = terrain_->GetHeightMap();
		float ratio = ((float)splatmap->GetWidth() / (float)height->GetWidth());

		PODVector<PRotScale> qpList_;
	//	Vector3 rotatedpos = (rot.Inverse() * qp.pos);  //  (rot.Inverse() * qp.pos) + terrainpos;
		for (unsigned i = 0; i < NUM_OBJECTS; ++i)
		{
			PRotScale qp;
			

			qp.pos = (node_->GetRotation().Inverse() * Vector3(Random((float)cellsize.x_), 0.0f, Random((float)cellsize.y_))) + (node_->GetRotation().Inverse() * position);
			//IntVector2 splatpos = terrain_->WorldToHeightMap(qp.pos);

			//Vector3 rotatedpos = rot.Inverse() * qp.pos;
			Vector2 normalized = CustomWorldToNormalized(splatmap, terrain_, qp.pos);
			int ix = (normalized.x_*(float)(splatmap->GetWidth()));
			int iy = (normalized.y_*(float)(splatmap->GetHeight()));
			iy = splatmap->GetHeight() - iy;


			Color red = splatmap->GetPixel(ix, iy);

			if (splatmap && red.b_ > 0.5)
			{
				qp.rot = Quaternion(0.0f, Random(360.0f), 0.0f);
				qp.pos.y_ = terrain_->GetHeight(node_->GetRotation() * qp.pos) - 0.2f;
				qp.scale = 1.0f + Random(1.8f);
				qpList_.Push(qp);
			}
		}

		if (qpList_.Size() < 1)
		{
			ATOMIC_LOGDEBUG("Vegetation list is empty (maybe the splatmap is empty?");
			return;
		}

		Model *pModel = cache->GetResource<Model>("Models/Veg/vegbrush.mdl");
		SharedPtr<Model> cloneModel = pModel->Clone();


		Node *grassnode = node_->CreateChild();
		GeomReplicator *grass = grassnode->CreateComponent<GeomReplicator>();
		grass->SetModel(cloneModel);
		grass->SetMaterial(cache->GetResource<Material>("Models/Veg/veg-alphamask.xml"));

		Vector3 lightDir(0.6f, -1.0f, 0.8f);

		lightDir = -1.0f * lightDir.Normalized();
		grass->Replicate(qpList_, lightDir);

		// specify which verts in the geom to move
		// - for the vegbrush model, the top two vertex indeces are 2 and 3
		PODVector<unsigned> topVerts;
		topVerts.Push(2);
		topVerts.Push(3);

		// specify the number of geoms to update at a time
		unsigned batchCount = 10000;

		// wind velocity (breeze velocity shown)
		Vector3 windVel(0.1f, -0.1f, 0.1f);

		// specify the cycle timer
		float cycleTimer = 1.4f;

		grass->ConfigWindVelocity(topVerts, batchCount, windVel, cycleTimer);
		grass->WindAnimationEnabled(true);

		vegReplicators_.InsertNew(sector, grass);

	}

}
