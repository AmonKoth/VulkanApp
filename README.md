# Doom-Kan : Doom level renderer done in Vulkan
The mvp of this project is to create a rendering engine in C++ using Vulkan API. What I have done up until now is:

-	Created the window using GlFW
-	Created physical and logical devices.
-	Created the Validation Layer
-	Created Surface Swapchain and Staging buffers.
-	Created the Graphics Pipeline
-	Created Vertex and Staging buffers.
-	Created Dynamic Uniform Buffers
-	Created Push Constants
-	Texture Loading

Even though since this project is only going to render walls and levels, it is not necessary to add model loading but I have added model loading in to this project using the assimp library.
Also the way that I am going to create the level is I'm going to read through the original DOOM (1993) .wad file and genarate the vertices and indices from there.

What are the next steps?
Creating Input Management
Assigning the textures to corresponding drawn walls
Creating simple UI elements
