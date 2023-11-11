Coordinates
===========

The ``LevelCoordinates`` actor is used to georeference your level. It lets you
specify a coordinate system, the longitude and latitude of a real-world location,
and the number of Unreal Engine units (cm) per longitude/latitude unit. The
location that you choose in the ``LevelCoordinates`` corresponds to the (0, 0)
location of Unreal Engine.
You can use `epsg.io <https://epsg.io/map#srs=4326>`_ to find real-world coordinates in EPSG 4326
(this works better in Chrome than in Firefox in my experience). You can also change the URL if you
need coordinates in another EPSG.

You can add an ``ActorCoordinates`` component to an actor that you wish to place on a particular
real-world location. Clicking the ``Move Actor`` button on this component will move the actor
correctly with respect to the ``LevelCoordinates``.

If you are creating landscapes using a :ref:`LandscapeSpawner <landscape-spawner>`,
a ``LevelCoordinates`` actor will be created automatically for some well-known
coordinate systems such as ``EPSG:4326``, ``EPSG:2056``, ``EPSG:2154``,
``CRS:84``.

