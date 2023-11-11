.. _landscape-spawner:

Create Landscapes with LandscapeSpawner
=======================================

LandscapeSpawner Overview
-------------------------

#. Search for ``LandscapeSpawner`` in the Content Browser, and drag one in your landscape.

#. Choose the heightmap source that you want in the :ref:`Image Downloader <image-downloader>` component section
   (the ``LandscapeCombinatorMap`` in the Plugin's Content contains several ``LandscapeSpawner`` examples).

#. In the Details Panel of the ``LandscapeSpawner``, click on ``Spawn Landscape``.



Settings
--------

* **Drop Data (bool)**:
  When the landscape components do not exactly match with the total size of the heightmaps,
  Unreal Engine adds some padding at the border. Check this option if you are willing to
  drop some data at the border in order to make your heightmaps match the landscape
  components.

* **Create Landscape Streaming Proxies (bool)**:
  If you are using World Partition, check this option if you want to create landscape streaming proxies.
  This is useful if you have a large landscape, but it might slow things down for small landscapes.

* **ZScale (double)**:
  ``ZScale = 1`` means that one altitude unit (usually meters) in your heightmaps corresponds to 100 cm in Unreal Engine in the Z-axis.
