.. _landscape-spawner:

Landscape Spawner
==================

Landscape Spawner Overview
---------------------------

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


Landscape Spawner Settings
---------------------------

.. include:: params/LandscapeSpawner.inc


Position Based Generation Reference
-----------------------------------

.. include:: params/LCPositionBasedGeneration.inc


General Settings
----------------

.. include:: params/LCSettings.inc


Concurrency Settings
--------------------

.. include:: params/ConcurrencySettings.inc
