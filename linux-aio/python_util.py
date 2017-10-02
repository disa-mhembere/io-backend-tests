import numpy as np
from glob import glob
import os

files = glob("partfile*")

for fn in files:
    data = np.fromfile(fn, dtype=np.float, count=-1)
    print data
