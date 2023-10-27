# Landscape Combinator

![Landscape Picture of Entrevaux Village](Gallery/entrevaux.png?raw=true "Landscape Picture of Entrevaux Village")
![Landscape Picture with Road](Gallery/picture.png?raw=true "Landscape Picture with Road")
![Landscape Picture with Forests](Gallery/picture2.png?raw=true "Landscape Picture with Forests")
![Plugin Use Example](Gallery/capture.png?raw=true "Plugin Use Example")

## Description

Landscape Combinator is an Unreal Engine 5.3 plugin that enables you to create landscapes from
real-world data in a few simple steps:

* Create landscapes from real-world heightmaps (with a basic landscape material to visualize them),
* Create landscape splines from [OSM](https://www.openstreetmap.org) data for roads, rivers, etc.
* Automatically generate buildings using splines representing buildings outlines in [OSM](https://www.openstreetmap.org),
* Add procedural foliage (such as forests) based on [OSM](https://www.openstreetmap.org) data.

When creating a landscape for a real-world location, the plugin will adjust its position and scale
correctly in a planar world. You can thus create several real-world landscapes of various quality and
have them placed and scaled correctly with respect to one another. In the places where the landscapes
overlap, you can use a button to automatically blend landscape data.

For more information, please refer to the [documentation](https://landscapecombinator.github.io/LandscapeCombinator/).

# Credit

## Heightmap Providers

For now we only support downloading heightmaps from the following sources. Please check their licenses
before using their heightmaps in your projects.

* [RGE ALTI® 1m France](https://geoservices.ign.fr/rgealti)

* [Elevation API IGN1m](https://elevationapi.com/) (this should be the same data as RGE ALTI® 1m)

* [swissALTI3D](https://www.swisstopo.admin.ch/en/geodata/height/alti3d.html)
	To use swissALTI3D, please go to their web page, and on the bottom, choose "Selection by Rectangle".
	Click on "New Rectangle", and select the rectangle area that you want. Choose the resolution
	that you want (0.5m is the highest precision, or 2m), and click on "Search".
	Then, click on "Export all links" and download the CSV file. In the Landscape Combinator
	plugin, you then provide the path to the CSV file on your computer.

* [Viewfinder Panoramas](http://viewfinderpanoramas.org/)


## Data for Landscape Splines and Procedural Foliage

If you use [OSM](https://www.openstreetmap.org) data or [Overpass API](https://overpass-api.de/) to get data for
splines or procedural foliage, please check their licenses.
