

Building
========

Drag a ``Building`` actor into your level, adjust the spline to draw the floor outline of a building, and click on ``Generate Building``.
In the Building Configuration section of your building, you can change:
materials, number of floors, floor size, windows sizes, distance between windows, windows meshes, roof parameters, etc.
If you want a curved building, make sure that your spline has curves (select all spline points, and select Curve as a point type),
and increase the number of ``Wall Divisions`` in the Building Configuration.

.. _building-configuration-settings:

Building Configuration
----------------------

The building is built from a :ref:`Building Configuration <building-configuration-settings>` which defines its structure.
The configuration is organized into several sections:

**Structure**: Choose between ``Building Without Inside`` (a simple solid block, best for performance) and
``Building With Floors And Empty Inside`` (a hollow building with separate floors and internal walls).
You can set the wall thicknesses, the number of floors (or let it be auto-computed from OSM data),
and add extra wall height above and below the floors.

**Levels**: A building is composed of a sequence of levels (floors), each with its own height and thickness.
You define the available levels in the ``Levels Map`` (a dictionary of ``LevelDescription`` objects),
then specify their order and repetition with the ``Levels Expression``. See below for the grammar.

**Wall Segments**: Within each level, the walls are composed of wall segments that are placed along the building spline.
Wall segments can be solid walls or holes (for windows and doors). You define the available wall segments
in the ``Wall Segments Map`` of each level, then specify their order along the spline with the
``Wall Segments Expression``. By default, wall segments repeat for each segment between corners;
set ``bResetWallSegmentsOnCorners`` to false to apply the same pattern to the whole building.

**Material Expressions**: Materials are referenced by name using expressions.
For instance, ``{Brick:3, Stone:1}`` will choose Brick 75% of the time
and Stone 25% of the time. Wall segments can also override materials per face (interior/exterior, above/below holes).

**Roof**: Choose from ``None``, ``Flat``, ``Point``, ``Inner Spline``, ``Hip``, or ``Gable`` roofs.
Hip and Gable roofs are computed using a straight skeleton algorithm, which automatically creates
complex roof shapes from the building footprint.

**Level Expression Grammar**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``Levels Expression`` and ``Wall Segments Expression`` use a simple grammar:

* Space-separated names are concatenated in order.
* ``*`` repeats the preceding element zero or more times.
* ``+`` repeats the preceding element one or more times.
* ``{A:n, B:m}`` randomly chooses between A and B with relative weights (here, A appears n/(n+m) of the time).
* ``Name:N`` repeats the element exactly N times.

Example: ``GroundLevel OtherLevel*`` means one ground floor followed by zero or more ``OtherLevel`` floors.
Example: ``{LevelA:4, LevelB:1}`` randomly picks LevelA (80%) or LevelB (20%) for each occurrence.
Example: ``GroundLevel {LevelA:3, LevelB:1}+`` means one ground floor followed by one or more
randomly chosen LevelA or LevelB levels.


Building Configuration Settings
-------------------------------

.. include:: params/BuildingConfiguration.inc


Building Settings
-----------------

.. include:: params/Building.inc
