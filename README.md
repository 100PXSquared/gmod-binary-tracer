# GMod Binary Module Path Tracer
Monte-Carlo path tracer C++ binary module and GLua interface for GMod  
*(still working on this, right now trying to implement a Cook-Torrance Microfacet BRDF)*  

### Features
* Multi-threaded denoiser
* Importance sampling
* Next event estimation for direct lights (right now only point and directional, emissives don't have NEE, although this means they produce caustics, as they're hit at random)
* Image based lighting

Two of my favourite renders showing the denoiser:  
**Render:**  
![render](https://github.com/100PXSquared/gmod-binary-tracer/blob/master/Example%20Renders/first%20denoised%20render%20%C2%A6%2032spp%20512x512%20%C2%A6%20Took%204s%20%C2%A6%20Render.png?raw=true)  
**Denoised:**  
![render](https://github.com/100PXSquared/gmod-binary-tracer/blob/master/Example%20Renders/first%20denoised%20render%20%C2%A6%2032spp%20512x512%20%C2%A6%20Took%204s%20%C2%A6%20Denoised.png?raw=true)  

**Render:**  
![render](https://github.com/100PXSquared/gmod-binary-tracer/blob/master/Example%20Renders/chess%20board%20%C2%A6%20didn't%20save%20the%20sample%20count%201024x1024%20%C2%A6%20didn't%20save%20the%20time%20%C2%A6%20Render.png?raw=true)  
**Denoised:**  
![render](https://github.com/100PXSquared/gmod-binary-tracer/blob/master/Example%20Renders/chess%20board%20%C2%A6%20didn't%20save%20the%20sample%20count%201024x1024%20%C2%A6%20didn't%20save%20the%20time%20%C2%A6%20Denoised.png?raw=true)  
