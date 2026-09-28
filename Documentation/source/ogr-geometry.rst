.. _ogr-geometry:

OGR Geometry
============

The ``OGRGeometry`` actor lets you import vector geometry data from `OSM <https://www.openstreetmap.org>`_
or any file format supported by `GDAL <https://gdal.org/>`_ and visualize the imported areas on your landscape.

Import Geometry
---------------

#. First make sure that you have a :ref:`Level Coordinates <coordinates>` actor in your level.
#. Search for ``OGRGeometry`` in the Content Browser, and drag one into your level.
#. In the Details Panel, select a geometry source:

   * **OSM Roads**, **OSM Rivers**, **OSM Buildings**, **OSM Forests**, **OSM Beaches**,
     **OSM Parks**, **OSM Ski Slopes**, or **OSM Grass** for built-in OSM data queries
   * **Overpass Short Query** or **Overpass Query** for custom Overpass API queries
   * **Local File** for any vector file supported by GDAL (e.g., Shapefile, GeoJSON, KML)
#. Choose a ``Bounding Actor`` (a volume, landscape, or rectangular actor) to define the area
   for importing geometry, or use tile-based bounding.
#. Press the ``Import Geometry`` button.

Preview Decal
-------------

Once geometry is imported, you can preview it as a colored decal overlay on your landscape:

* Enable **Show Geometry Preview** to display the imported geometry on the landscape
* Adjust the **Preview Material**, **Preview Resolution**, and **Preview Color** to customize the look

This preview helps you verify that the geometry covers the correct area before using it for filtering.

Use with OGR Filter
-------------------

The imported geometry can be used with the :ref:`OGR Filter <ogrfilter>` node in PCG to filter
procedural generation points based on the imported area. For example, you can spawn trees only
within OSM forest areas or place foliage only in park boundaries.

See :ref:`OGR Filter` for details on using geometry to filter PCG points.

OGR Geometry Settings
---------------------

.. include:: params/OGRGeometry.inc
