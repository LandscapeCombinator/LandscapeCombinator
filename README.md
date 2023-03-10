# Landscape Combinator

![Plugin Use Example](capture.png?raw=true "Plugin Use Example")
![Landscape Picture with Road](picture.png?raw=true "Landscape Picture with Road")
![Landscape Picture with Forests](picture2.png?raw=true "Landscape Picture with Forests")

## Description

Landscape Combinator is an Unreal Engine 5.1 plugin that enables you to create landscapes from
real-world data in a few simple steps:

* Create landscapes from real-world heightmaps (with a basic landscape material to visualize them),
* Create landscape splines from [OSM](https://www.openstreetmap.org) data.
* Add procedural foliage (such as forests) based on [OSM](https://www.openstreetmap.org) data (or other vector files).

When creating a landscape for a real-world location, the plugin will adjust its position and scale
correctly in a planar world. You can thus create several real-world landscapes of various quality and
have them placed and scaled correctly with respect to one another. In that case, the fact that
you have have multiple landscapes will be visible at the borders, unless you adjust the borders
manually or hide them with static meshes (such as forests).


## Installation

You can use the plugin sources directly from here for personal projects.
Copy the content of this repository to a folder called `LanscapeCombinator` inside
the Plugins folder of your project, and then click `Yes` when Unreal Engine asks
whether you want to compile the module.

If you plan to use [Viewfinder Panoramas](http://viewfinderpanoramas.org/) heightmaps, please install
[7-Zip](https://www.7-zip.org/download.html) and make sure it is available in your PATH environment variable.

For commercial projects, please check the Unreal Engine Marketplace (plugin to be published soon).


## Creating Landscapes

* After loading the plugin, open the plugin window using the plugin button or in the `Tools` menu.
* Some heightmaps lines examples are already there for you to try, or you can add a new heightmap line
  using the heightmap provider of your choosing. Heightmap lines are saved automatically in the HeightMapListV1 file in the Saved directory of your project.
* Press the `Create Landscape` button for the landscape(s) of your chosing.
  Make sure there are no errors in the Output Log (in the LogLandscapeCombinator category).
  You should see your landscape appear in your editor.
* Go to the `Landscape Mode` of Unreal Engine in order to paint the landscape. You can use the basic landscape material
  `MI_Landscape` provided by the plugin, which as an auto layer called `Mix` (with adjustable snow, grass, and cliff materials).


### Technical Notes

The plugin places landscapes in a planar world using EPSG 4326 coordinates.
If your original data does not use EPSG 4326, you can choose to reproject it to EPSG 4326 (with a checkbox).
The reprojection is done thanks to [GDAL](https://gdal.org/) and can introduce some artefacts such as staircases in your landscape.
If someone knows how to avoid this, please tell me!

If you choose not to reproject, the landscapes will be nicer, but you will loose coordinate precision within your landscape
if you choose to add landscape splines or procedural foliage from OSM data (see next sections) which use EPSG 4326 coordinates.

Finally, if you want to find the location of a real-world place in your landscape inside the editor,
find the [EPSG 4326 coordinates (Long, Lat)](https://epsg.io/map#srs=4326) (I found the site works better on Chrome than on Firefox)
  and convert them to Unreal Engine coordinates as follows:
```C
x = (Long - WorldOriginX) * WorldWith (in km) * 1000 * 100 / 360
y = (WorldOriginY - Lat) * WorldHeight (in km) * 1000 * 100 / 180 // note the signs are opposite from the line above
```


## Creating Landscape Splines

* You can add landscape splines from OSM data on the landscapes you created.
* In the plugin tab, choose a target landscape and a source for your OSM data, and click the `Add` button, and then `Create Roads` button.
* After the data is downloaded, you will be asked what percent of nodes you want to keep in your splines.
  If you have thousands of nodes, creating splines will be slow.
* After the landscape splines are created, you should go to `Landscape Mode > Manage > Splines` to control the spline parameters as usual.
* You can choose to paint a layer (fill the `Layer Name` field in the details panel after selecting all control points and then all segments).
  If you use the `MI_Landscape` material for your landscape, you can try `Road` as a `Layer Name` to paint roads.
* You can also select all segments to add meshes for roads (or something else).


## Procedural Foliage

There are (at least) two kinds of foliage that you may add. Foliage that you want almost everywhere, such as grass or rocks,
can be added using the Landscape Grass Types defined in the landscape material `MI_Landscape`. Other foliage such as trees
can be spawned only in some areas (using real-world data or any kind of vector file supported by [GDAL](https://gdal.org/).

* Press the `Add` button in the Unreal Engine toolbar, and from `Volumes`, drop an `OGRProceduralFoliageVolume` on your level.
* Scale the volume where you want foliage to spawn (if the volume is too large, spawning will be slow).
* Add a `Procedural Foliage Spawner` to your volume (and add static mesh foliages to your newly created PFS).
* In the `Procedural Foliage` section of your volume's Details panel, there is the usual `Resimulate` button that spawns foliage everywhere in the volume.
  Make sure to click the `Load Unloaded Areas` if you see that the `Resimulate` button is greyed out. 
* The plugin adds a section `OGR Procedural Foliage` and a button `Resimulate With Filter` to spawn foliage only where there are forests in the real world (data from OpenStreetMap).
  There, you can also choose to filter based on any kind of area defined in an OpenStreetMap local file (or more generally, a vector file), or given by an Overpass Query.

## Future Work

* Add buildings from OSM data.

Feel free to open an issue if there is a feature you would like see.

## Heightmap Providers

For now we only support downloading heightmaps from the following sources. Please check their licenses
before using their heightmaps in your projects.

* [RGE ALTI?? 1m France](https://geoservices.ign.fr/rgealti)
* [Elevation API IGN1m](https://elevationapi.com/) (this should be the same data as RGE ALTI?? 1m)
* [Viewfinder Panoramas](http://viewfinderpanoramas.org/)


## Data for Landscape Splines and Procedural Foliage

If you use [OSM](https://www.openstreetmap.org) data or [Overpass API](https://overpass-api.de/) to get data for
landscape splines or procedural foliage, please check their licenses.
