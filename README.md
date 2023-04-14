# Model View Projection

An OpenGL ES application written in C99 that lets you inspect 3D transform matrices used to draw an object in a scene.

To look around, hold down right mouse and move the mouse. You can fly forward, backward, or strafe with WASD.

When you move the camera or manipulate the controls, you can see how they affect the values of the model, view, and projection transform matrices in real-time.

See [accompanying blog post](https://jeskin.net/blog/model-view-projection/)

![preview](https://github.com/jpe90/images/raw/master/mvp_ss.png)

The "Cube" section on the left modifies the model matrix calculation.

The "Scene camera" section affects the grey camera depicted in the scene. Checking the "Fix Eye to camera" box switches to this camera's perspective.

# Dependencies

- SDL2
- OpenGL3

# Building

```Bash
make
```