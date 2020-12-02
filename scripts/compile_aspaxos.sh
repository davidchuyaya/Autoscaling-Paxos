sudo apt-get install -y unzip git ca-certificates net-tools curl apt-transport-https libpq-dev libzmq3-dev curl libprotobuf-dev protobuf-compiler software-properties-common
sudo snap install cmake --classic
sudo wget https://github.com/protocolbuffers/protobuf/releases/download/v3.13.0/protobuf-all-3.13.0.zip
sudo unzip protobuf-all-3.13.0.zip -d /usr/local
sudo cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++ . -B build
sudo cmake --build build --target all -- -j 2