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

#pragma once

#include <Atomic/Core/Object.h>
#include <Atomic/Graphics/DecalSet.h>
#include <Atomic/Math/Vector3.h>
#include <Atomic/Graphics/TerrainPatch.h>
#include <Atomic/Graphics/CustomGeometry.h>
#include <Atomic/Resource/Image.h>
#include <Atomic/UI/UIWindow.h>
#include <Atomic/UI/UISlider.h>
#include <AtomicEditor/Editors/SceneEditor3D/SceneView3D.h>
#include <Atomic/Environment/GeomReplicator.h>
#include "ColorMap.h"

using namespace Atomic;

namespace Atomic
{
	class Node;
	class Scene;
}

namespace AtomicEditor
{
	enum TerrainEditMode
	{
		RAISE = 0,
		LOWER,
		SMOOTH,
		FLATTEN,
		SETHEIGHT,
		PAINT,
		OBJECTS
	};

    class SceneEditor3D;

	class TerrainEditor : public Object
	{
		ATOMIC_OBJECT(TerrainEditor, Object);

	public:

		TerrainEditor(Context* context, SceneEditor3D* sceneEditor);
		//Node * CreateTree(const Vector3 & pos, Node* forest);
		virtual ~TerrainEditor();

		void SetBrushSize(float size);
		float GetBrushSize();

		void SetBrushHardness(float hardness);
		float GetBrushHardness();

		void SetBrushPower(float power);
		float GetBrushPower();

		void SetPaintLayer(int layer);
		int GetPaintLayer();

		void SetTerrainEditMode(TerrainEditMode mode);
		float GetTerrainEditMode();
		bool Revert();

	protected:
		SharedPtr<ColorMap> colorMap_;

	private:
		void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData);
		void HandleGizmoEditModeChanged(StringHash eventType, VariantMap& eventData);
		void ApplyHeightBrush(Terrain * terrain, Image * height, Image * mask, float x, float z, float radius, float max, float power, float hardness, bool usemask, float dt);
		void SetHeightValue(Image * height, int x, int y, float val);
		float GetHeightValue(Image * height, int x, int y);
		float CalcSmooth(Image * height, float * kernel, int kernelsize, int terrainx, int terrainz);
		void ApplySmoothBrush(Terrain * terrain, Image * height, Image * mask, float x, float z, float radius, float max, float power, float hardness, bool usemask, float dt);
		void HandleNodeAdded(StringHash eventType, VariantMap & eventData);
		void UpdateTerrainCursor();
		void SetBrushCursorHeight(Terrain* terrain);
		void ApplyBlendBrush(Terrain *terrain, Image *height, ColorMap *blend, Image *mask, float x, float z, float radius, float mx, float power, float hardness, int layer, bool usemask, float dt);
		void DrawObject(Terrain *terrain, Image *height, Image *mask, float x, float y, float z, float radius, float mx, float power, float hardness, int layer, bool usemask, float dt);

		float framerateTimer_;
		Vector3 cursorPosition_;
		SharedPtr<TerrainPatch> selectedPatch_;
		WeakPtr<SceneEditor3D> sceneEditor3D_;
		WeakPtr<Scene> scene_;
		SharedPtr<DecalSet> terrainDecals_;
		Vector3 GetScreenGround(float mousex, float mousey);
		WeakPtr<Terrain> terrain_;
		float brushSize_;
		Vector3 spacing_;
		bool hasCopied_;
		double brushHardness_;
		double brushPower_;
		PODVector<RayQueryResult> result_;
		Vector3 hitpos_;
		SharedPtr<SceneView3D> sceneview3d_;
		TerrainEditMode mode_;
		float flattenHeight_;
		int paintLayer_;

		Vector2 WorldToNormalized(Image *height, Terrain *terrain, Vector3 world);
		SharedPtr<Node> brushcursornode_;
		SharedPtr<CustomGeometry> brushcursor_;
		SharedPtr<Material> brushmat_;
		SharedPtr<Model> facker_;
		void BuildCursorMesh(float radius, Vector3 spacing);
		void FileSaveHandler(StringHash eventType, VariantMap& eventData);

		SharedPtr<Terrain> lastTerrain_;
		SharedPtr<Material> terrainMaterial_;
		SharedPtr<Texture2D> weightTexture_;

		bool terrainUpdated_;
		bool updateObjects_;
		Vector2 CustomWorldToNormalized(Image *height, Terrain *terrain, Vector3 world);
	};



}
