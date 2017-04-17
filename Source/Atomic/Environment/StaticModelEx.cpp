#include <Atomic/Environment/StaticModelEx.h>
#include <Atomic/Environment/Wind.h>
#include <Atomic/Core/Attribute.h>

#include <Atomic/Core/Context.h>
#include <Atomic/Graphics/Camera.h>
#include <Atomic/Graphics/Geometry.h>
#include <Atomic/Graphics/Material.h>
#include <Atomic/Graphics/Model.h>
#include <Atomic/Scene/Scene.h>
#include <Atomic/Graphics/Renderer.h>

#include <Atomic/Graphics/Zone.h>
#include <Atomic/Graphics/Texture2D.h>
#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Graphics/RenderPath.h>

#include <Atomic/Graphics/Material.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/IO/Log.h>
#include <ToolCore/Project/Project.h>
#include <ToolCore/ToolSystem.h>

#include <Atomic/Graphics/VertexBuffer.h>
#include <Atomic/Graphics/IndexBuffer.h>
#include <Atomic/Graphics/Technique.h>
#include <Atomic/Graphics/GraphicsDefs.h>

namespace Atomic
{
    extern const char* GEOMETRY_CATEGORY;
    StaticModelEx::StaticModelEx(Context* context)
        : StaticModel(context)
    {

    }

    StaticModelEx::~StaticModelEx()
    {
    }

    void StaticModelEx::RegisterObject(Context* context)
    {
        context->RegisterFactory<StaticModelEx>(GEOMETRY_CATEGORY);

        ATOMIC_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
        ATOMIC_MIXED_ACCESSOR_ATTRIBUTE("Model", GetModelAttr, SetModelAttr, ResourceRef, ResourceRef(Model::GetTypeStatic()), AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Material", GetMaterialsAttr, SetMaterialsAttr, ResourceRefList, ResourceRefList(Material::GetTypeStatic()),
            AM_DEFAULT);

        ATOMIC_ACCESSOR_ATTRIBUTE("Apply Wind", ShouldApplyWind, SetApplyWind, bool, false, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Clone Materials", AreMaterialsCloned, SetCloneMaterials, bool, false, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Unique Materials", AreMaterialsUnique, SetUniqueMaterials, bool, false, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("LOD Switch Bias", GetLodSwitchBias, SetLodSwitchBias, float, 1.0f, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("LOD Switch Duration", GetLodSwitchDuration, SetLodSwitchDuration, float, 1.0f, AM_DEFAULT);

        ATOMIC_ATTRIBUTE("Is Occluder", bool, occluder_, false, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
        ATOMIC_ATTRIBUTE("Cast Shadows", bool, castShadows_, false, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("Shadow Distance", GetShadowDistance, SetShadowDistance, float, 0.0f, AM_DEFAULT);
        ATOMIC_ACCESSOR_ATTRIBUTE("LOD Bias", GetLodBias, SetLodBias, float, 1.0f, AM_DEFAULT);
        ATOMIC_COPY_BASE_ATTRIBUTES(Drawable);
        ATOMIC_ATTRIBUTE("Occlusion LOD Level", int, occlusionLodLevel_, M_MAX_UNSIGNED, AM_DEFAULT);

    }

    void StaticModelEx::ApplyAttributes()
    {
    }

    void StaticModelEx::UpdateBatches(const FrameInfo& frame)
    {
        UpdateLodLevels(frame);
        UpdateWind();
    }

    void StaticModelEx::SetModel(Model* model)
    {
        StaticModel::SetModel(model);

        // Setup extra batches
        const unsigned numGeometries = GetNumGeometries();
        geometryDataEx_.Resize(numGeometries);
        batches_.Resize(numGeometries * 2);
        for (unsigned i = 0; i < numGeometries; ++i)
        {
            batches_[i + numGeometries] = batches_[i];

            StaticModelGeometryDataEx& geometryDataEx = geometryDataEx_[i];
            batches_[i].instancingData_ = &geometryDataEx.primaryInstanceData_;
            batches_[i + numGeometries].instancingData_ = &geometryDataEx.secondaryInstanceData_;
            geometryDataEx.primaryInstanceData_.x_ = 1.0;
            geometryDataEx.secondaryInstanceData_.x_ = 0.0;
        }

        SetupLodDistances();
        ResetLodLevels();
    }

    void StaticModelEx::SetMaterial(Material* material)
    {
        StaticModel::SetMaterial(material);
        UpdateReferencedMaterial(material);
        for (unsigned i = 0; i < GetNumGeometries(); ++i)
        {
            SetMaterialImpl(i, material);
            SetBatchMaterial(i);
        }
		//CreateBillboard();
    }

    bool StaticModelEx::SetMaterial(unsigned index, Material* material)
    {
        if (index < GetNumGeometries())
        {
            StaticModel::SetMaterial(index, material);
            SetMaterialImpl(index, material);
            UpdateReferencedMaterial(material);
            SetBatchMaterial(index);
            return true;
        }
        return false;
    }

    Material* StaticModelEx::GetMaterial(unsigned index /*= 0*/) const
    {
        return index < geometryDataEx_.Size() ? geometryDataEx_[index].originalMaterial_ : nullptr;
    }

    void StaticModelEx::SetApplyWind(bool applyWind)
    {
        applyWind_ = applyWind;
        UpdateReferencedMaterials();
    }

    void StaticModelEx::SetCloneMaterials(bool cloneMaterials)
    {
        cloneMaterials_ = cloneMaterials;
        if (cloneMaterials_)
        {
            for (unsigned i = 0; i < GetNumGeometries(); ++i)
                SetMaterialImpl(i, geometryDataEx_[i].originalMaterial_);
        }
        else
        {
            cloneRequests_ = 0;
        }
    }

    const ResourceRefList& StaticModelEx::GetMaterialsAttr() const
    {
        materialsAttr_.names_.Resize(geometryDataEx_.Size());
        for (unsigned i = 0; i < geometryDataEx_.Size(); ++i)
            materialsAttr_.names_[i] = GetResourceName(geometryDataEx_[i].originalMaterial_);

        return materialsAttr_;
    }

    void StaticModelEx::OnSceneSet(Scene* scene)
    {
        StaticModel::OnSceneSet(scene);
        if (scene)
        {
            windSystem_ = scene->GetOrCreateComponent<WindSystem>();
            UpdateReferencedMaterials();
        }
    }

    void StaticModelEx::UpdateReferencedMaterials()
    {
        if (windSystem_ && applyWind_)
        {
            for (unsigned i = 0; i < geometryDataEx_.Size(); ++i)
            {
                windSystem_->ReferenceMaterial(geometryDataEx_[i].originalMaterial_);
            }
        }
    }

    void StaticModelEx::UpdateReferencedMaterial(Material* material)
    {
        if (windSystem_ && applyWind_)
            windSystem_->ReferenceMaterial(material);
    }

    void StaticModelEx::SetMaterialImpl(unsigned index, Material* material)
    {
        assert(index < GetNumGeometries());
        geometryDataEx_[index].originalMaterial_ = material;
        geometryDataEx_[index].clonedMaterial_.Reset();
        if (cloneMaterials_)
            geometryDataEx_[index].clonedMaterial_ = material ? material->Clone() : nullptr;
    }

    void StaticModelEx::SetBatchMaterial(unsigned index)
    {
        assert(index < geometryDataEx_.Size());
        batches_[index].material_ = cloneRequests_ ? geometryDataEx_[index].clonedMaterial_ : geometryDataEx_[index].originalMaterial_;
        batches_[index + geometryDataEx_.Size()].material_ = batches_[index].material_;
    }

    void StaticModelEx::SetCloneRequestSet(unsigned flagSet)
    {
        if (cloneMaterials_)
        {
            if (!!cloneRequests_ != !!flagSet)
            {
                cloneRequests_ = flagSet;
                for (unsigned i = 0; i < GetNumGeometries(); ++i)
                    SetBatchMaterial(i);
            }
        }
    }

    void StaticModelEx::SetCloneRequest(unsigned flag, bool enable)
    {
        if (enable)
            SetCloneRequestSet(cloneRequests_ | flag);
        else
            SetCloneRequestSet(cloneRequests_ & ~flag);
    }

    //////////////////////////////////////////////////////////////////////////
    void StaticModelEx::SetupLodDistances()
    {
        const unsigned numGeometries = GetNumGeometries();
        for (unsigned i = 0; i < numGeometries; ++i)
        {
            const Vector<SharedPtr<Geometry> >& batchGeometries = geometries_[i];
            StaticModelGeometryDataEx& geometryDataEx = geometryDataEx_[i];

            geometryDataEx.lodDistances_.Resize(batchGeometries.Size());
            for (unsigned j = 0; j < batchGeometries.Size(); ++j)
            {
                const float distance = batchGeometries[j]->GetLodDistance();
                const float fadeIn = Min(distance, distance * lodSwitchBias_);
                const float fadeOut = Max(distance, distance * lodSwitchBias_);
                geometryDataEx.lodDistances_[j] = Vector2(fadeIn, fadeOut);
            }
        }
    }

    void StaticModelEx::ResetLodLevels()
    {
        numLodSwitchAnimations_ = 0;
        for (unsigned i = 0; i < geometryDataEx_.Size(); ++i)
        {
            StaticModelGeometryDataEx& geometryDataEx = geometryDataEx_[i];
            geometryDataEx.primaryLodLevel_ = 0;
            geometryDataEx.secondaryLodLevel_ = 0;
            geometryDataEx.lodLevelMix_ = 0.0f;
        }
    }

    void StaticModelEx::CalculateLodLevels(float timeStep)
    {
        const unsigned numBatches = batches_.Size() / 2;
        for (unsigned i = 0; i < numBatches; ++i)
        {
            const Vector<SharedPtr<Geometry> >& batchGeometries = geometries_[i];
            // If only one LOD geometry, no reason to go through the LOD calculation
            if (batchGeometries.Size() <= 1)
                continue;

            StaticModelGeometryDataEx& geometryDataEx = geometryDataEx_[i];

            // #TODO Don't animate switch if first time or camera warp
            if (geometryDataEx.lodLevelMix_ > 0.0f)
            {
                // Update animation
                geometryDataEx.lodLevelMix_ -= timeStep / lodSwitchDuration_;

                if (geometryDataEx.lodLevelMix_ <= 0.0)
                {
                    // Hide second instance
                    --numLodSwitchAnimations_;
                    geometryDataEx.lodLevelMix_ = 0.0f;
                    batches_[i + numBatches].geometry_ = nullptr;
                }
            }
            else
            {
                // Re-compute LOD
                const unsigned newLod = ComputeBestLod(lodDistance_, geometryDataEx.primaryLodLevel_, geometryDataEx.lodDistances_);
                if (newLod != geometryDataEx.primaryLodLevel_)
                {
                    // Start switch
                    ++numLodSwitchAnimations_;
                    geometryDataEx.secondaryLodLevel_ = geometryDataEx.primaryLodLevel_;
                    geometryDataEx.primaryLodLevel_ = newLod;
                    geometryDataEx.lodLevelMix_ = 1.0;

                    batches_[i].geometry_ = batchGeometries[geometryDataEx.primaryLodLevel_];
                    batches_[i + numBatches].geometry_ = batchGeometries[geometryDataEx.secondaryLodLevel_];
                }
            }

            // Update factor
            geometryDataEx.primaryInstanceData_.x_ = 1.0f - geometryDataEx.lodLevelMix_;
            geometryDataEx.secondaryInstanceData_.x_ = 2.0f - geometryDataEx.lodLevelMix_;
        }
    }

    unsigned StaticModelEx::ComputeBestLod(float distance, unsigned currentLod, const PODVector<Vector2>& distances)
    {
        const unsigned numLods = distances.Size();

        // Compute best LOD
        unsigned bestLod = 0xffffffff;
        for (unsigned i = 0; i < numLods - 1; ++i)
        {
            // Inner and outer distances for i-th LOD
            const float innerDistance = distances[i + 1].x_;
            const float outerDistance = distances[i + 1].y_;

            if (distance < innerDistance)
            {
                // Nearer than inner distance of i-th LOD, so use it
                bestLod = i;
                break;
            }
            else if (distance < outerDistance)
            {
                // Nearer that outer distance of i-th LOD, so it must be i-th or at least i+1-th level
                bestLod = Clamp(currentLod, i, i + 1);
                break;
            }
        }

        return Min(bestLod, distances.Size() - 1);
    }

    void StaticModelEx::UpdateLodLevels(const FrameInfo& frame)
    {
        /// #TODO Add immediate switch if invisible
        // Update distances
        const BoundingBox& worldBoundingBox = GetWorldBoundingBox();
        distance_ = frame.camera_->GetDistance(worldBoundingBox.Center());

        const unsigned numBatches = batches_.Size() / 2;
        if (numBatches == 1)
        {
            batches_[0].distance_ = distance_;
        }
        else
        {
            const Matrix3x4& worldTransform = node_->GetWorldTransform();
            for (unsigned i = 0; i < numBatches; ++i)
            {
                batches_[i].distance_ = frame.camera_->GetDistance(worldTransform * geometryData_[i].center_);
                batches_[i + numBatches].distance_ = batches_[i].distance_;
            }
        }

        // Update LODs
        float scale = worldBoundingBox.Size().DotProduct(DOT_SCALE);
        float newLodDistance = frame.camera_->GetLodDistance(distance_, scale, lodBias_);

        assert(numLodSwitchAnimations_ >= 0);
        if (newLodDistance != lodDistance_ || numLodSwitchAnimations_ > 0)
        {
            lodDistance_ = newLodDistance;
            CalculateLodLevels(frame.timeStep_);
        }

		////TESTING ROTATIONS FOR BILLBOARD
		//if (lodDistance_ > 10) {
		//	GetNode()->SetWorldRotation(frame.camera_->GetFaceCameraRotation(GetNode()->GetPosition(), GetNode()->GetRotation(), Atomic::FC_ROTATE_XYZ));
		//}
    }

    void StaticModelEx::UpdateWind()
    {
        if (windSystem_ && applyWind_)
        {
            if (cloneMaterials_ && windSystem_->HasLocalWindZones())
            {
                const Pair<WindSample, bool> sample = windSystem_->GetWindSample(node_->GetWorldPosition());
                if (sample.second_)
                {
                    SetCloneRequest(CR_WIND, true);
                    for (unsigned i = 0; i < batches_.Size(); ++i)
                    {
                        if (batches_[i].material_)
                            WindSystem::SetMaterialWind(*batches_[i].material_, sample.first_);
                    }
                }
                else
                {
                    SetCloneRequest(CR_WIND, false);
                }
            }
            else
            {
                SetCloneRequest(CR_WIND, false);
            }
        }
    }


    Vector3 StaticModelEx::makeCeil(Vector3& first, Vector3& second)
    {
        return Vector3(Max(first.x_, second.x_), Max(first.y_, second.y_), Max(first.z_, second.z_));
    }

    float StaticModelEx::boundingRadiusFromAABB(BoundingBox& aabb)
    {
        Vector3& max = aabb.max_;
        Vector3& min = aabb.min_;

        Vector3 magnitude = max;
        magnitude = makeCeil(magnitude, -max);
        magnitude = makeCeil(magnitude, min);
        magnitude = makeCeil(magnitude, -min);

        return magnitude.Length();
    }

	SharedPtr<Geometry> StaticModelEx::CreateQuadGeom() {

		const unsigned numVertices = 4;
		const unsigned numIndices = 6;

		float vertexData[]{
			// Position             Normal                    Texture
			-0.5f, -0.5f,  0.5f,     0.0f,  1.0f,  0.0f,      0.0f, 0.0f,
			-0.5f, 0.5f,  0.5f,     0.0f,  1.0f,  0.0f,      1.0f, 0.0f,
			0.5f, 0.5f,  0.5f,     0.0f,  1.0f,  0.0f,       1.0f, 1.0f,
			0.5f,  -0.5f,  0.5f,     0.0f,  1.0f,  0.0f,       0.0f, 1.0f
		};

		unsigned short indexData[]{
			0, 1, 2,
			3, 0, 2
		};

		SharedPtr<VertexBuffer> vb(new VertexBuffer(context_));
		SharedPtr<IndexBuffer> ib(new IndexBuffer(context_));
		SharedPtr<Geometry> geom(new Geometry(context_));
		// Shadowed buffer needed for raycasts to work, and so that data can be automatically restored on device loss
		vb->SetShadowed(true);
		// We could use the "legacy" element bitmask to define elements for more compact code, but let's demonstrate
		// defining the vertex elements explicitly to allow any element types and order
		PODVector<VertexElement> elements;
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
		elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));
		elements.Push(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD));
		vb->SetSize(numVertices, elements);
		vb->SetData(vertexData);

		ib->SetShadowed(true);
		ib->SetSize(numIndices, false);
		ib->SetData(indexData);

		geom->SetVertexBuffer(0, vb);
		geom->SetIndexBuffer(ib);
		geom->SetDrawRange(TRIANGLE_LIST, 0, numIndices);
		geom->SetLodDistance(0);

		return geom;

	}

	SharedPtr<Texture2D> StaticModelEx::CreateBillboardTexture()
	{
		SharedPtr<Texture2D> tex(new Texture2D(context_));
		tex->SetFilterMode(TextureFilterMode::FILTER_NEAREST);
		tex->SetNumLevels(1);
		tex->SetSize(IMPOSTER_SIZE, IMPOSTER_SIZE, Graphics::GetRGBAFormat(), TEXTURE_STATIC);
		tex->SetData(billboardImage_, true);
		return tex;
	}

	void StaticModelEx::CreateBillboardImage() {
		ResourceCache* cache = GetSubsystem<ResourceCache>();
		Image* cached = cache->GetResource<Image>(GetModel()->GetName() + ".png");
		if (cached)
		{
				billboardImage_ = cached;
		        return;
	    }

		
        Renderer *renderer = GetSubsystem<Renderer>();
        Scene* m_p3DViewportScene = new Scene(context_);
        m_p3DViewportScene->CreateComponent<Octree>();

        Node* _pZoneAmbientNode = m_p3DViewportScene->CreateChild("Zone");

        if (_pZoneAmbientNode != nullptr)
        {

            Zone* _pZoneAmbient = _pZoneAmbientNode->CreateComponent<Zone>();

            if (_pZoneAmbient != nullptr)
            {
                _pZoneAmbient->SetBoundingBox(BoundingBox(-2048.0f, 2048.0f));
                Zone* mainzone = nullptr;

                _pZoneAmbient->SetAmbientColor(Color(0.5f, 0.5f, 0.5f));

            }

        }

        // Create camera viewport
        Node* m_p3DViewportCameraNode = m_p3DViewportScene->CreateChild();
        Camera* billboardCam;

        if (m_p3DViewportCameraNode != nullptr)
            billboardCam = m_p3DViewportCameraNode->CreateComponent<Camera>();

        // Create rendertarget
        Texture2D* billboardRenderTex = new Texture2D(context_);
        billboardRenderTex->SetSize(IMPOSTER_SIZE, IMPOSTER_SIZE, Graphics::GetRGBAFormat(), TEXTURE_RENDERTARGET);
        billboardRenderTex->SetFilterMode(FILTER_TRILINEAR);

        RenderSurface* billboardSurface = billboardRenderTex->GetRenderSurface();

        if (billboardSurface == nullptr)
            return;

        Viewport* billboardViewport = (new Viewport(context_, m_p3DViewportScene, m_p3DViewportCameraNode->GetComponent<Camera>()));
        SharedPtr<RenderPath> billboardpath = billboardViewport->GetRenderPath()->Clone();

        RenderTargetInfo target;
        target.enabled_ = true;
        target.name_ = "treetarget";
        target.tag_ = "TreeTarget";
        target.format_ = Graphics::GetRGBAFormat();
        target.sizeMode_ = SIZE_VIEWPORTDIVISOR;
        target.size_ = Vector2(4, 4);
        target.filtered_ = false;
        target.persistent_ = true;

		billboardpath->AddRenderTarget(target);

        billboardViewport->SetRenderPath(billboardpath);

        billboardSurface->SetViewport(0, billboardViewport);
        billboardSurface->SetUpdateMode(RenderSurfaceUpdateMode::SURFACE_MANUALUPDATE);


        Node* billboardNode = m_p3DViewportScene->CreateChild();

        StaticModel* billboardModel = billboardNode->CreateComponent<StaticModel>();
        billboardModel->SetModel(GetModel());
        billboardModel->SetCastShadows(false);
        //billboardModel->ApplyMaterialList();
        billboardModel->SetMaterial(GetMaterial());

        BoundingBox boundingbox = billboardModel->GetBoundingBox();
        Vector3 entityCenter = boundingbox.Center();
        float entityRadius = boundingRadiusFromAABB(boundingbox);
        float entityDiameter = 2.0f * entityRadius;

        //Set up camera FOV
        float objDist = entityRadius;
        float nearDist = objDist - (entityRadius + 1);
        float farDist = objDist + (entityRadius + 1);

        if (billboardCam != nullptr)
        {

            billboardCam->SetLodBias(1000.0f);
            billboardCam->SetAspectRatio(1.0f);

            billboardCam->SetFov(Atan(entityDiameter / objDist));
            billboardCam->SetFarClip(farDist);
            billboardCam->SetNearClip(nearDist);
            billboardCam->SetOrthographic(true);
        }

        billboardNode->SetPosition(-entityCenter + Vector3(0, 0, 0));
        billboardNode->SetRotation(Quaternion(0.0f, 0.0f, 0.0f));

        if (true) {
            //If this has not been pre-rendered, do so now
            const float xDivFactor = 1.0f / IMPOSTOR_YAW_ANGLES;
            const float yDivFactor = 1.0f / IMPOSTOR_PITCH_ANGLES;
            for (int o = 0; o < IMPOSTOR_PITCH_ANGLES; ++o) { //4 pitch angle renders
               #ifdef IMPOSTOR_RENDER_ABOVE_ONLY
                float pitch = Degree((90.0f * o) * yDivFactor); //0, 22.5, 45, 67.5
               #else
                float pitch = ((180.0f * o) * yDivFactor - 90.0f);
               #endif

                for (int i = 0; i < IMPOSTOR_YAW_ANGLES; ++i) { //8 yaw angle renders
                    float yaw = (360.0f * i) * xDivFactor; //0, 45, 90, 135, 180, 225, 270, 315
                                                           //Position camera
                    m_p3DViewportCameraNode->SetPosition(Vector3(0, 0, 0));
                    m_p3DViewportCameraNode->SetRotation(Quaternion(yaw, Vector3::UP) * Quaternion(-pitch, Vector3::RIGHT));
                    m_p3DViewportCameraNode->Translate(Vector3(0, 0, -objDist), TS_LOCAL);

                    //Render the impostor
                    int width = billboardSurface->GetWidth() / IMPOSTOR_YAW_ANGLES;
                    int height = billboardSurface->GetHeight() / IMPOSTOR_PITCH_ANGLES;
                    int left = (float)(i)* width;
                    int top = (float)(o)* height;
                    IntRect region = IntRect(left, top, left + width, top + height);
                    billboardViewport->SetRect(region);
                    //renderer->SetViewport(0, billboardViewport);
                    billboardSurface->QueueUpdate();
                    renderer->Update(1.0f);
                    renderer->Render();
                }
            }
        }

        // Image saving
        billboardImage_ = new Image(context_);

        unsigned char* imageData = new unsigned char[billboardRenderTex->GetDataSize(IMPOSTER_SIZE, IMPOSTER_SIZE)];
        billboardRenderTex->GetData(0, imageData);

        int channels = billboardRenderTex->GetComponents();
        Color transparentcolor = Color::BLACK; //Black is transparent

        for (int i = 0; i < IMPOSTER_SIZE * IMPOSTER_SIZE; i++) {
            //If pixel is the color we want to use as transparent in the imposter, set its alpha to 0
            if (Color(imageData[4 * i], imageData[4 * i + 1], imageData[4 * i + 2]) == transparentcolor) {
				imageData[4 * i + 3] = 0; //ALPHA
            }
            else {
				imageData[4 * i + 3] = 255;
            }
        }

		ToolCore::ToolSystem* toolsystem = GetSubsystem<ToolCore::ToolSystem>();
		String myresources = "";
		if (toolsystem) {
			ToolCore::Project* project = toolsystem->GetProject();
			myresources = project->GetProjectPath() + "Resources/";
		}

        billboardImage_->SetSize(IMPOSTER_SIZE, IMPOSTER_SIZE, channels);
        billboardImage_->SetData(imageData);
		String name = model_->GetName() + ".png";
        billboardImage_->SavePNG(myresources + name);
        ATOMIC_LOGDEBUG("Wrote " + myresources + name + "test.png");

        delete[] imageData;
    }

	SharedPtr<Geometry> StaticModelEx::AddImposter() {
		SharedPtr<Geometry> imposter = CreateQuadGeom();
	/*	CreateBillboardImage();
		Texture2D* tex = CreateBillboardTexture();
		Material* mat = new Material(context_);*/
		imposter->SetLodDistance(10);
		//unsigned levels = model_->GetNumGeometryLodLevels(0) + 1;
		//model_->SetNumGeometryLodLevels(0,2);
		//model_->SetGeometry(0, 1, imposter);
		return imposter;
	}
	void  StaticModelEx::GenerateImpostorTexture() {
		CreateBillboardImage();
		Texture2D* tex = CreateBillboardTexture();
		
		Material* mat = GetMaterial(); //new Material(context_);
		
		Technique* tech0 = new Technique(context_);
		tech0->SetName("lod1");

		mat->SetTechnique(1, tech0, 0, 10);
		mat->SetTexture(TextureUnit::TU_DIFFUSE, tex);
		//mat->
		//mat->SetTexture(
		Geometry* tmp = model_->GetGeometry(0, 1);
		
	}

}
