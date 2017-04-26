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

#include "JSMath.h"
#include "JSRenderPath.h"

#include <Atomic/Graphics/RenderPath.h>
#include <Atomic/Graphics/Light.h>
#include <Atomic/Graphics/Octree.h>
#include <Atomic/Graphics/Camera.h>
#include <Atomic/Scene/Node.h>

namespace Atomic
{


// RenderPath

static int RenderPath_SetShaderParameter(duk_context* ctx)
{
    duk_push_this(ctx);
	RenderPath* rp = js_to_class_instance<RenderPath>(ctx, -1, 0);

    const char* name = duk_require_string(ctx, 0);
    String value = duk_require_string(ctx, 1);

    const Variant& v = rp->GetShaderParameter(name);

    if (v == Variant::EMPTY)
        return 0;

    Variant vset;
    vset.FromString(v.GetType(), value);

    rp->SetShaderParameter(name, vset);

    return 0;

}




void jsapi_init_renderpath(JSVM* vm)
{
    duk_context* ctx = vm->GetJSContext();


    js_class_get_prototype(ctx, "Atomic", "RenderPath");
    duk_push_c_function(ctx, RenderPath_SetShaderParameter, 2);
    duk_put_prop_string(ctx, -2, "setShaderParameter");
    duk_pop(ctx);


    // static methods
    js_class_get_constructor(ctx, "Atomic", "RenderPath");
    duk_pop(ctx);


}

}
