﻿.. _landscape-spawner:

Create Landscapes with LandscapeSpawner
=======================================

LandscapeSpawner Overview
-------------------------

#. Search for ``LandscapeSpawner`` in the Content Browser, and drag one in your landscape.
   You can have several ``LandscapeSpawner``'s in your level.

#. Choose the heightmap source that you want in the :ref:`Heightmap Downloader <image-downloader>` component
   (the ``LandscapeCombinatorMap`` in the Plugin's Content contains several ``LandscapeSpawner`` examples).

#. In the Details Panel of the ``LandscapeSpawner``, you can check the "Create Mapbox Decals" or
   "Create Custom Decals" options if you also want to create decals. "Create Custom Decals" uses
   the Image Downloader settings from the :ref:`Texture Downloader <image-downloader>`
   component.

#. If you prefer to keep the creation of decals separated, you can use
   :ref:`Landscape Texturer actors <landscape-texturer>` after (or before) your
   landscape has been created.

#. In the Details Panel of the ``LandscapeSpawner``, click on ``Spawn Landscape``.



LandscapeSpawner Settings
-------------------------

* **ComponentsMethod**:
  Select the method used to compute the components count when creating the
  landscape. This can either be ``Manual``, or using a recommended landscape size
  from Unreal Engine. You can also choose ``Auto``, which uses the same
  method as in Landscape Mode.

  If your heightmap data does not match the landscape components, you will get
  flat data at the border. This can be prevented the ``Auto Without Border``
  option, but note that this will also remove some data at the border of your
  heightmaps to make them match the components.

* **Create Landscape Streaming Proxies (bool)**:
  If you are using World Partition, check this option if you want to create landscape streaming proxies.
  This is useful if you have a large landscape, but it might slow things down for small landscapes.

* **ZScale (double)**:
  ``ZScale = 1`` means that one altitude unit (usually meters) in your heightmaps corresponds to 100 Unreal units (cm by default) in Unreal Engine in the Z-axis.
