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

csvpath = "full/Financial1.spc";
obj = pandas.read_csv(csvpath,header=None, names=['diskno', 'addr','size','type','time']);
print(obj.values[1,:])
obj = obj[obj['type']=='w']
for i in range(0,100):
    addrs = obj.values[i*4000:(i+2)*4000,1]
    print(addrs.min())
    print(addrs.max())

    print(addrs[0:100])

    cnt, bins = np.histogram(addrs[0:4000],int(addrs.max()/200),range=(0,1000000));
    cnt1, bins1 = np.histogram(addrs[4000:7999],int(addrs.max()/200),range=(0,1000000));

    print(np.sum(cnt))
    print(np.sum(cnt1))
    #print(obj)
    #plt.style.use('ggplot')
    fig1 = plt.figure(1)
    #canvas = FigureCanvas(fig1)

    #ax1 = plt.subplot()
    ax1 = fig1.subplots(2, 1, sharex=True)
    ax1[0].plot(bins[:-1], cnt)
    ax1[1].plot(bins1[:-1], cnt1)
    # ax1.hist(addrs, bins=int(addrs.max()/2000), normed=0, facecolor="blue", edgecolor="black", alpha=0.7)
    # ax1.set_xlim(0, 10e4)

    # ax2 = fig1.add_axes([0.2, 0.6, 0.2, 0.2])
    # ax2.plot(obj.values[:,0], obj.values[:,1], 'r')
    # pl1 = plt.plot(obj.values[:,0], obj.values[:,1], 'r')
    # plt.xlim(0, 10e4)
    # plt.show()
    plt.show()