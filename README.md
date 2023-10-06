# Building From Spline

![A castle-like building](Gallery/castle.png?raw=true "A castle-like building")
![Three buildings](Gallery/three-buildings.png?raw=true "Three buildings")
![Mushroom shaped building](Gallery/mushroom.png?raw=true "Mushroom shaped building")

## Description

Building From Spline is an Unreal Engine 5.3 plugin that creates a customizable building from a spline.

Search for `Building` in the Content Browser (make sure that `Show C++ classes` is enabled), and drag it into your level.
Select the newly added building's spline, and add and place points as you wish to draw the floor outline of your building.
In the details panel, press the `Generate Building` button, and that's it, a building is generated!

In the `Building Configuration` section of your building, you have several parameters that you can change:
materials, number of floors, floor size, windows sizes, distance between windows, windows meshes, roof parameters, etc.

If you want a curved building, make sure that your spline has curves (select all spline points, and select `Curve` as a point type),
and increase the number of `Wall Divisions` in the `Building Configuration`.
