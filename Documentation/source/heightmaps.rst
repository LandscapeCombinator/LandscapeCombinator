.. _heightmaps:

Creating Landscapes
===================

LandscapeSpawner Overview
-------------------------

#. Search for ``LandscapeSpawner``. You will find ``LandscapeSpawner_Example`` classes, and the main ``LandscapeSpawner`` class.

#. Drag any of these examples in your level, or the main ``LandscapeSpawner`` class where you can choose heightmap sources manually.

#. Click the landscape spawner that you have imported, and click on ``Spawn Landscape`` in its details panel.


Miscellaneous Settings
----------------------

* **Drop Data (bool)**:
  When the landscape components do not exactly match with the total size of the heightmaps,
  Unreal Engine adds some padding at the border. Check this option if you are willing to
  drop some data at the border in order to make your heightmaps exactly match the landscape
  components.

* **ZScale (double)**:
  ``ZScale = 1`` means that one altitude unit (usually meters) in your heightmaps corresponds to 100 cm in Unreal Engine in the Z-axis.


Heightmap Sources
-----------------

Here are the heightmap sources that are supported by Landscape Combinator. Feel free to suggest new sources on:
`GitHub <https://github.com/LandscapeCombinator/LandscapeCombinator/issues>`_
and I will make my best to add them.

Viewfinder Panoramas
~~~~~~~~~~~~~~~~~~~~


Please :ref:`make sure<Installation>` that you have 7Z installed if you want to use Viewfinder Panoramas.

* `Viewfinder Panoramas 1" <http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm>`_:
  Highest resolution, around 30 meters per pixel.
  In a ``LandscapeSpawner``, choose "Viewfinder Panoramas 1" and enter the comma-separated list of rectangles (e.g. L31, L32).
* `Viewfinder Panoramas 3" <http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm>`_:
  Intermadiate resolution, around 90 meters per pixel.
  In a ``LandscapeSpawner``, choose "Viewfinder Panoramas 3" and enter the comma-separated list of rectangles (e.g. L31, L32).
* `Viewfinder Panoramas 15" <http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm>`_:
  Lowest resolution, around 450 meters per pixel.
  In a ``LandscapeSpawner``, choose "Viewfinder Panoramas 15" and enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H).


USGS 1/3"
~~~~~~~~~

This is 1/3" or around 10 meters per pixel data for the United States.

* Go to the `USGS National Map Data Download Application <https://apps.nationalmap.gov/downloader/>`_.
* In the Datasets tab, click "Elevation Products (3DEP)". Unselect everything except "1/3 arc-second DEM" and "Current" underneath.
* Zoom on the area for which you want to download heightmaps.
* Click on "Search Products".
* In the "Products" tab, you can visualize the tiles that will be downloaded.
* If you want all the tiles suggested here, click on the TXT button (not CSV) to download the list of links.
* If you want only some of the tiles, add the ones that you want to your Cart, and click the TXT button here to download the list of links.
* In the Details Panel of a ``LandscapeSpawner``, choose ``USGS One Third`` as a source, and enter the path to the TXT file on your computer.

RGE ALTI®
~~~~~~~~~

This is very high resolution data, 1 meter per pixel, for Metropolitan France.
Enter the ``MinLong (Left), MaxLong (Right), MinLat (Bottom), MaxLat (Top)`` `EPSG 2154 <https://epsg.io/map#srs=2154>`_ coordinates
of the area that you want to use. Optionally, and especially if the area that you are covering is too large, use the
``Resize RGE ALTI Using Web API`` option to resize the heightmap before downloading it. The largest heightmap size supported by
this API is 10000 x 10000.


swissALTI3D (©swisstopo)
~~~~~~~~~~~~~~~~~~~~~~~~

This is very high resolution data, 0.5 meter per pixel, for Switzerland. It uses EPSG 2056.

* Go to the `swisstopo website <https://www.swisstopo.admin.ch/en/geodata/height/alti3d.html>`_.
* In Section "swissALTI3D - Access to geodata", choose "Selection by rectangle".
* Zoom on the map to the area that you wish to download.
* Click on "New rectangle", and draw a rectangle in the area that you want to download.
* Choose the resolution, 0.5m per pixel is the best resolution but will lead to heavy heightmaps.
* Click on "Search".
* Click on "Export all links" and then on "File ready. Click here to download".
* In the Details Panel of a ``LandscapeSpawner``, choose ``Swiss ALTI 3D`` as a source, and enter the path to the CSV file on your computer.



Litto 3D Guadeloupe
~~~~~~~~~~~~~~~~~~~

Please :ref:`make sure<Installation>` that you have 7Z installed if you want to use Litto 3D Guadeloupe.
This is very high resolution data, 1 meter per pixel, for Guadeloupe. It uses EPSG 4559.

* Go to the `Litto 3D Guadeloupe website <https://diffusion.shom.fr/litto3d-guad2016.html>`_.
* Click on "Télécharger".
* Click on the areas that you want to download.
* Click on "Télécharger la sélection".
* Move all the 7z files that you have downloaded into a new folder.
* In the Details Panel of a ``LandscapeSpawner``, choose ``Litto 3D Guadeloupe`` as a source, and enter the path to the new folder on your computer.


Local File
~~~~~~~~~~

Enter the path to a georeferenced file on your computer.

Local Folder
~~~~~~~~~~~~

Enter the path to a folder containing files named following the ``_x0_y0`` convention.

URL
~~~

Enter an URL to a georeferenced heightmap.


Preprocessing
-------------

You can preprocess downloaded heightmaps using the following options.
(These options are also available in the ``HeightmapModifier`` component that is attached to created landscapes).

* **Preprocess (bool)**:
  Check this option if you want to run an external binary to prepare the heightmaps right after fetching them.

* **Command (FString)**:
  Enter the name of the binary, which should be in your ``PATH``, and which will be used on your heightmap.
  Your processing command must take exactly two arguments: the input file and the output file.


Resolution Scaling
------------------

* **Change Resolution (bool)**:
  Check this option if you want to scale your heightmap resolution up or down using GDAL.

* **Precision Percent (int)**:
  Depending on the sizes of your heightmaps, you can use a small value under ``100%`` to make
  importing the landscape faster. For heightmaps which are low resolution, you can use a value
  above ``100%`` in order to have a better landscape grid size within Unreal Engine to be able
  to paint or sculpt things. Upscaling will however not add details that were not there in the
  original heightmaps.


Landscape Material
------------------

There is a landscape material ``MI_Landscape`` in the plugin that has the following features:

* Au auto layer called ``Mix`` (with adjustable snow, grass, cliff, and road materials).
* Adjustable height above which there is snow instead of grass.
* The cliff material goes on slopes which are steeper than a certain threshold.
* You can tint the materials with custom colors, and adjust saturation and brightness.
