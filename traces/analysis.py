#!python3

import matplotlib.pyplot as plt
import pandas
import os
import numpy as np
from matplotlib.figure import Figure
#from matplotlib.backends.backend_agg import FigureCanvasAgg as FigureCanvas

# a = [3.0,4.0,5.0,6.0];
# a = (a[0:-1] + a[1:])/2
# print(a)

csvpath = "full/WebSearch3.spc";
obj = pandas.read_csv(csvpath,header=None, names=['diskno', 'addr','size','type','time']);
print(obj.values)
obj = obj[((obj['type']=='r') | (obj['type']=='R'))]
addrs=obj.values[:,1]
print(len(addrs))
# for i in range(0,100):
#     addrs = obj.values[i*4000:(i+2)*4000,1]
#     print(addrs.min())
#     print(addrs.max())

#     addrs = addrs % 67108864;
#     addrs = addrs / 4;
#     print(addrs[0:100])

#     cnt, bins = np.histogram(addrs[0:4000],int(addrs.max()),range=(0,addrs.max()));
#     cnt1, bins1 = np.histogram(addrs[4000:7999],int(addrs.max()),range=(0,addrs.max()));

#     print(np.sum(cnt))
#     print(np.sum(cnt1))
    #print(obj)
plt.style.use('ggplot')
fig1,(ax1) = plt.subplots(1, 1, sharex=True)
#    ax1[0].plot(bins[:-1], cnt)
#    ax1[1].plot(bins1[:-1], cnt1)
cnt, bins = np.histogram(addrs,int(addrs.max()/20),range=(0,addrs.max()));
ax1.plot(bins[:-1], cnt);
#ax1.plot(addrs, bins=int(addrs.max()/2000), normed=0, facecolor="blue", edgecolor="black", alpha=0.7)

print(ax1)

ax1.set_xlim(0, 10e6)
ax1.set_ylim(0, cnt.max()*1.1)
ax1.set_xlabel("Address")
ax1.set_ylabel("Access time")
fig1.savefig('/media/DataDisk/development/SSDSimGit/figures/traces.eps')

plt.show()