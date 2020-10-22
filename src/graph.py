from __future__ import print_function

from collections import namedtuple, defaultdict
import csv
import matplotlib.pyplot as plt
import sys


inputfile = sys.argv[1] if len(sys.argv) == 2 else 'out.txt'

Hit = namedtuple('Hit', ['slot', 'addr', 'time']) # 定义一个namedtuple类型Hit，并包含slot，addr和time属性

CUTOFF = 2000
square = {'label': 'Square', 'marker': 'x', 'color': '#FF0059', 'addrs': [0]} #字典 [0] 一个元素 值为0
red = {'label': 'Reduce', 'marker': '.', 'color': '#2C00E8', 'addrs': [1]}
#mult = {'label': 'Multiply', 'marker': '^', 'color': '#00F1FF', 'addrs': [2]}
hit_types = [square, red]  #列表

with open(inputfile, 'rt') as outfile:
    probereader = csv.reader(outfile, delimiter=' ') #delimiter 断点表示 空格
    rows = [Hit(slot=int(row[0]), addr=int(row[1]), time=int(row[2]))
            for row in probereader]  #列表推导式 创建列表 for example squares = [x**2 for x in range(10)]
    hits = [row for row in rows if row.time < CUTOFF] #CUTOFF 不能太小
    seen = {} #{} 创建字典 set()创建空集合
    for hit in hits:
        if hit.slot not in seen:
            seen[hit.slot] = 0.0
        seen[hit.slot] += 1.0    #因为我是两个地址，所以值为2
    print("length of seen ",len(seen))
    print(sum(seen.values()) / len(seen))  #字典的所有值 seen.values()
    
    print("Slots >= 2 {:d}".format(len([s for s in seen.keys() if seen[s] >= 2])))
    print("Slots == 0 {:d}".format(len([s + 1 for s in seen.keys() if (s + 1) not in seen])))
    print("Slots, tot {:d}".format(len(seen)))

    addr_counts = defaultdict(int) #作用是当key不存在时，返回的是工厂函数的默认值 int对应0  dict1[1]=0 where 1 done's exist
    for hit in hits:
        addr_counts[hit.addr + 1] += 1
    print("Counts for each address:", addr_counts) #计算各个地址 被观察到几次

    multiplies = [hit for hit in hits if hit.addr in red['addrs']]
    slots_to_multiplies = {}
    for multiply in multiplies:
        slots_to_multiplies[multiply.slot] = multiply  #hits have three arttibutes #建立slot : hit 的字典

    multiply_slots = sorted(slots_to_multiplies.keys()) #按升序排列 返回的是一个新的 list
    dists = {}
    for i in range(0, len(multiply_slots) - 1):
        dist = multiply_slots[i + 1] - multiply_slots[i]
        if dist not in dists:
            dists[dist] = 0
        dists[dist] += 1
    print("Counts for each distance between subsequent red (in slots)", dists)  #计算slot相差值，有可能slot没有观察到

    for hit_type in hit_types: #addr is 0 or 1
        hits_of_type = [hit for hit in hits if hit.addr in hit_type['addrs']] #addr 0==0 , 1==1  分别获得地址为0,1的hit 
        slot_to_hits = {}
        for hit in hits_of_type:
            slot_to_hits[hit.slot] = hit #build slot to hit
        print("{:s} {:d}".format(hit_type['label'], len(slot_to_hits.keys())))

    #绘图了
    fig = plt.figure() #Create a new figure.
    axis = fig.add_subplot(111)
    plots = []
    # Somewhat of a hack right now, but we assume that there are 5 addresses
    # range instead of xrange for forwards compatibility
    # Also, have to do each address separately because scatter() does not accept
    # a list of markers >_>
    for hit_type in hit_types:
        addr_hits = [hit for hit in hits if hit.addr in hit_type['addrs']]
        slot_to_hits = {}
        for hit in addr_hits:
            slot_to_hits[hit.slot] = hit
        addr_hits = slot_to_hits.values()
        plot = axis.scatter(
            [hit.slot for hit in addr_hits],  #row
            [hit.time for hit in addr_hits],  #column
            c=hit_type['color'],
            marker=hit_type['marker'],
        )
        plots.append(plot)
    # plt.plot([hit.slot for hit in hits], [hit.time for hit in hits])
    plt.legend(tuple(plots),
               tuple([hit_type['label'] for hit_type in hit_types]),
               scatterpoints=1,
               loc='upper right',
               ncol=1)
    plt.show()
