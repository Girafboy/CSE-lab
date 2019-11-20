#!/usr/bin/env bash
make
g++ -L. -L/usr/local/lib selftest.c -lpthread -lfuse -lrt -ldl -o selftest

YFSDIR1=$PWD/yfs1
YFSDIR2=$PWD/yfs2
YFSDIR3=$PWD/yfs3
YFSDIR4=$PWD/yfs4
YFSDIR5=$PWD/yfs5

export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -u";
fi
$UMOUNT $YFSDIR1
$UMOUNT $YFSDIR2
$UMOUNT $YFSDIR3
$UMOUNT $YFSDIR4
$UMOUNT $YFSDIR5
killall extent_server
killall yfs_client
killall lock_server

score=0

mkdir yfs1 >/dev/null 2>&1
mkdir yfs2 >/dev/null 2>&1
mkdir yfs3 >/dev/null 2>&1
mkdir yfs4 >/dev/null 2>&1
mkdir yfs5 >/dev/null 2>&1

ulimit -c unlimited

LOSSY=$1
NUM_LS=$2

if [ -z $NUM_LS ]; then
    NUM_LS=0
fi

BASE_PORT=$RANDOM
BASE_PORT=$[BASE_PORT+2000]
EXTENT_PORT=$BASE_PORT
YFS1_PORT=$[BASE_PORT+2]
YFS2_PORT=$[BASE_PORT+4]
YFS3_PORT=$[BASE_PORT+6]
YFS4_PORT=$[BASE_PORT+8]
YFS5_PORT=$[BASE_PORT+10]
LOCK_PORT=$[BASE_PORT+12]


if [ "$LOSSY" ]; then
    export RPC_LOSSY=$LOSSY
fi

if [ $NUM_LS -gt 1 ]; then
    x=0
    rm config
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo $port >> config
    done
    x=0
    while [ $x -lt $NUM_LS ]; do
      port=$[LOCK_PORT+2*x]
      x=$[x+1]
      echo "starting ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &"
      ./lock_server $LOCK_PORT $port > lock_server$x.log 2>&1 &
      sleep 1
    done
else
    echo "starting ./lock_server $LOCK_PORT > lock_server.log 2>&1 &"
    ./lock_server $LOCK_PORT > lock_server.log 2>&1 &
    sleep 1
fi

unset RPC_LOSSY

echo "starting ./extent_server $EXTENT_PORT > extent_server.log 2>&1 &"
./extent_server $EXTENT_PORT > extent_server.log 2>&1 &
sleep 1

rm -rf $YFSDIR1
mkdir $YFSDIR1 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR1 $EXTENT_PORT $LOCK_PORT > yfs_client1.log 2>&1 &"
./yfs_client $YFSDIR1 $EXTENT_PORT $LOCK_PORT > yfs_client1.log 2>&1 &
sleep 1

rm -rf $YFSDIR2
mkdir $YFSDIR2 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR2 $EXTENT_PORT $LOCK_PORT > yfs_client2.log 2>&1 &"
./yfs_client $YFSDIR2 $EXTENT_PORT $LOCK_PORT > yfs_client2.log 2>&1 &
sleep 1

rm -rf $YFSDIR3
mkdir $YFSDIR3 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR3 $EXTENT_PORT $LOCK_PORT > yfs_client3.log 2>&1 &"
./yfs_client $YFSDIR3 $EXTENT_PORT $LOCK_PORT > yfs_client3.log 2>&1 &
sleep 1

rm -rf $YFSDIR4
mkdir $YFSDIR4 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR4 $EXTENT_PORT $LOCK_PORT > yfs_client4.log 2>&1 &"
./yfs_client $YFSDIR4 $EXTENT_PORT $LOCK_PORT > yfs_client4.log 2>&1 &
sleep 1

rm -rf $YFSDIR5
mkdir $YFSDIR5 || exit 1
sleep 1
echo "starting ./yfs_client $YFSDIR5 $EXTENT_PORT $LOCK_PORT > yfs_client5.log 2>&1 &"
./yfs_client $YFSDIR5 $EXTENT_PORT $LOCK_PORT > yfs_client5.log 2>&1 &

sleep 2

# make sure FUSE is mounted where we expect
pwd=`pwd -P`
if [ `mount | grep "$pwd/yfs1" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs1"
    exit -1
fi

# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/yfs2" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs2"
    exit -1
fi

# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/yfs3" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs3"
    exit -1
fi

# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/yfs4" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs4"
    exit -1
fi

# make sure FUSE is mounted where we expect
if [ `mount | grep "$pwd/yfs5" | grep -v grep | wc -l` -ne 1 ]; then
    sh stop.sh
    echo "Failed to mount YFS properly at ./yfs5"
    exit -1
fi

test_if_has_mount(){
	mount | grep -q "yfs_client"
	if [ $? -ne 0 ];
	then
			echo "FATAL: Your YFS client has failed to mount its filesystem!"
			exit
	fi;
	yfs_count=$(ps -e | grep -o "yfs_client" | wc -l)
	extent_count=$(ps -e | grep -o "extent_server" | wc -l)

	if [ $yfs_count -ne 5 ];
	then
			echo "error: yfs_client not found (expecting 5)"
			exit
	fi;

	if [ $extent_count -ne 1 ];
	then
			echo "error: extent_server not found"
			exit
	fi;
}
test_if_has_mount

./selftest yfs1 yfs2 yfs3 yfs4 yfs5 | tee tmp.0
lcnt=$(cat tmp.0 | grep -o "OK" | wc -l)

if [ $lcnt -ne 1 ];
then
        echo "Failed selftest "
	score=$((score))
else
        #exit
		ps -e | grep -q "yfs_client"
		if [ $? -ne 0 ];
		then
				echo "FATAL: yfs_client DIED!"
				exit
		else
			score=$((score+100))
			echo "Passed selftest"
			#echo $score
		fi
fi

rm tmp.0

test_if_has_mount

export PATH=$PATH:/usr/local/bin
UMOUNT="umount"
if [ -f "/usr/local/bin/fusermount" -o -f "/usr/bin/fusermount" -o -f "/bin/fusermount" ]; then
    UMOUNT="fusermount -u";
fi
$UMOUNT $YFSDIR1
$UMOUNT $YFSDIR2
$UMOUNT $YFSDIR3
$UMOUNT $YFSDIR4
$UMOUNT $YFSDIR5
killall extent_server
killall yfs_client
killall lock_server
