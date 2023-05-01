cd /root
$HOME/premake5 --os=linux gmake2

cd $HOME/projects/linux/gmake2

make clean && make build=release
