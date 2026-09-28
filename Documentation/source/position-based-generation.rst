.. _position-based-generation:

Position Based Generation
==========================

Position Based Generation is an optional component that fixes the boundaries of a Generator.
It calculates the tile boundaries based on the current position of the editor camera or the player.
It then calls the underlying Generator to create content at those boundaries.
It can be used in editor or at runtime.
You need a :ref:`LevelCoordinates <coordinates>` actor in your level for this to work.

Position Based Generation Settings
----------------------------------

.. include:: params/LCPositionBasedGeneration.inc
