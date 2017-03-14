#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"
#include "ScreenPos.glsl"
#include "Lighting.glsl"
#include "Fog.glsl"

#define PI 3.14159265358979323846 //couldn't find pi elsewhere :(

//varying vec2 oUv;
//varying vec3 debugColor;
//varying mat3 viewangle;

int numTilesX = 8; // number of texture tiles columns
int numTilesY = 8; // number of texture tiles rows
float tileWidth = 1.0/numTilesX; // actual tile width in UV space
float tileHeight = 1.0/numTilesY; // actual tile height in UV space

varying vec2 uvCoords;

//varying vec3 DBGCOL;

#ifdef NORMALMAP
    varying vec4 vTexCoord;
    varying vec4 vTangent;
#else
    varying vec2 vTexCoord;
#endif
varying vec3 vNormal;
varying vec4 vWorldPos;
#ifdef VERTEXCOLOR
    varying vec4 vColor;
#endif
#ifdef PERPIXEL
    #ifdef SHADOW
        #ifndef GL_ES
            varying vec4 vShadowPos[NUMCASCADES];
        #else
            varying highp vec4 vShadowPos[NUMCASCADES];
        #endif
    #endif
    #ifdef SPOTLIGHT
        varying vec4 vSpotPos;
    #endif
    #ifdef POINTLIGHT
        varying vec3 vCubeMaskVec;
    #endif
#else
    varying vec3 vVertexLight;
    varying vec4 vScreenPos;
    #ifdef ENVCUBEMAP
        varying vec3 vReflectionVec;
    #endif
    #if defined(LIGHTMAP) || defined(AO)
        varying vec2 vTexCoord2;
    #endif
#endif



/*
mat3 GetTreeViewAngle(vec3 position, vec3 direction, vec3 camPos)
{
    vec3 cameraDir = normalize(position - camPos);
    vec3 front = normalize(direction);
    vec3 right = normalize(cross(front, cameraDir));
    vec3 up = normalize(cross(front, right));

    return mat3(
        right.x, up.x, front.x,
        right.y, up.y, front.y,
        right.z, up.z, front.z
    );
}
*/

mat3 GetTreeRotMatrix(vec3 dir)
{
	vec3 ny = normalize(dir);
	vec3 nz = normalize(cross(ny, vec3(0, 0, -1)));
	vec3 nx = normalize(cross(ny, nz));
	return mat3(-nx, nz, -ny);
}

mat4 RotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

vec2 GetUVtoSample(vec2 uvMins)
{
	return vec2((vTexCoord.x / numTilesX) + uvMins.x, (vTexCoord.y / numTilesY) + 1 - (uvMins.y + tileHeight));
}


void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);

    //viewangle = inverse(GetTreeViewAngle(iPos.xyz, iNormal, cCameraPos)); 
	
    //float red = viewangle[0][1];
   // vec3 hstep = floor(horizontal / (1.0/4)) * (1.0/4);

    gl_Position = GetClipPos(worldPos);
    vNormal = GetWorldNormal(modelMatrix);
    vWorldPos = vec4(worldPos, GetDepth(gl_Position));

    #ifdef VERTEXCOLOR
        vColor = iColor;
    #endif

    #ifdef NORMALMAP
        vec3 tangent = GetWorldTangent(modelMatrix);
        vec3 bitangent = cross(tangent, vNormal) * iTangent.w;
        vTexCoord = vec4(GetTexCoord(iTexCoord), bitangent.xy);
        vTangent = vec4(tangent, bitangent.z);
    #else
        vTexCoord = GetTexCoord(iTexCoord);
    #endif

    #ifdef PERPIXEL
        // Per-pixel forward lighting
        vec4 projWorldPos = vec4(worldPos, 1.0);

        #ifdef SHADOW
            // Shadow projection: transform from world space to shadow space
            for (int i = 0; i < NUMCASCADES; i++)
                vShadowPos[i] = GetShadowPos(i, vNormal, projWorldPos);
        #endif

        #ifdef SPOTLIGHT
            // Spotlight projection: transform from world space to projector texture coordinates
            vSpotPos = projWorldPos * cLightMatrices[0];
        #endif
    
        #ifdef POINTLIGHT
            vCubeMaskVec = (worldPos - cLightPos.xyz) * mat3(cLightMatrices[0][0].xyz, cLightMatrices[0][1].xyz, cLightMatrices[0][2].xyz);
        #endif
    #else
        // Ambient & per-vertex lighting
        #if defined(LIGHTMAP) || defined(AO)
            // If using lightmap, disregard zone ambient light
            // If using AO, calculate ambient in the PS
            vVertexLight = vec3(0.0, 0.0, 0.0);
            vTexCoord2 = iTexCoord1;
        #else
            vVertexLight = GetAmbient(GetZonePos(worldPos));
        #endif
        
        #ifdef NUMVERTEXLIGHTS
            for (int i = 0; i < NUMVERTEXLIGHTS; ++i)
                vVertexLight += GetVertexLight(i, worldPos, vNormal) * cVertexLights[i * 3].rgb;
        #endif
        
        vScreenPos = GetScreenPos(gl_Position);

        #ifdef ENVCUBEMAP
            vReflectionVec = worldPos - cCameraPos;
        #endif
    #endif
	
	
	
	
	//vec3 objectPos = iPos.xyz;//(cModel * vec4(0,0,0,1)).xyz;
	
	//vec3 treeCamVec = vec3(1,1,1) * cBillboardRot;
	
	vec3 treeCamVec = (RotationMatrix(vec3(0,1,0), iNormal.x) * vec4((cCameraPos - GetBillboardPos(iPos, vec2(0,0), iModelMatrix).xyz), 1)).xyz;
	
	float tileMinX = floor(((atan(treeCamVec.x, treeCamVec.z) / PI + 1) / 2) * numTilesX) / numTilesX;
	//float tileMinY = (int((((treeCamVec.y) / length(vec2(treeCamVec.x, treeCamVec.z)) + 1) / 2) * (numTilesY + 1)) - 1) / float(numTilesY + 1) * (tileHeight / (1f / (numTilesY + 1)));
	//float tileMinY = (floor(((treeCamVec.y / length(vec2(treeCamVec.x, treeCamVec.z)) + 1) / 2) * (numTilesY + 1)) - 1) / (numTilesY + 1) * (tileHeight / (1f / (numTilesY + 1)));
	
	//float tileMinY = (floor(((treeCamVecNorm.y / length(vec2(treeCamVecNorm.x, treeCamVecNorm.z)) + 1) / 2) * (numTilesY + 1)) - 1) / (numTilesY + 1) * (tileHeight / (1f / (numTilesY + 1)));
	//float tileMinY = ((((treeCamVecNorm.y / length(vec2(treeCamVecNorm.x, treeCamVecNorm.z))))));

	float tileMinY = (floor((((normalize(treeCamVec).y) + 1) / 2) * (numTilesY + 1)) - 1) / (numTilesY + 1) * (tileHeight / (1f / (numTilesY + 1)));
	
	//cache uv coords
	uvCoords = GetUVtoSample(vec2(tileMinX, tileMinY));
	
	
	// float r = 0;
	// if (cCameraPos.x > GetBillboardPos(iPos, vec2(0,0), iModelMatrix).x){
		// r=1;
	// }
	// DBGCOL = vec3(r,0,0);
	
}



void PS()
{
	vec2 scrolled = uvCoords;
	
	
	
    //Texture coords for the different faces in the atlas
    // vec2 front = vec2((vTexCoord.x / 4), (vTexCoord.y / 4) + 0.5);
    // vec2 right = vec2((vTexCoord.x / 4) + 0.25, (vTexCoord.y / 4) + 0.5);
    // vec2 back = vec2((vTexCoord.x / 4) + 0.5, (vTexCoord.y / 4) + 0.5);
    // vec2 left = vec2((vTexCoord.x / 4) + 0.75, (vTexCoord.y / 4) + 0.5);

    // if(frontside > 0.5)
    // {
    //   scrolled = front;
    // }
    // else if (rightside > 0.5) {
    //   scrolled = right;
    // }
    // else if (leftside > 0.5) {
    //   scrolled = left;
    // }
    // else {
    //   scrolled = back;
    // }

    //Set to front for testing
    //scrolled = front;

    // Get material diffuse albedo
    #ifdef DIFFMAP
        vec4 diffInput = texture2D(sDiffMap, scrolled.xy);
        #ifdef ALPHAMASK
            if (diffInput.a < 0.5)
                discard;
        #endif
        vec4 diffColor = cMatDiffColor * diffInput;
    #else
        vec4 diffColor = cMatDiffColor;
    #endif

    #ifdef VERTEXCOLOR
        diffColor *= vColor;
    #endif

    //TESTING: SET THE COLOR TO SHOW THE ANGLE
    //diffColor.rgb = vec3(viewangle,0,0);
    
	//gl_FragColor = vec4(DBGCOL, 1);
	//return;
	
    // Get material specular albedo
    #ifdef SPECMAP
        vec3 specColor = cMatSpecColor.rgb * texture2D(sSpecMap, scrolled.xy).rgb;
    #else
        vec3 specColor = cMatSpecColor.rgb;
    #endif

    // Get normal
    #ifdef NORMALMAP
        mat3 tbn = mat3(vTangent.xyz, vec3(vTexCoord.zw, vTangent.w), vNormal);
        vec3 normal = normalize(tbn * DecodeNormal(texture2D(sNormalMap, scrolled.xy)));
    #else
        vec3 normal = normalize(vNormal);
    #endif

    // Get fog factor
    #ifdef HEIGHTFOG
        float fogFactor = GetHeightFogFactor(vWorldPos.w, vWorldPos.y);
    #else
        float fogFactor = GetFogFactor(vWorldPos.w);
    #endif

    #if defined(PERPIXEL)
        // Per-pixel forward lighting
        vec3 lightColor;
        vec3 lightDir;
        vec3 finalColor;

        float diff = GetDiffuse(normal, vWorldPos.xyz, lightDir);

        #ifdef SHADOW
            diff *= GetShadow(vShadowPos, vWorldPos.w);
        #endif
    
        #if defined(SPOTLIGHT)
            lightColor = vSpotPos.w > 0.0 ? texture2DProj(sLightSpotMap, vSpotPos).rgb * cLightColor.rgb : vec3(0.0, 0.0, 0.0);
        #elif defined(CUBEMASK)
            lightColor = textureCube(sLightCubeMap, vCubeMaskVec).rgb * cLightColor.rgb;
        #else
            lightColor = cLightColor.rgb;
        #endif
    
        #ifdef SPECULAR
            float spec = GetSpecular(normal, cCameraPosPS - vWorldPos.xyz, lightDir, cMatSpecColor.a);
            finalColor = diff * lightColor * (diffColor.rgb + spec * specColor * cLightColor.a);
        #else
            finalColor = diff * lightColor * diffColor.rgb;
        #endif

        #ifdef AMBIENT
            finalColor += cAmbientColor.rgb * diffColor.rgb;
            finalColor += cMatEmissiveColor;
            gl_FragColor = vec4(GetFog(finalColor, fogFactor), diffColor.a);
        #else
            gl_FragColor = vec4(GetLitFog(finalColor, fogFactor), diffColor.a);
        #endif
    #elif defined(PREPASS)
        // Fill light pre-pass G-Buffer
        float specPower = cMatSpecColor.a / 255.0;

        gl_FragData[0] = vec4(normal * 0.5 + 0.5, specPower);
        gl_FragData[1] = vec4(EncodeDepth(vWorldPos.w), 0.0);
    #elif defined(DEFERRED)
        // Fill deferred G-buffer
        float specIntensity = specColor.g;
        float specPower = cMatSpecColor.a / 255.0;

        vec3 finalColor = vVertexLight * diffColor.rgb;
        #ifdef AO
            // If using AO, the vertex light ambient is black, calculate occluded ambient here
            finalColor += texture2D(sEmissiveMap, vTexCoord2).rgb * cAmbientColor.rgb * diffColor.rgb;
        #endif

        #ifdef ENVCUBEMAP
            finalColor += cMatEnvMapColor * textureCube(sEnvCubeMap, reflect(vReflectionVec, normal)).rgb;
        #endif
        #ifdef LIGHTMAP
            finalColor += texture2D(sEmissiveMap, vTexCoord2).rgb * diffColor.rgb;
        #endif
        #ifdef EMISSIVEMAP
            finalColor += cMatEmissiveColor * texture2D(sEmissiveMap, vTexCoord.xy).rgb;
        #else
            finalColor += cMatEmissiveColor;
        #endif

        gl_FragData[0] = vec4(GetFog(finalColor, fogFactor), 1.0);
        gl_FragData[1] = fogFactor * vec4(diffColor.rgb, specIntensity);
        gl_FragData[2] = vec4(normal * 0.5 + 0.5, specPower);
        gl_FragData[3] = vec4(EncodeDepth(vWorldPos.w), 0.0);
    #else
        // Ambient & per-vertex lighting
        vec3 finalColor = vVertexLight * diffColor.rgb;
        #ifdef AO
            // If using AO, the vertex light ambient is black, calculate occluded ambient here
            finalColor += texture2D(sEmissiveMap, vTexCoord2).rgb * cAmbientColor.rgb * diffColor.rgb;
        #endif
        
        #ifdef MATERIAL
            // Add light pre-pass accumulation result
            // Lights are accumulated at half intensity. Bring back to full intensity now
            vec4 lightInput = 2.0 * texture2DProj(sLightBuffer, vScreenPos);
            vec3 lightSpecColor = lightInput.a * lightInput.rgb / max(GetIntensity(lightInput.rgb), 0.001);

            finalColor += lightInput.rgb * diffColor.rgb + lightSpecColor * specColor;
        #endif

        #ifdef ENVCUBEMAP
            finalColor += cMatEnvMapColor * textureCube(sEnvCubeMap, reflect(vReflectionVec, normal)).rgb;
        #endif
        #ifdef LIGHTMAP
            finalColor += texture2D(sEmissiveMap, vTexCoord2).rgb * diffColor.rgb;
        #endif
        #ifdef EMISSIVEMAP
            finalColor += cMatEmissiveColor * texture2D(sEmissiveMap, vTexCoord.xy).rgb;
        #else
            finalColor += cMatEmissiveColor;
        #endif

        gl_FragColor = vec4(GetFog(finalColor, fogFactor), diffColor.a);
    #endif
		
}