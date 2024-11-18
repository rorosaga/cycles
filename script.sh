export CYCLES_PORT=50014

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

# for i in {1..1}
# do
# ./build/bin/client randomio$i &
# done

# for i in {1..1}
# do
# ./build/bin/clientramcav ramcav$i &
# done

for i in {1..1}
do
./build/bin/clientrorosaga rorosag$i &
done 

./build/bin/clienttest test &
