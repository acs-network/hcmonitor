yum install -y mysql-devel
yum install -y numactl-devel
yum install -y python-devel

#wget https://hyperrealm.github.io/libconfig/dist/libconfig-1.7.2.tar.gz
mkdir -p deps

cd downloads

cp libconfig-1.7.2.tar.gz ../deps

cp dpdk-19.11.4.tar.xz ../deps

cd ../deps

tar -xvf libconfig-1.7.2.tar.gz

cd libconfig-1.7.2

./configure 

make && make check && make install

cp /usr/local/lib/libconfig* /usr/lib

ldconfig -v

cd ../

tar -xvf dpdk-19.11.4.tar.xz

cd dpdk-stable-19.11.4/usertools/

./dpdk-setup.sh


