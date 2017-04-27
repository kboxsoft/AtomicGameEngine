#pragma once

#include <Atomic/Core/CoreEvents.h>
#include <Atomic/Engine/Engine.h>
#include <Atomic/Scene/LogicComponent.h>
#include <Atomic/Graphics/Viewport.h>
#include <Atomic/Graphics/Octree.h>

namespace Atomic
{


	class SmoothFocus : public LogicComponent
	{
		ATOMIC_OBJECT(SmoothFocus, LogicComponent);
	public:
		/// Construct.
		///
		SmoothFocus(Context* context);

		/// Destruct.
		~SmoothFocus();

		static void RegisterObject(Context* context);
		virtual void Start();
		void PostUpdate(float timeStep);
		void SetViewport(SharedPtr<Viewport> viewport);
		void SetFocusTime(float time);
		void SetSmoothFocusEnabled(bool enabled);
		bool GetSmoothFocusEnabled();

		float smoothTimeElapsed;
		float smoothFocus;
		float smoothFocusRawLast;
		float smoothFocusRawPrev;
		float smoothTimeSec;

	private:
		SharedPtr<Camera> camera;
		SharedPtr<RenderPath> rp;
		SharedPtr<Octree> octree;
		SharedPtr<Viewport> vp;

		// Get closer hit with scene drawables(from screen center), else return far value
		float GetNearestFocus(float zCameraFarClip);

	};
}