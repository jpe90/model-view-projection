# Model View Projection

This is a WIP project that displays the internal state of model, view, and projection matrices that determine how a box is drawn to the screen.
Several controls to transform the box and change a secondary camera's parameter are provided. Manipulating these controls change the MVP matrix state and update the display.

![preview](https://github.com/jpe90/images/raw/master/mvp_ss.png)

The "Cube" section on the left modifies the model matrix calculation.

The "Scene camera" section affects the grey camera depicted in the scene. Checking the "Fix Eye to camera" box switches to this camera's perspective.

# Dependencies

- cglm
- SDL2
- OpenGL ES

# Building

```Bash 
make
```

# Credit

- The project was forked off an example in the [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) repo
- [Tizen](https://developer.tizen.org/dev-guide/2.3.1/org.tizen.tutorials/html/native/graphics/opengl_tutorial_n.htm) has excellent documentation and I took a handful of code from the linked example.
