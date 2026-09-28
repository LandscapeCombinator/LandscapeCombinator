Landscape Combinator
====================

Landscape Combinator is an `Unreal Engine 5.8 plugin <https://www.unrealengine.com/marketplace/en-US/product/landscape-combinator/>`_
that lets you create landscapes from real-world data in a few simple steps:

* :ref:`Create landscapes <landscape-spawner>` from real-world heightmaps from many sources,
* :ref:`Create landscape meshes <landscape-mesh-spawner>` as static mesh representations,
* :ref:`Create decals with satellite images <landscape-texturer>` for your landscapes,
* :ref:`Create splines <splines>` from `OSM <https://www.openstreetmap.org>`_ data for roads, rivers, etc.
* :ref:`Create roads <roads>` from spline components
* Automatically generate buildings from `OSM <https://www.openstreetmap.org>`_,
* Add procedural foliage (such as forests) based on `OSM <https://www.openstreetmap.org>`_ data.

All content is created using :ref:`Generators <generators>`.

When creating a landscape, the plugin will adjust its position and scale correctly in a planar world.
You can thus create several real-world landscapes with different resolutions and have them placed and
scaled correctly with respect to one another. The plugin also has :ref:`a blending feature <blending>`
to modify the landscapes heightmap data to make the transition between different landscapes as
seamless as possible.

.. toctree::
   :maxdepth: 2

   installation
   coordinates
   generators
   landscape-spawner
   landscape-mesh-spawner
   landscape-texturer
   image-downloader
   blending
   splines
   ogr-geometry
   ogrfilter
   landscape-pcg-volume
   roads
   building
   buildings-from-splines
   position-based-generation
   continuous-generation
   landscape-material
   credit
