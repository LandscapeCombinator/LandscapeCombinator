# Landscape Combinator


## Description

Landscape Combinator is a simple Unreal Engine 5.1 plugin that has three main features:

* downloading real-world heightmaps from various sources,
* converting heightmaps to the format expected by Unreal Engine using [GDAL](https://gdal.org/),
* adjusting the position and scale of a landscape to place it at the right position in a planar world.

You can thus create several real-world landscapes of various quality and have them placed and scaled
correctly with respect to one another.


Landscape Combinator does not provide a landscape material, nor does it make the landscapes superimpose
seamlessly. The fact that you have multiple landscapes will be visible at the borders, unless you
manually adjust by sculpting or other methods.


## Installation

You can use the sources directly from here for personal projects.
Copy the content of this repository to a folder called "LanscapeCombinator" inside
the Plugins folder of your project.

For commercial projects, please check the Unreal Engine Marketplace (plugin to be published soon).

## Usage


TLDR: Add heightmap line. Download. Convert. Import landscape and rename it. Adjust.

* Install [7-Zip](https://www.7-zip.org/download.html) and make sure it is available in your PATH environment variable (used for Viewfinder Panoramas).
* After loading the plugin, open the plugin window using the plugin button or in the "Tools" menu.
* Some heightmaps lines examples are already there for you to try, or you can add a new heightmap line
  using the heightmap provider of your choosing. Heightmap lines are saved automatically in the HeightMapList file in the Saved directory of your project.
* Press the download button of the heightmap line you want, this should download the heightmaps to:
  ```
  C:\Path\To\Engine\Intermediate\LandscapeCombinator\Download
  ```
  Make sure there are no errors in the Output Log (in the LogLandscapeCombinator category).
* If the heightmaps are correctly downloaded, you can then press the Convert button, which will reduce the size (width and height pixels)
  of the heightmaps based on the "Precision" percentage, and will convert the heightmaps to PNG format in a subfolder of:
  ```
  C:\Path\To\Engine\Intermediate\LandscapeCombinator\
  ```
  Again, make sure there are no errors in the Output Log.
* Open a level with World Partition enabled or create a new one (File > New Level > Open World or Empty Open World).
* Make sure World Partition is enabled in your settings.
* Disable world bound checks if you have a large landscape (or a landscape far from the origin) so that your actors don't get destroyed.
* You can then go to Landscape mode to import the converted heightmaps for a landscape.
* Rename the lanscape you have just imported to match the name given in the heightmap line of the plugin.
* In the plugin window, choose the size you would like for the whole planet, for instance 40000km/20000km.
  If your landscape doesn't cover the whole world, then it will be proportionally smaller.
* Press the Adjust button to adjust the scale and position of your landscape.
* Heightmap Lines are saved automatically in the HeightMapList in the plugin directory.
* If you want to find the location of a real-world place in your landscape inside the editor, find the [EPSG 4236 coordinates (Long, Lat)](https://epsg.io/map#srs=4326) (I found the site works better on Chrome than on Firefox)
  and convert them to Unreal Engine coordinates as follows:
```C
x = Long * WorldWith (in km) * 1000 * 100 / 360
y = - Lat * WorldHeight (in km) * 1000 * 100 / 180 // note the negative sign here
```


## Heightmap Providers

For now we only support downloading heightmaps from the following sources. Please check their licenses
before using their heightmaps in your projects. Feel free to open an issue to request support for new
sources.

* [RGE ALTI® 1m France](https://geoservices.ign.fr/rgealti)
* [Elevation API IGN1m](https://elevationapi.com/) (this should be the same data as RGE ALTI® 1m)
* [Viewfinder Panoramas](http://viewfinderpanoramas.org/)
