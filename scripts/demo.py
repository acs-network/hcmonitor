import sys,os, time
sys.path.append('/usr/local/python3/lib/python3.6/site-packages')
import matplotlib
from PIL import Image
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.font_manager import FontProperties

font_title = FontProperties(fname=r"/usr/share/fonts/newfontsdir/msyh.ttc",size=20)
font_axis = FontProperties(fname=r"/usr/share/fonts/newfontsdir/msyh.ttc",size=14)
interval_time = int(sys.argv[1])
arch_img = Image.open('30-30pp.png')
t0 = time.time()
conns = []
delay_99 = []
time_points = []
fig = plt.figure('QStack')
fig.suptitle("青云亿级并发演示系统",fontproperties=font_title)
delay_lines = 260


def obtain_data(i):

    print(str(time.time()-t0)+'s, start updating graph')

    #架构图
    ax = plt.subplot(2,2,1)
    plt.title("大规模并发系统架构图",fontproperties=font_axis)
    plt.imshow(arch_img)
    plt.axis("off")

    #活跃连接数图
    with open('conn_total','r') as conn_file:
        total_conn = float(conn_file.readlines()[0])/100000000
    conns.append(total_conn)
    t = time.time() - t0
    time_points.append(t)
    ax1 = plt.subplot(2,2,2)
    for (key,spine) in ax1.spines.items():
        if key == 'right' or key == 'top':
            spine.set_visible(False)
    plt.title("活跃连接数",fontproperties=font_axis)
    plt.xlabel('时间(秒)',fontproperties=font_axis)
    plt.ylabel('连接数(亿)',fontproperties=font_axis)
    line = ax1.plot(time_points, conns, '-g', marker='*', c='red')[0]
    line.set_xdata(time_points)
    line.set_ydata(conns)
    ax1.set_ylim([3.0, 3.1])
    ax1.set_xlim([t - 460, t + 60])
    plt.grid(ls='--')
    plt.tight_layout()
    print("Connections OK")

    #cdf图
    with open('sketch','r') as cdf_file:
        lines = cdf_file.readlines()
    while len(lines) < delay_lines :
        print(str(time.time()-t0)+"s, "+str(len(lines))+' delays, NOT ready')
        time.sleep(1)
        with open('sketch', 'r') as cdf_file:
            lines = cdf_file.readlines()
    print(str(time.time()-t0)+'s, delays ready')
    yaxis_accumulation = []
    yaxis = []
    xaxis_delay = []
    accumulator = 0
    for i in lines:
        item = i.replace('\n', '')
        accumulator += int(item)
        yaxis_accumulation.append(accumulator)
    for i in range(0,len(yaxis_accumulation)):
        temp = 100.0*yaxis_accumulation[i]/accumulator
        if temp < 99:
            xaxis_delay.append(i*0.1)
            yaxis.append(temp)
        else:
            break
    ax2 = plt.subplot(2,2,4)
    #plt.cla()
    ax2.spines['top'].set_visible(False)
    ax2.spines['right'].set_visible(False)
    plt.ylabel("百分比",fontproperties=font_axis)
    plt.xlabel("延迟(毫秒)",fontproperties=font_axis)
    plt.title("一分钟延迟累积分布",fontproperties=font_axis)
    plt.grid(ls='--')
    delay_99_percentile = xaxis_delay[len(xaxis_delay)-1]
    #plt.axvline(x=delay_99_percentile,lw=2,ls=':',c='blue',label='ssss')
    #plt.plot([i*0.1 for i in range(1,delay_lines+1)], yaxis, "r")
    plt.plot(xaxis_delay,yaxis,'r',lw=0.3)
    #plt.bar([i*0.1 for i in range(1,delay_lines+1)],yaxis_accumulation, width=1)
    plt.tight_layout()
    print("CDF graph OK")

    #99分位延迟图
    delay_99.append(delay_99_percentile)
    ax3 = plt.subplot(2,2,3)
    for (key,spine) in ax3.spines.items():
        if key == 'right' or key == 'top':
            spine.set_visible(False)
    plt.xlabel('时间(秒)',fontproperties=font_axis)
    plt.ylabel('99分位延迟(毫秒)',fontproperties=font_axis)
    plt.title('99分位延迟变化',fontproperties=font_axis)
    line = ax3.plot(time_points, delay_99, '-g', marker='*', c='red')[0]
    line.set_xdata(time_points)
    line.set_ydata(delay_99)
    ax3.set_xlim([t - 460, t + 60])
    ax3.set_ylim([0,4])
    plt.grid(ls='--')
    plt.tight_layout()
    print("99 percentile graph OK")

ani = FuncAnimation(plt.gcf(), obtain_data, interval=interval_time * 1000)
plt.tight_layout()
plt.show()
