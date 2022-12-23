# Model View Projection

An OpenGL ES application written in C99 that lets you inspect 3D transform matrices used to draw an object in a scene.

To look around, hold down right mouse and move the mouse. You can fly forward, backward, or strafe with WASD.

When you move the camera or manipulate the controls, you can see how they affect the values of the model, view, and projection transform matrices in real-time.

See [accompanying blog post](https://jeskin.net/blog/model-view-projection/)

![preview](https://github.com/jpe90/images/raw/master/mvp_ss.png)

The "Cube" section on the left modifies the model matrix calculation.

The "Scene camera" section affects the grey camera depicted in the scene. Checking the "Fix Eye to camera" box switches to this camera's perspective.

# Dependencies

- cglm [(AUR)](https://aur.archlinux.org/packages/cglm) [(Ubuntu)](https://launchpad.net/ubuntu/+source/cglm)
- SDL2
- OpenGL ES

# Building

```Bash
make
```

# Credit

- The project was forked off an example in the [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) repo
- [Tizen](https://developer.tizen.org/dev-guide/2.3.1/org.tizen.tutorials/html/native/graphics/opengl_tutorial_n.htm) has excellent documentation and I took a handful of code from the linked example.
