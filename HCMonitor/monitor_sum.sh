fil=$1
node=$2
grep -nr "conn" $fil | awk -F: '{print $3}' | awk -F, '{print $1}' >conn_$node

