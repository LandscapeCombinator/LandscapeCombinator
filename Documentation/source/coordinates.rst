Coordinates
===========

The Landscape Combinator plugin needs to map real-world coordinates to Unreal Engine coordinates.
For that, we drag a ``LevelCoordinates`` actor in the level that specifies an EPSG coordinate system, the
longitude and latitude of a real-world location, and the number of Unreal Engine units
(cm) per longitude/latitude unit. The location that you choose in the ``LevelCoordinates``
corresponds to the (0, 0) location of Unreal Engine.
You can use `epsg.io <https://epsg.io/map#srs=4326>`_ to find real-world coordinates in EPSG 4326
(this works better in Chrome than in Firefox in my experience). You can also change the URL if you
need coordinates in another EPSG.

You may add an ``ActorCoordinates`` component to an actor that you wish to place on a particular
real-world location. Clicking the ``Move Actor`` button on this component will move the actor
correctly with respect to the ``LevelCoordinates``.
