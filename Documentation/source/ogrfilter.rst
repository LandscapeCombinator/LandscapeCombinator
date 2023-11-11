Use OpenStreetMap Areas in PCG
==============================

There are (at least) two kinds of foliage that you may add. Foliage that you want almost everywhere, such as grass or rocks,
can be added using the Landscape Grass Types defined in the landscape material ``MI_Landscape``.

Other foliage such as trees can be spawned only in some areas (using real-world data from `OSM <https://www.openstreetmap.org>`_
or any kind of vector file supported by `GDAL <https://gdal.org/>`_). This is done thanks to a new PCG node called ``OGRFilter``
that filters out PCG points based on real-world geometries of forests or other areas.

You may also try to download landuse images using ``LandscapeTexturer`` for your georeference landscape, and then use these
images to paint a specific layer where you want foliage.
