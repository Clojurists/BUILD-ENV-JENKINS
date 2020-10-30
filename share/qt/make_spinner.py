#!/usr/bin/env python
# W.J. van der Laan, 2011
# Make spinning .mng animation from a .png
# Requires imagemagick 6.7+
from __future__ import division
from os import path
from PIL import Image
from subprocess import Popen

SRC='img/reload_scaled.png'
DST='../../src/qt/res/movies/update_spinner.mng'
TMPDIR='/tmp'
TMPNAME='tmp-%03i.png'
NUMFRAMES=35
FRAMERATE=10.0
CONVERT=