A bindless renderer using DirectX 12, featuring deferred shading, raytracing shadow, indirect diffuse lighting, and irradiance volume.

<img width="1918" height="1013" alt="image" src="https://github.com/user-attachments/assets/3a35c627-aa7b-4341-9b18-034f1730bc47" />

`CSceneObject` forms the scene graph. It maintains transform related info and a list of child scene objects, and allows multiple `CMesh` added to it for rendering. The properties `Roughness` and `Metallic` from scene object are only used when corresponding texture is not present in the `.mtl` file (by field `map_Pr`).  
`CMaterial` wraps the root signature, PSOs (graphics, compute, raytracing), and the shader binding table. It reflects the shader blobs to generate root parameters, builds root signature, and provides helper functions to bind resources by register.  
`CScene` owns all scene objects and meshes, and all mesh info are collected using a structured buffer (`ModelBuffer`). It loads the scene from `.obj` files specified in `assets/scene.json`, and builds raytracing acceleration structures as needed.  
`CRenderer` is a singleton class. It manages descriptor allocation, textures, view buffers and render passes.  
`CShadowRTPass` traces shadow rays against the main directional light, and saves result in a screen-sized render texture.  
`CIrradianceVolumeRTPass` maintains a probe grid over the scene bounds. It accumulates irradiance and stores the result as L1 spherical harmonics in three 3D textures.   
`CIndirectLightRTPass` raytraces indirect light per pixel from the G-buffer. It then uses temporal accumulation and A-Trous denoiser to get stable one-bounce indirect lighting.  
`CLightPass` calculates PBR lighting, composites shadow and indirect lighting, then applies tonemapping before presenting to the framebuffer.  
 
Controls: using WASD key to move camera, pressing LMB to rotate camera, and pressing RMB to change light direction.
