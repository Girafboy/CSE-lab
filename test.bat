make

export RPC_LOSSY=0
./lock_server 3772 > lock_server.log &
sleep 1
./lock_tester 3772
kill -9 %1
echo "15/100"

sleep 1
export RPC_LOSSY=5
./lock_server 3772 > lock_server.log &
sleep 1
./lock_tester 3772
kill -9 %1
echo "30/100"

sleep 1
./start.sh
sleep 1
./test-lab-3-a ./yfs1 ./yfs2
./stop.sh
echo "35/100"

sleep 1
./start.sh
sleep 1
./test-lab-3-b ./yfs1 ./yfs2
./stop.sh
echo "45/100"

sleep 1
./start.sh 5
sleep 1
./test-lab-3-a ./yfs1 ./yfs2
./stop.sh
echo "50/100"

sleep 1
./start.sh 5
sleep 1
./test-lab-3-b ./yfs1 ./yfs2
./stop.sh
echo "60/100"

sleep 1
export RPC_COUNT=20
./start.sh
sleep 1
./test-lab-3-b ./yfs1 ./yfs2
./stop.sh
echo "100/100"