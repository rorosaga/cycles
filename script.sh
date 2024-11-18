export CYCLES_PORT=50017

export LIBGL_ALWAYS_SOFTWARE=true

cat<<EOF> config.yaml
gameHeight: 800
gameWidth: 1000
gameBannerHeight: 100
gridHeight: 80
gridWidth: 100
maxClients: 60
enablePostProcessing: false
EOF

./build/bin/server &
sleep 1

for i in {1..10}
do
./build/bin/client randomio$i &
done