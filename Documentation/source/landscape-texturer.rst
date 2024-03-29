﻿.. _landscape-texturer:

Download Images for Landscapes with Landscape Texturer
======================================================

LandscapeTexturer Overview
--------------------------

#. Search for ``LandscapeTexturer`` in the Content Browser, and drag one in your landscape.

#. Choose the image source that you want in the :ref:`Texturer Downloader <image-downloader>` component
   (the ``LandscapeCombinatorMap`` in the Plugin's Content contains several ``LandscapeTexturer`` examples).

#. (Optional) In the ``ImageDownloader`` component, you can use the ``Adapt Resolution``
   so that the image gets resized to the same resolution as a given Landscape.
   This does not work at the moment for sources that come as tiles, such as Mapbox Satellite. 

#. (Optional) You can use the ``Crop Coordinates`` option to crop the images
   to a given actor. This is typically not needed for WMS or XYZ sources where
   you can choose the coordinates upfront.

#. In the Details Panel of the ``LandscapeTexturer``, click on ``Create Decal``.
