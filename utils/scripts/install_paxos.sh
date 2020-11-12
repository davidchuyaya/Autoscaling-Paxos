sudo apt-get update
sudo apt-get install -y wget unzip git ca-certificates net-tools python3-pip libzmq3-dev curl apt-transport-https libprotobuf-dev protobuf-compiler software-properties-common
sudo snap install cmake --classic
sudo wget https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protobuf-all-3.13.0.zip
sudo unzip protobuf-all-3.13.0.zip -d /usr/local

sudo git clone https://github.com/davidchuyaya/Autoscaling-Paxos.git

cd Autoscaling-Paxos
sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ .
sudo cmake --build . --target all -- -j 2