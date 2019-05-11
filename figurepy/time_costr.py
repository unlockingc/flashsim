#!python3

import matplotlib.pyplot as plt
import pandas
import os
from matplotlib.figure import Figure
#from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas

csvpath = "./testr.csv";
obj = pandas.read_csv(csvpath);



#print(obj)
plt.style.use('ggplot')
fig1 = plt.figure(1)
#canvas = FigureCanvas(fig1)

#ax1 = plt.subplot()
ax1 = fig1.subplots()
ax1.plot(obj.values[:,0], obj.values[:,1], 'b')
ax1.set_xlim(0, 10e4)

# ax2 = fig1.add_axes([0.2, 0.6, 0.2, 0.2])
# ax2.plot(obj.values[:,0], obj.values[:,1], 'r')
# pl1 = plt.plot(obj.values[:,0], obj.values[:,1], 'r')
# plt.xlim(0, 10e4)
# plt.show()
ax1.set_ylabel("Time Cost(s)")
ax1.set_xlabel("Struct Size")
# ax1.set_xlim(0, 10e4)
fig1.savefig('/media/DataDisk/development/SSDSimGit/figures/TimeCost_read.eps')
plt.show()