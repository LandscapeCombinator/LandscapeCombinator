# Landscape Combinator

![Landscape Picture of Entrevaux Village](Gallery/entrevaux.png?raw=true "Landscape Picture of Entrevaux Village")
![Landscape Picture with Road](Gallery/picture.png?raw=true "Landscape Picture with Road")
![Landscape Picture with Forests](Gallery/picture2.png?raw=true "Landscape Picture with Forests")

## Description

Landscape Combinator is an [Unreal Engine 5.3 plugin](https://www.unrealengine.com/marketplace/en-US/product/landscape-combinator)
that lets you create landscapes from real-world data in a few simple steps:

* Create landscapes from real-world heightmaps (with a basic landscape material to visualize them),
* Create landscape splines from [OSM](https://www.openstreetmap.org) data for roads, rivers, etc.
* Automatically generate buildings using splines representing buildings outlines in [OSM](https://www.openstreetmap.org),
* Add procedural foliage (such as forests) based on [OSM](https://www.openstreetmap.org) data.

When creating a landscape for a real-world location, the plugin will adjust its position and scale
correctly in a planar world. You can thus create several real-world landscapes of various resolution and
have them placed and scaled correctly with respect to one another. The plugin also has a blending feature
to modify the landscapes heightmap data to make the transition between different landscapes as
seamless as possible.

For more information, please refer to the [documentation](https://landscapecombinator.github.io/LandscapeCombinator/).

# Credit

Thanks to all these wonderful websites without which this plugin could not have been made!
Please check each website license when using their data.

Heightmap and Imagery Providers
-------------------------------

* [RGE ALTI® 1m France](https://geoservices.ign.fr/rgealti)

* [Elevation API IGN1m](https://elevationapi.com/)

* [swissALTI3D (©swisstopo)](https://www.swisstopo.admin.ch/en/geodata/height/alti3d.html)

* [Viewfinder Panoramas](http://viewfinderpanoramas.org/)

* [Litto 3D Guadeloupe](https://diffusion.shom.fr/litto3d-guad2016.html)

* All the WMS servers such as IGN, SHOM, USGS, etc.

* [Mapbox](https://www.mapbox.com/)


Data for Splines and Procedural Foliage
---------------------------------------

* [OSM](https://www.openstreetmap.org)

* [Overpass API](https://overpass-api.de/)


GDAL
----

* [GDAL](https://gdal.org/) for all heightmaps transformations.


EPSG
----

* [EPSG.io](https://epsg.io/map#srs=4326) to obtain coordinates on a map.
