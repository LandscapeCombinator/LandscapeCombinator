.. _splines:

Importing Splines and Landscape Splines
=======================================

SplineImporter Overview
-----------------------

* First make sure that you have a :ref:`Level Coordinates <coordinates>` actor in your level.
* Search for ``SplineImporter``, which is a C++ class in the Landscape Combinator plugin, and drag it into your level.
* In the Details Panel, select the splines source as you wish.
  You can either use a premade query, such as Roads, Buildings, or you can write the query yourself using `Overpass <https://overpass-turbo.eu/>`_ syntax.
  For instance, the premade query for Roads is the same as using "Overpass Short Query" with the query:
  ``way["highway"]["highway"!~"path"]["highway"!~"track"];``.
* Select whether you want landscape splines, or regular spline components.
* Choose an actor on which to spawn the splines (usually a landscape).
  This actor's area in Unreal will be mapped to a real-world area thanks to the ``Level Coordinates``,
  and the plugin will use this real-world area to search for splines.
* Press the ``Generate Splines`` button.

Landscape Splines
-----------------

If you have generated landscape splines, you can go to
``Landscape Mode > Manage > Splines`` to control the spline parameters as usual.
You can choose to paint using a layer (fill the ``Layer Name`` field in the details panel after selecting all control points and then all segments).
If you use the ``MI_Landscape`` material for your landscape, you can try ``Road`` as a ``Layer Name`` to paint roads.

You can also select all the segments, and then add a mesh on the landscape splines in the Details Panel.

Buildings
---------

For buildings, it is recommended that you use regular splines (not landscape splines).
When you generate regular splines, they are added as Spline components in a new ``SplineCollection`` actor.
Use the ``Toggle Linear`` button to switch between curvy and straight splines.

Then, drag a ``BuildingsFromSplines`` actor in your level.
Select the parameters you want for your buildings (windows and doors meshes, materials, number of floors, etc.).
You may also enter tags to filter the spline components from which buildings are generated.
Keep the tags set to `None` if you want to generate buildings on all the spline components
existing in your level. Once you are satisfied with your settings, press the ``Generate Buildings`` button.