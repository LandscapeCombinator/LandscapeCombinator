.. _buildings-from-splines:

Buildings From Splines
======================

The ``BuildingsFromSplines`` actor generates 3D buildings from spline components
in your level. It's commonly used together with the :ref:`SplineImporter <splines>`
to automatically create buildings on road networks.

To use it, drag a ``BuildingsFromSplines`` actor into your level, select the spline
components to use (or leave it set to all splines), configure the building parameters,
and click ``Generate Buildings``.

For details on building configuration (levels, wall segments, material expressions, etc.),
see the :ref:`Building section <building>`.


Buildings From Splines Settings
---------------------------------

.. include:: params/BuildingsFromSplines.inc


Weighted Building Configuration Settings
------------------------------------------

.. include:: params/WeightedBuildingConfiguration.inc
