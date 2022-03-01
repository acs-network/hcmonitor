nodes=$1
cmd=$2
filename=$3

start_mo()
{
    id=0
    for line in `cat $filename`
    do  
        ip1=${line##*.}
        echo "Monitor BX'$ip1' start"
        ssh root@$line 'ulimit -c unlimited && cd /home/HCMonitor/ && sh start.sh' &
        id=$(($id+1))
        if [ $id -eq $nodes ]
        then
            break;
        fi  
    done
    #pdsh -w ssh:10.1.2.[27-29,47-49,67-69,87-89,107-108] "cd /home/HCMonitor;sh start.sh &"
}

stop_mo()
{
    pdsh -w ssh:10.1.2.[27-29,47-49,67-69,87-89,107-108] "killall -9 monitor &"
    pdsh -w ssh:10.1.2.[27-29,47-49,67-69,87-89,107-108] "cd /home/HCMonitor;rm cdf.txt &"
    pdsh -w ssh:10.1.2.[27-29,47-49,67-69,87-89,107-108] "cd /home/HCMonitor;rm conn* &"
}

sum_mo()
{
  fil="/home/HCMonitor/cdf.txt"
  ip=(27 47 67 87 107)
  while true
  do
    conn_total=0
    id=0
    for line in `cat $filename`
    do  
        ip1=${line##*.}
        mkdir -p mon_dir/con
        echo "server BX $ip1"
        ssh root@$line 'bash -s' $fil $ip1 < extr_conn.sh &
        sleep 1
        scp -r root@$line:/home/HCMonitor/conn_$ip1 mon_dir/con/
        file=mon_dir/con/conn_$ip1
        i=0
        for line in `cat $file`
        do  
            eval BX$ip1[$i]=$line
            i=$(($i+1))
        done
        id=$(($id+1))
        if [ $id -eq $nodes ]
        then
            break;
        fi  
    done
    st_file=mon_dir/con/conn_27
    i=0
    for line in `cat $st_file`
    do
        for j in ${ip[*]}
        do
          sid=$(($j/20-1))
          k=$(($j+1))
          file=mon_dir/con/conn_$j
          #echo "open $file"
          eval va1=\${BX$j[$i]}
          eval va2=\${BX$k[$i]}
          #echo "BX$j[$i]:$va1"
          #echo "BX$k[$i]:$va2"
          tmp=$((va1+va2))
          echo "$i min: SW$sid Conn:$tmp"
          if [ $tmp -lt 60000000 ]
          then
            fg=1
            break;
          fi
          if [ $tmp -gt 62000000 ]
          then
            fg=1
            break;
          fi
          conn_sw=$((conn_sw+tmp))
        done
    i=$(($i+1))
    if [ $fg -eq 0 ]
    then
        echo "CONN_TOTAL:$conn_sw" 
        echo "$conn_sw">conn_total
    fi
    conn_sw=0
    fg=0
    #conn_total=$((conn_total+conn_sw))
    #echo "conn_total:$conn_total"
    done
  sleep 30
  done
}

delay_mo()
{
  fil="/home/HCMonitor/cdf.txt"
  u=(0.05 0.15 0.25 0.35 0.45 0.55 0.65 0.75 0.85 0.98)
  y=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 0.99)
  while true
  do
    id=0
    sum=0
    for line in `cat $filename`
    do  
        ip1=${line##*.}
        mkdir -p mon_dir/delay
        #echo "server BX $ip1"
        scp -r root@$line:/home/HCMonitor/sketch.txt mon_dir/delay/sketch_$ip1 1>scp_log
        sleep 1
        file=mon_dir/delay/sketch_$ip1
        i=0
        for num in `cat $file`
        do  
            eval CNT$ip1[$i]=$num
            #eval tmp=\${CNT$ip1[$i]}
            i=$(($i+1))
            sum=$((sum+num))
        done
        id=$(($id+1))
        if [ $id -eq $nodes ]
        then
            break;
        fi  
    done
    #echo "rsp_total:$sum"
    tal=0
    uid=0
    id=0
    true >sketch
    for j in {0..259}
    do
        count[$j]=0
        for line in `cat $filename`
        do
          ip1=${line##*.}
          eval va1=\${CNT$ip1[$j]}
          tmp=${count[$j]}
          count[$j]=$((tmp+va1))
          id=$(($id+1))
          if [ $id -eq $nodes ]
          then
            break;
          fi  
        done
        echo "${count[$j]}">>sketch
#       up=${u[$uid]}
#        if [ $uid -eq 0 ];then
#            cnt=${count[$j]}
#            tal=$((tal+cnt))
#            per=`echo "scale=2; $tal/$sum" | bc`
#            up=0.98
#            if [ `echo "$per > $up"|bc` -eq 1 ];then
#                t=$(echo "0.01*($j+1)" | bc)
#                uid=$(($uid+1))
#        fi
#        if [ $uid -eq 10 ]
#        then
#            true >cdf.txt
#            for k in {0..9}
#            do
#                echo "${y[$k]}:${t[$k]}">>cdf.txt
#                echo "${y[$k]}:${t[$k]}"
#            done
#            echo "$t">d99.txt
#            break;
#            fi  
#        fi
    done
  sleep 20
  done
}

if [ "$cmd" == "help" ]; then
    echo "./deploy.sh <command> <number of nodes> <hosts file>"
    echo "command:"
    echo "start     : start all the monitor nodes"
    echo "stop      : stop  all the monitor nodes" 
elif [ "$cmd" == "start" ]; then
    start_mo
elif [ "$cmd" == "stop" ]; then
    stop_mo
elif [ "$cmd" == "sum" ]; then
    sum_mo
elif [ "$cmd" == "delay" ]; then
    delay_mo
else
    echo "Warning, Unknown option."
fi
