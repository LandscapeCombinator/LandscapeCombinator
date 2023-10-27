.. _Building:

Building From Spline
====================

Drag a ``Building`` actor into your level, adjust the spline to draw the floor outline of a building, and click on ``Generate Building``.
In the Building Configuration section of your building, you can change:
materials, number of floors, floor size, windows sizes, distance between windows, windows meshes, roof parameters, etc.
If you want a curved building, make sure that your spline has curves (select all spline points, and select Curve as a point type),
and increase the number of ``Wall Divisions`` in the Building Configuration.

You can also generate buildings from many splines (like the ones created in the :ref:`Splines section <splines>`) using
the ``BuildingsFromSplines`` actor.
