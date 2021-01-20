# Example Renders

Some of the best renders from my tracer, all of which are from before starting to implement Cook-Torrance, as my tracer is currently non-functional.  

Each render is titled as `{name} ¦ {samples}spp {width}x{height} ¦ Took {render time if render, otherwise denoise time if denoised}s`, and whether it's the output render before denoising, or after denoising, is denoted by ` ¦ Render` and ` ¦ Denoised`.  
*If a render has no ` ¦ Denoised` counterpart, then it's from before I wrote the denoiser, or the denoiser failed to produce a useable image (there's a caustics test where the albedo map didn't contain information for refractions, so everything refracted was blurred together)*

Note that the tracer outputs to P6 PPMs not PNG, which is easy to change, I just haven't got around to including a 3rd party image library yet, so I just have a simple python script to auto convert them.