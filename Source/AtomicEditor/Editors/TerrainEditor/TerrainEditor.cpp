//
// Copyright (c) 2014-2016 THUNDERBEAST GAMES LLC
//
// Includes work from MIT licensed Urho3d terrain editor by JTippetts
// https://github.com/JTippetts/U3DTerrainEditor
// Copyright (c) 2015 JTippetts
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


#include <Atomic/IO/Log.h>
#include <Atomic/Core/CoreEvents.h>
#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Graphics/Drawable.h>
#include <Atomic/Graphics/DebugRenderer.h>
#include <Atomic/Graphics/Terrain.h>
#include <Atomic/Graphics/TerrainPatch.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/Material.h>
#include <Atomic/Graphics/DecalSet.h>
#include <Atomic/Scene/SceneEvents.h>
#include <Atomic/Scene/Node.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Scene/PrefabComponent.h>

#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Environment/ProcSky.h>

#include <AtomicEditor/Editors/SceneEditor3D/SceneEditor3D.h>
#include <AtomicEditor/Editors/SceneEditor3D/SceneEditor3DEvents.h>
#include <AtomicEditor/Editors/SceneEditor3D/SceneEditHistory.h>
#include <Atomic/UI/UIWindow.h>
#include "TerrainEditor.h"
#include <cmath>
#include <algorithm>

#include <Atomic/UI/UIView.h>
#include <Atomic/UI/UIWindow.h>
#include <Atomic/UI/UISlider.h>
#include <Atomic/UI/UILayout.h>
#include <Atomic/UI/UITextField.h>
#include <Atomic/UI/UIEvents.h>

#include <Atomic/Graphics/Model.h>
#include <Atomic/Graphics/Technique.h>
#include <Atomic/Navigation/NavigationMesh.h>
#include <Atomic/Navigation/Navigable.h>
#include <Atomic/Resource/ResourceEvents.h>

#include <Atomic/Resource/XMLFile.h>
#include <Atomic/Environment/StaticModelEx.h>

namespace AtomicEditor
{

    TerrainEditor::TerrainEditor(Context* context, SceneEditor3D *sceneEditor) : Object(context), hasCopied_(false)
    {
		framerateTimer_ = 0;
		terrainUpdated_ = false;
        sceneEditor3D_ = sceneEditor;
        scene_ = sceneEditor3D_->GetScene();
		sceneview3d_ = sceneEditor3D_->GetSceneView3D();
		hitpos_ = Vector3::ZERO;
        spacing_ = Vector3::ONE;
		mode_ = TerrainEditMode::RAISE;
        ResourceCache* cache = GetSubsystem<ResourceCache>();
		//sceneview3d_->GetCamera()->SetFillMode(Atomic::FillMode::FILL_WIREFRAME);
        //brushcursornode_ = scene_->CreateChild();
		brushcursornode_ = new Node(context_);
       // brushcursornode_->SetTemporary(true);
        brushcursor_ = brushcursornode_->CreateComponent<CustomGeometry>();
		brushcursornode_->SetTemporary(true);
		brushcursor_->SetEnabled(false);
		brushcursor_->SetViewMask(0x80000000); // Editor raycasts use viewmask 0x7fffffff
		brushcursor_->SetOccludee(false);
		brushcursor_->SetMaterial(cache->GetResource<Material>("Materials/TerrainCursor.xml"));
		brushcursornode_->SetPosition(Vector3(0, 0, 0));
        brushSize_ = 5.0f;

		updateObjects_ = false;

		SubscribeToEvent(E_GIZMOEDITMODECHANGED, ATOMIC_HANDLER(TerrainEditor, HandleGizmoEditModeChanged));
		SubscribeToEvent(E_FILECHANGED, ATOMIC_HANDLER(TerrainEditor, FileSaveHandler));	
    }

    TerrainEditor::~TerrainEditor()
    {

    }




	void TerrainEditor::HandleGizmoEditModeChanged(StringHash eventType, VariantMap& eventData)
	{


		EditMode mode = (EditMode)(eventData[GizmoEditModeChanged::P_MODE].GetInt());
		if (mode == EditMode::TERRAIN) {
			Octree* octree = scene_->GetComponent<Octree>();
			if (!octree)
				ATOMIC_LOGERROR("Terrain brush error: Octree Missing");
			brushcursor_->SetEnabled(true);
			octree->AddManualDrawable(brushcursor_);
			SubscribeToEvent(E_POSTRENDERUPDATE, ATOMIC_HANDLER(TerrainEditor, HandlePostRenderUpdate));
		}
		else {
			brushcursor_->SetEnabled(false);
			UnsubscribeFromEvent(E_POSTRENDERUPDATE);
		}
	}

    void TerrainEditor::HandleNodeAdded(StringHash eventType, VariantMap& eventData)
    {
        Node* node = (Node*)(eventData[NodeAdded::P_NODE].GetPtr());

        //Do something with the added node here
    }

	void TerrainEditor::UpdateTerrainCursor()
	{
		if (!sceneEditor3D_->GetSceneView3D()->MouseInView()) {
			brushcursornode_->SetEnabled(false);
			return;
		}

		Ray camRay = sceneview3d_->GetCameraRay();

		RayOctreeQuery query(result_, camRay, RAY_TRIANGLE, sceneEditor3D_->GetSceneView3D()->GetCamera()->GetFarClip(), DRAWABLE_GEOMETRY);
		sceneEditor3D_->GetSceneView3D()->GetOctree()->Raycast(query);


		if (query.result_.Size())
		{

			for (int n = 0; n < result_.Size(); n++)
			{
               const RayQueryResult& r = result_[n];
			   if (r.drawable_->GetType() == Atomic::TerrainPatch::GetTypeStatic())
			   {

				   TerrainPatch* patch = (TerrainPatch*)r.drawable_;
				   terrain_ = patch->GetOwner();
				   if (terrain_ != lastTerrain_) {
					 terrainMaterial_ = terrain_->GetMaterial();
				     weightTexture_ = (Texture2D*)terrainMaterial_->GetTexture(TU_DIFFUSE);
					 colorMap_ = new ColorMap(context_);
				     colorMap_->SetSourceColorMap(weightTexture_);
					   lastTerrain_ = terrain_;
				   }

				   cursorPosition_ = r.position_;
				   float height = terrain_->GetHeight(Vector3(cursorPosition_.x_, cursorPosition_.y_, cursorPosition_.z_));
				   brushcursornode_->SetPosition(Vector3(cursorPosition_.x_, height + 1, cursorPosition_.z_));
				   brushcursornode_->SetEnabled(true);
				   Octree* octree = scene_->GetComponent<Octree>();
				   if (!octree)
					   ATOMIC_LOGERROR("Terrain brush error: Octree Missing");
				   octree->AddManualDrawable(brushcursor_);
				   break;
			   }
			}

			if(terrain_)
		    	SetBrushCursorHeight(terrain_);
		}
	}


    




    void  TerrainEditor::SetBrushCursorHeight(Terrain* terrain)
    {
        int numverts = brushcursor_->GetNumVertices(0);
        for (int v = 0; v < numverts - 1; v++)
        {
            CustomGeometryVertex* vert = brushcursor_->GetVertex(0, v);
            float ht = terrain->GetHeight(Vector3(vert->position_.x_ + brushcursornode_->GetPosition().x_, 0, vert->position_.z_ + brushcursornode_->GetPosition().z_));
            vert->position_.y_ = ht - cursorPosition_.y_ + 0.00001;
        }
        brushcursor_->Commit();
    }

    void TerrainEditor::BuildCursorMesh(float radius, Vector3 spacing) {


        brushcursor_->SetNumGeometries(1);
        brushcursor_->BeginGeometry(0, TRIANGLE_LIST);
        brushcursor_->SetDynamic(true);

        float spacingx = spacing.x_;
        float spacingz = spacing.z_;
        float meshsize = (floor(radius) * 2) + 2;
        float originx = (-meshsize)*spacingx*0.5;
        float originz = (-meshsize)*spacingx*0.5;

        float uvspacing = 1 / (meshsize - 2);

        int x, z;

        for (x = 0; x < meshsize - 2; x++) {
            for (z = 0; z < meshsize - 2; z++)
            {
                brushcursor_->DefineVertex(Vector3(originx + (x + 1)*spacingx, 0, originz + (z + 1)*spacingz));
                brushcursor_->DefineTexCoord(Vector2((x + 1)*uvspacing, (z + 1)*uvspacing));

                brushcursor_->DefineVertex(Vector3(originx + (x + 1)*spacingx, 0, originz + z*spacingz));
                brushcursor_->DefineTexCoord(Vector2((x + 1)*uvspacing, z*uvspacing));

                brushcursor_->DefineVertex(Vector3(originx + x*spacingx, 0, originz + z*spacingz));
                brushcursor_->DefineTexCoord(Vector2(x*uvspacing, z*uvspacing));

                brushcursor_->DefineVertex(Vector3(originx + x*spacingx, 0, originz + (z + 1)*spacingz));
                brushcursor_->DefineTexCoord(Vector2(x*uvspacing, (z + 1)*uvspacing));

                brushcursor_->DefineVertex(Vector3(originx + (x + 1)*spacingx, 0, originz + (z + 1)*spacingz));
                brushcursor_->DefineTexCoord(Vector2((x + 1)*uvspacing, (z + 1)*uvspacing));

                brushcursor_->DefineVertex(Vector3(originx + x*spacingx, 0, originz + z*spacingz));
                brushcursor_->DefineTexCoord(Vector2(x*uvspacing, z*uvspacing));
            }
        }

        brushcursor_->Commit();
    }

    void TerrainEditor::SetBrushSize(float size) {
        brushSize_ = size;
		BuildCursorMesh(brushSize_, spacing_);
    }

    float TerrainEditor::GetBrushSize() {
        return brushSize_;
    }

	void TerrainEditor::SetBrushHardness(float hardness) {
		brushHardness_ = hardness;
	}

	float TerrainEditor::GetBrushHardness() {
		return brushHardness_;
	}

	void TerrainEditor::SetBrushPower(float power) {
		brushPower_ = power;
	}

	float TerrainEditor::GetBrushPower() {
		return brushPower_;
	}

	void TerrainEditor::SetTerrainEditMode(TerrainEditMode mode) {
		mode_ = mode;
	}

	float TerrainEditor::GetTerrainEditMode() {
		return mode_;
	}

	void TerrainEditor::SetPaintLayer(int layer) {
		paintLayer_ = layer;
	}

	int TerrainEditor::GetPaintLayer() {
		return paintLayer_;
	}


    void TerrainEditor::HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
    {
		//ATOMIC_LOGDEBUG("Updating cursor");

		////Only update terrain 30 times per second, there's no point going faster and this should make it smoother
		framerateTimer_ += eventData[Update::P_TIMESTEP].GetFloat();
		if (framerateTimer_ < 1 / 2)
			return;
		framerateTimer_ = 0;

		float dt = eventData[Update::P_TIMESTEP].GetFloat();


        Input* input = GetSubsystem<Input>();

		if (input->GetMouseButtonDown(MOUSEB_LEFT) && sceneEditor3D_->GetSceneView3D()->MouseInView())
		{
			if (!terrain_) {
				ATOMIC_LOGDEBUG("No terrain");
				return;
			}

			IntVector2 v = terrain_->WorldToHeightMap(cursorPosition_);
			Image* heightmap = terrain_->GetHeightMap();



			int invert = 1;
			if (input->GetKeyDown(KEY_LSHIFT))
				invert = -1;

			float max = 256 * terrain_->GetSpacing().y_;
			float power = brushPower_ / (max * 4);
			float smoothpower = brushPower_ * 2;
			float radius = brushSize_ / terrain_->GetSpacing().x_;
			float layer = 2;
			float paintpower = brushPower_ * 10;

			if (!flattenHeight_)
				flattenHeight_ = cursorPosition_.y_ / max;

			if (mode_ == TerrainEditMode::RAISE) {
				ApplyHeightBrush(terrain_, heightmap, nullptr, cursorPosition_.x_, cursorPosition_.z_, radius, max, invert * power, brushHardness_, false, dt);
			}
			else if (mode_ == TerrainEditMode::SMOOTH || input->GetKeyDown(KEY_LCTRL)) {
				ApplySmoothBrush(terrain_, heightmap, nullptr, cursorPosition_.x_, cursorPosition_.z_, radius, max, smoothpower, brushHardness_, false, dt);
			}
			else if (mode_ == TerrainEditMode::LOWER) {
				ApplyHeightBrush(terrain_, heightmap, nullptr, cursorPosition_.x_, cursorPosition_.z_, radius, max, invert * -power, brushHardness_, false, dt);
			}
			else if (mode_ == TerrainEditMode::FLATTEN) {
				ApplyHeightBrush(terrain_, heightmap, nullptr, cursorPosition_.x_, cursorPosition_.z_, radius, flattenHeight_, smoothpower, brushHardness_, false, dt);
			}
			else if (mode_ == TerrainEditMode::PAINT) {
				ApplyBlendBrush(terrain_, heightmap, colorMap_, nullptr, cursorPosition_.x_, cursorPosition_.z_, radius, max, paintpower, brushHardness_, paintLayer_, false, dt);
			}
			//else if (mode_ == TerrainEditMode::PAINT) {
			//	DrawObject(terrain_, heightmap, nullptr, cursorPosition_.x_, cursorPosition_.y_, cursorPosition_.z_, radius, max, paintpower, brushHardness_, paintLayer_, false, dt);
			//}

			if (mode_ != TerrainEditMode::PAINT && mode_ != TerrainEditMode::OBJECTS) {
				terrain_->ApplyHeightMap();
			}
			else {
				colorMap_->ApplyColorMap();
			}
			updateObjects_ = true;
			terrainUpdated_ = true;
			sceneEditor3D_->GetScene()->SendEvent(E_SCENEEDITSCENEMODIFIED);
        }
		else {
			flattenHeight_ = NULL;
		}

		//if (updateObjects_)
		//{
		//	updateObjects_ = false;
		//	if (terrain_->GetNode()->HasComponent<GeomReplicator>())
		//	{
		//		PODVector<GeomReplicator*> replicators;
		//		terrain_->GetNode()->GetComponents<GeomReplicator>(replicators);
		//		for (unsigned i = 0; i < replicators.Size(); ++i)
		//		{
		//			replicators[i]->Update();
		//		}

		//	}
		//}


		// Code to change brush size using mousewheel. Don't really need it now that it's changed in the UI, but would be nice to have it. 
		// Will need an event to keep the UI in sync though, so commenting it out for now
        //else if (input->GetMouseMoveWheel() && input->GetKeyDown(KEY_LCTRL) && sceneEditor3D_->GetSceneView3D()->MouseInView())
        //{
        //    int mouseWheel = input->GetMouseMoveWheel();
        //    int sensitivity = 1;

        //    // Apple decided to change the direction of mousewheel input to match touch devices
        //    #ifdef ATOMIC_PLATFORM_OSX
        //    mouseWheel = -mouseWheel;
        //    #endif

        //    if (mouseWheel) {
        //        SetBrushSize(GetBrushSize() + (mouseWheel * sensitivity));
        //     //   BuildCursorMesh(brushSize_, spacing_);
        //        SetBrushCursorHeight(terrain_);
        //    }
        //}

        UpdateTerrainCursor();
    }







    void TerrainEditor::ApplyHeightBrush(Terrain *terrain, Image *height, Image *mask, float x, float z, float radius, float max, float power, float hardness, bool usemask, float dt)
    {
        if (!terrain || !height) return;

        Vector3 world = Vector3(x, 0, z);
        IntVector2 ht = terrain->WorldToHeightMap(world);

        int sz = radius + 1;
        int comp = height->GetComponents();
        for (int hx = ht.x_ - sz; hx <= ht.x_ + sz; ++hx)
        {
            for (int hz = ht.y_ - sz; hz <= ht.y_ + sz; ++hz)
            {
                if (hx >= 0 && hx<height->GetWidth() && hz >= 0 && hz<height->GetHeight())
                {
                    float dx = (float)(hx - ht.x_);
                    float dz = (float)(hz - ht.y_);
                    float d = std::sqrt(dx*dx + dz*dz);
					float i = ((d - radius) / (hardness*radius - radius));
                    i = std::max(0.0f, std::min(1.0f, i));
                    i = i*dt*power;
                    if (usemask)
                    {
                        float m = mask->GetPixelBilinear((float)(hx) / (float)(height->GetWidth()), (float)(hz) / (float)(height->GetHeight())).r_;
                        i = i*m;
                    }
                    float hval = GetHeightValue(height, hx, hz);
                    float newhval = hval + (max - hval)*i;
                    SetHeightValue(height, hx, hz, newhval);

                }
            }
        }
    }


    void TerrainEditor::SetHeightValue(Image *height, int x, int y, float val)
    {
        if (!height) return;
        if (height->GetComponents() == 1) height->SetPixel(x, y, Color(val, 0, 0));
        else
        {
            float expht = std::floor(val*255.0f);
            float rm = val*255.0f - expht;
            height->SetPixel(x, y, Color(expht / 255.0f, rm, 0));
        }
    }

    float TerrainEditor::GetHeightValue(Image *height, int x, int y)
    {
        if (!height) return 0.0f;
        if (height->GetComponents() == 1) return height->GetPixel(x, y).r_;
        else
        {
            Color c = height->GetPixel(x, y);
            return c.r_ + c.g_ / 255.0f;
        }
    }

    float TerrainEditor::CalcSmooth(Image *height, float *kernel, int kernelsize, int terrainx, int terrainz)
    {
        float sum = 0.0f;
        float weight = 0.0f;
        int ox = terrainx - int(kernelsize / 2);
        int oz = terrainz - int(kernelsize / 2);

        for (int x = 0; x<kernelsize; ++x)
        {
            for (int z = 0; z<kernelsize; ++z)
            {
                int nx = x + ox;
                int nz = z + oz;
                if (x >= 0 && x<height->GetWidth() && z >= 0 && z<height->GetHeight())
                {
                    float hval = GetHeightValue(height, nx, nz);
                    sum += hval*kernel[z*kernelsize + x];
                    weight += kernel[z*kernelsize + x];
                }
            }
        }
        if (weight>0) return sum / weight;
        else return 0.0f;
    }

    void TerrainEditor::ApplySmoothBrush(Terrain *terrain, Image *height, Image *mask, float x, float z, float radius, float max, float power, float hardness, bool usemask, float dt)
    {
        static float kernel[81] =
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0.0058874471228999, 0.012503642863169, 0.014925760324933, 0.012503642863169, 0.0058874471228999, 0, 0,
            0, 0.0058874471228999, 0.017486615939231, 0.026328026597312, 0.029851520649865, 0.026328026597312, 0.017486615939231, 0.0058874471228999, 0,
            0, 0.012503642863169, 0.026328026597312, 0.038594828619481, 0.044777280974798, 0.038594828619481, 0.026328026597312, 0.012503642863169, 0,
            0, 0.014925760324933, 0.029851520649865, 0.044777280974798, 0.059703041299731, 0.044777280974798, 0.029851520649865, 0.014925760324933, 0,
            0, 0.012503642863169, 0.026328026597312, 0.038594828619481, 0.044777280974798, 0.038594828619481, 0.026328026597312, 0.012503642863169, 0,
            0, 0.0058874471228999, 0.017486615939231, 0.026328026597312, 0.029851520649865, 0.026328026597312, 0.017486615939231, 0.0058874471228999, 0,
            0, 0, 0.0058874471228999, 0.012503642863169, 0.014925760324933, 0.012503642863169, 0.0058874471228999, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0,
        };

        if (!terrain || !height || !terrain) return;

        Vector3 world = Vector3(x, 0, z);
        IntVector2 ht = terrain->WorldToHeightMap(world);

        int sz = radius + 1;
        int comp = height->GetComponents();
        for (int hx = ht.x_ - sz; hx <= ht.x_ + sz; ++hx)
        {
            for (int hz = ht.y_ - sz; hz <= ht.y_ + sz; ++hz)
            {
                if (hx >= 0 && hx<height->GetWidth() && hz >= 0 && hz<height->GetHeight())
                {
                    float dx = (float)(hx - ht.x_);
                    float dz = (float)(hz - ht.y_);
                    float d = std::sqrt(dx*dx + dz*dz);
                    float i = ((d - radius) / (hardness*radius - radius));
                    i = std::max(0.0f, std::min(1.0f, i));
                    i = i*dt*power;
                    if (usemask)
                    {
                        float m = mask->GetPixelBilinear((float)(hx) / (float)(height->GetWidth()), (float)(hz) / (float)(height->GetHeight())).r_;
                        i = i*m;
                    }
                    float hval = GetHeightValue(height, hx, hz);
                    float smooth = CalcSmooth(height, kernel, 9, hx, hz);
                    float newhval = hval + (smooth - hval)*i;
                    SetHeightValue(height, hx, hz, newhval);
                }
            }
        }
    }

	bool TerrainEditor::Revert() {
		Image* image = terrain_->GetHeightMap();
		ResourceCache* cache = GetSubsystem<ResourceCache>();
		cache->ReloadResource(image);
		//TODO: We should probably return false if it doesn't work and unset the scene modified flag if it does.
		return true;
	}


	void TerrainEditor::FileSaveHandler(StringHash eventType, VariantMap& eventData)
	{
		if (!terrainUpdated_)
			return;

		using namespace FileChanged;
		const String& fileName = eventData[P_FILENAME].GetString();
        

		if (fileName == scene_->GetFileName() && terrain_)
		{
			Image* heightmap = terrain_->GetHeightMap();
			ResourceCache* cache = GetSubsystem<ResourceCache>();
			String heightmapName = heightmap->GetName();
			String heightmapFile = cache->GetResourceFileName(heightmapName);
			heightmap->SavePNG(heightmapFile);
			ATOMIC_LOGDEBUG("Saved terrain as " + heightmapFile);

			String blendmapName = weightTexture_->GetName();
			String blendmapFile = cache->GetResourceFileName(blendmapName);
			colorMap_->SavePNG(blendmapFile);
			terrainUpdated_ = false;

		}
	}

	Vector2 TerrainEditor::WorldToNormalized(Image *height, Terrain *terrain, Vector3 world)
	{
		if (!terrain || !height) return Vector2(0, 0);
		Vector3 spacing = terrain->GetSpacing();
		int patchSize = terrain->GetPatchSize();
		IntVector2 numPatches = IntVector2((height->GetWidth() - 1) / patchSize, (height->GetHeight() - 1) / patchSize);
		Vector2 patchWorldSize = Vector2(spacing.x_*(float)(patchSize*numPatches.x_), spacing.z_*(float)(patchSize*numPatches.y_));
		Vector2 patchWorldOrigin = Vector2(-0.5f * patchWorldSize.x_, -0.5f * patchWorldSize.y_);
		return Vector2((world.x_ - patchWorldOrigin.x_) / patchWorldSize.x_, (world.z_ - patchWorldOrigin.y_) / patchWorldSize.y_);
	}


	Vector2 TerrainEditor::CustomWorldToNormalized(Image *height, Terrain *terrain, Vector3 world)
	{
		if (!terrain || !height) return Vector2(0, 0);
		Vector3 spacing = terrain->GetSpacing();
		int patchSize = terrain->GetPatchSize();
		IntVector2 numPatches = IntVector2((height->GetWidth() - 1) / patchSize, (height->GetHeight() - 1) / patchSize);
		Vector2 patchWorldSize = Vector2(spacing.x_*(float)(patchSize*numPatches.x_), spacing.z_*(float)(patchSize*numPatches.y_));
		Vector2 patchWorldOrigin = Vector2(-0.5f * patchWorldSize.x_, -0.5f * patchWorldSize.y_);
		return Vector2((world.x_ - patchWorldOrigin.x_) / patchWorldSize.x_, (world.z_ - patchWorldOrigin.y_) / patchWorldSize.y_);
	}

	void TerrainEditor::ApplyBlendBrush(Terrain *terrain, Image *height, ColorMap *blend, Image *mask, float x, float z, float radius, float mx, float power, float hardness, int layer, bool usemask, float dt)
	{
		if (!blend || !height || !terrain) return;

		Quaternion rot = terrain->GetNode()->GetRotation();
		Vector3 pos = Vector3(x, 0, z);
		Vector3 rotatedpos = rot.Inverse() * pos;
		Vector2 normalized = CustomWorldToNormalized(height, terrain, rotatedpos);
		float ratio = ((float)blend->GetWidth() / (float)height->GetWidth());
		int ix = (normalized.x_*(float)(blend->GetWidth() - 1));
		int iy = (normalized.y_*(float)(blend->GetHeight() - 1));
		iy = blend->GetHeight() - iy;

		float rad = radius*ratio;
		int sz = rad + 1;

		for (int hx = ix - sz; hx <= ix + sz; ++hx)
		{
			for (int hz = iy - sz; hz <= iy + sz; ++hz)
			{
				if (hx >= 0 && hx<blend->GetWidth() && hz >= 0 && hz<blend->GetHeight())
				{
					float dx = (float)hx - (float)ix;
					float dz = (float)hz - (float)iy;
					float d = std::sqrt(dx*dx + dz*dz);
					float i = ((d - rad) / (hardness*rad - rad));
					i = std::max(0.0f, std::min(1.0f, i));
					i = i*dt*power;
					if (usemask)
					{
						float m = mask->GetPixelBilinear((float)(hx) / (float)(blend->GetWidth()), (float)(hz) / (float)(blend->GetHeight())).r_;
						i = i*m;
					}
					Color col = blend->GetPixel(hx, hz);
					if (layer == 1)
					{
						col.r_ = col.r_ + i*(1.0f - col.r_);
						col.r_ = std::min(1.0f, col.r_);
						float others = col.g_ + col.b_ + col.a_;
						col.g_ = (col.g_ / others)*(1.0f - col.r_);
						col.b_ = (col.b_ / others)*(1.0f - col.r_);
						col.a_ = (col.a_ / others)*(1.0f - col.r_);
					}
					else if (layer == 2)
					{
						col.g_ = col.g_ + i*(1.0f - col.g_);
						col.g_ = std::min(1.0f, col.g_);
						float others = col.r_ + col.b_ + col.a_;
						col.r_ = (col.r_ / others)*(1.0f - col.g_);
						col.b_ = (col.b_ / others)*(1.0f - col.g_);
						col.a_ = (col.a_ / others)*(1.0f - col.g_);
					}
					else if (layer == 3)
					{
						col.b_ = col.b_ + i*(1.0f - col.b_);
						col.b_ = std::min(1.0f, col.b_);
						float others = col.r_ + col.g_ + col.a_;
						col.r_ = (col.r_ / others)*(1.0f - col.b_);
						col.g_ = (col.g_ / others)*(1.0f - col.b_);
						col.a_ = (col.a_ / others)*(1.0f - col.b_);
					}
					else if (layer == 4)
					{
						col.a_ = col.a_ + i*(1.0f - col.a_);
						col.a_ = std::min(1.0f, col.a_);
						float others = col.r_ + col.g_ + col.b_;
						col.r_ = (col.r_ / others)*(1.0f - col.a_);
						col.g_ = (col.g_ / others)*(1.0f - col.a_);
						col.b_ = (col.b_ / others)*(1.0f - col.a_);
					}
					blend->SetPixel(hx, hz, col);
					//LOGINFO(String(col.r_)+String(",")+String(col.g_));
				}
			}
		}
	}


	void TerrainEditor::DrawObject(Terrain *terrain, Image *height, Image *mask, float x, float y, float z, float radius, float mx, float power, float hardness, int layer, bool usemask, float dt)
	{
		if (!height || !terrain) return;
		
		Quaternion rot = terrain->GetNode()->GetRotation();
		Vector3 pos = Vector3(x, 0, z);
		Vector3 rotatedpos = rot.Inverse() * pos;
		Vector2 normalized = CustomWorldToNormalized(height, terrain, rotatedpos);
		float ratio = ((float)height->GetWidth() / (float)height->GetWidth());
		int ix = (normalized.x_*(float)(height->GetWidth() - 1));
		int iy = (normalized.y_*(float)(height->GetHeight() - 1));
		iy = height->GetHeight() - iy;

		float rad = radius*ratio;
		int sz = rad + 1;

		ResourceCache* cache = GetSubsystem<ResourceCache>();

		Node *forest = scene_->GetChild("Forest", true);
		if (!forest)
			return;

		String treename = "x" + (String)x + "y" + (String)y + "z" + (String)z;

		Node* oldtree = forest->GetChild(treename, true);

		if (oldtree)
		   return;

		SharedPtr<Node> treenode (new Node(context_));
		treenode->SetName(treename);
		forest->AddChild(treenode);
		treenode->SetPosition(Vector3(x, y + 2, z));

	 //   SharedPtr<StaticModelEx> tree(new StaticModelEx(context_));
		//SharedPtr<Model> treemodel(cache->GetResource<Model>("Models/Crate.mdl"));
		//treenode->AddComponent(tree, 0, CreateMode::LOCAL);
		//SharedPtr<Material> mat(cache->GetResource<Material>("Bostich/Materials/Material.material"));
		//tree->SetModel(treemodel);
		//tree->SetMaterial(mat);

		
		PrefabComponent* prefabComponent = treenode->CreateComponent<PrefabComponent>();
		prefabComponent->SetPrefabGUID("TreeSystem/broadleaf.prefab");
		treenode->SetName("PineTree");



		//SharedPtr<StaticModelEx>tree(new StaticModelEx(context_));
		//treenode->AddComponent(tree, 0, CreateMode::LOCAL);
		//SharedPtr<Model> model(tree->MakeModel());
		//tree->SetModel(model);
		//SharedPtr<Material> mat(cache->GetResource<Material>("Bostich/Materials/Material.material"));
		//tree->SetMaterial(mat);
		//tree->SetDrawDistance(300);

	//	forest->AddChild(treenode);
	//	StaticModelEx *tree = new StaticModelEx(context_);
	//	SharedPtr<Model> treemodel1(cache->GetResource<Model>("Models/Crate.mdl"));
	//	//SharedPtr<Model> treemodel2(cache->GetResource<Model>("Models/Bucket1_Bucket1.mdl"));
	//	treenode->AddComponent(tree, 0, CreateMode::LOCAL);
	//	

	//	SharedPtr<Geometry> lod1(treemodel1->GetGeometry(0, 1));
	////	SharedPtr<Geometry> lod2(treemodel2->GetGeometry(0, 1));
	//	lod1->SetLodDistance(0);
	////	lod2->SetLodDistance(10);

	//	facker_ = treemodel1->Clone();
	//	facker_->SetName("Crate.mdl");
	//	facker_->SetNumGeometries(1);
	//	facker_->SetNumGeometryLodLevels(0, 2);
	//	facker_->SetGeometry(0, 0, lod1);
	////	lodtree->SetGeometry(0, 1, lod2);
	//	
	//	SharedPtr<Geometry> geo = tree->AddImposter();
	//	facker_->SetGeometry(0, 1, geo);
	//	tree->SetModel(facker_);
	//	///tree->AddImposter();

	//	SharedPtr<Material> mat (cache->GetResource<Material>("Materials/Crate.material"));
	//	tree->SetMaterial(mat);
	//	tree->SetDrawDistance(300);

	//	

	//	treenode->SetPosition(Vector3(x, y+2, z));



		

	}
}
