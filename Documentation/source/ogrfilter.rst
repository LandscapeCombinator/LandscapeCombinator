OGR Filter
==========

.. _ogrfilter:

Use OpenStreetMap Areas in PCG
------------------------------

Foliage such as trees can be spawned only in some areas (using real-world data from `OSM <https://www.openstreetmap.org>`_
or any kind of vector file supported by `GDAL <https://gdal.org/>`_). This is done thanks to a new PCG node called ``OGRFilter``
that filters out PCG points based on real-world geometries of forests or other areas (see :ref:`OGR Geometry <ogr-geometry>`).


OGR Filter Settings
-------------------

.. include:: params/PCGOGRFilterSettings.inc
