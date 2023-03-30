#!/usr/bin/env python3

import numpy as np

x = np.linspace(0, 0.1, 10)
y = np.ones(10)

np.savetxt("out.txt", np.column_stack([x,y]))
