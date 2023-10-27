Landscape Combinator
====================

Landscape Combinator is an `Unreal Engine 5.3 plugin <https://www.unrealengine.com/marketplace/en-US/product/landscape-combinator/>`_
that enables you to create landscapes from real-world data in a few simple steps:

* :ref:`Create landscapes <heightmaps>` from real-world heightmaps from many sources,
* :ref:`Create splines <splines>` from `OSM <https://www.openstreetmap.org>`_ data for roads, rivers, etc.
* Automatically generate buildings from `OSM <https://www.openstreetmap.org>`_,
* Add procedural foliage (such as forests) based on `OSM <https://www.openstreetmap.org>`_ data.

When creating a landscape, the plugin will adjust its position and scale correctly in a planar world.
You can thus create several real-world landscapes with different resolutions and have them placed and
scaled correctly with respect to one another. The plugin also has :ref:`a blending feature <blending>`
to modify the landscapes heightmap data to make the transition between different landscape resolution as
seamless as possible.

.. toctree::
   :maxdepth: 2
   
   installation
   coordinates
   heightmaps
   blending
   splines
   ogrfilter
   building
   credit
