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

csvpath = "full/WebSearch1.spc";
obj = pandas.read_csv(csvpath,header=None, names=['diskno', 'addr','size','type','time']);
print(obj.values[1,:])
obj = obj[obj['type']=='R']
for i in range(0,100):
    addrs = obj.values[i*4000:(i+2)*4000,1]
    print(addrs.min())
    print(addrs.max())

    addrs = addrs % 67108864;
    addrs = addrs / 4;
    print(addrs[0:100])

    cnt, bins = np.histogram(addrs[0:4000],int(addrs.max()),range=(0,addrs.max()));
    cnt1, bins1 = np.histogram(addrs[4000:7999],int(addrs.max()),range=(0,addrs.max()));

    print(np.sum(cnt))
    print(np.sum(cnt1))
    #print(obj)
    plt.style.use('ggplot')
    fig1, ax1 = plt.subplots(2, 1, sharex=True)
    print(ax1)
    ax1[0].plot(bins[:-1], cnt)
    ax1[1].plot(bins1[:-1], cnt1)
    ax1[0].set_xlabel("Address")
    ax1[0].set_ylabel("Access time")
    ax1[1].set_xlabel("Address")
    ax1[1].set_ylabel("Access time")
    # ax1.set_xlim(0, 10e4)
    fig1.savefig('/media/DataDisk/development/SSDSimGit/figures/traces4000.eps')
    plt.show()