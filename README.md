# FXAA on Compute Queue in Direct3D 12

This project was created to learn the Direct3D 12 API as a part of the DV2511 3D Programming III course.

We investigated whether or not moving the FXAA stage (which was implemented as a compute shader) from the graphics queue to the compute queue might have a performance benefit by allowing for higher parallelism.
Our measurements&mdash;both the ones recorded in the application itself and by [GPUView](https://graphics.stanford.edu/~mdfisher/GPUView.html)&mdash;showed that this was not the case. 
Due to the nature of the program and the minuscule execution time of the FXAA stage (in comparison to the geometry rendering stage that came before it), the rendering work for the next frame on the graphics queue would not take place before the previous frame had been presented.
Therefore, moving the FXAA stage to the compute queue did not improve the parallelism in our case.

# Demo
The images below show 104 Stanford Dragons rendered by the application, before and after the FXAA demo pass.
![Before FXAA](/GitHubMedia/FXAAin.png?raw=true "Before FXAA")
![After FXAA](/GitHubMedia/FXAAout.png?raw=true "After FXAA")
- **Top left quadrant: 3x magnification.** 
    - **Left side:** FXAA off.
    - **Right side:** FXAA on.
- **Bottom left quadrant:** Edges detected by the FXAA algorithm.
- **Top right quadrant:** FXAA on.
- **Bottom right quadrant:** FXAA off.


# Developers

[Viktor Enfeldt](https://github.com/viktor4006094)

[Peter Meunier](https://github.com/soridanm)